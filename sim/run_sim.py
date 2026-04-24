#!/usr/bin/env python3
"""
Build firmware and run it inside litex_sim (VexRiscv, Verilator).

Usage:
    sim/run_sim.py [--script PATH] [--timeout SEC] [--output-dir PATH]
                   [--expect STRING] [--keep-running]

Design note — fast iteration:

    LiteX's default flow regenerates and rebuilds the Verilator simulator
    on every run (build_sim.sh starts with `rm -rf obj_dir/`). For a CI
    test harness that re-runs dozens of times that's prohibitive.

    This script therefore takes a two-phase approach:
      1. On first use, run `sim/gen_soc.py` plus one full `litex_sim`
         invocation to produce `obj_dir/Vsim` (several minutes).
      2. On subsequent runs, just convert the new firmware.bin into the
         `sim_main_ram.init` memory-image file that Vsim reads at reset
         and re-launch the existing Vsim binary directly (seconds).

By default the script waits until either:
    * the literal marker `[mqjs] done`  is seen on UART (exit 0)
    * the literal marker `[mqjs] fail`  is seen on UART (exit 1)
    * --timeout seconds elapse                          (exit 2)

Use --expect to add an extra pattern that must also appear for success.
--keep-running leaves the simulator attached to stdin/stdout for manual
interactive use (REPL mode).

Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
SPDX-License-Identifier: BSD-2-Clause
"""
import argparse
import os
import shutil
import signal
import subprocess
import sys
import time
from pathlib import Path


DONE_MARKER = "[mqjs] done"
FAIL_MARKER = "[mqjs] fail"


def run(cmd, cwd=None, env=None, check=True):
    print(f"[run_sim] $ {' '.join(str(c) for c in cmd)}")
    r = subprocess.run(cmd, cwd=cwd, env=env)
    if check and r.returncode != 0:
        sys.exit(r.returncode)
    return r.returncode


def build_firmware(repo_root: Path, build_dir: Path, script: Path | None,
                   heap_size: int | None = None) -> Path:
    fw_dir = repo_root / "firmware"
    cmd = ["make", "-C", str(fw_dir), f"BUILD_DIRECTORY={build_dir}", "-j"]
    if script is not None:
        cmd.append(f"SCRIPT={script.resolve()}")
    if heap_size is not None:
        cmd.append(f"HEAP_SIZE={heap_size}")
    run(cmd)
    fw_bin = fw_dir / "firmware.bin"
    if not fw_bin.exists():
        sys.exit("[run_sim] firmware.bin not found after build")
    return fw_bin


def write_mem_init(fw_bin: Path, init_file: Path):
    """Convert firmware.bin to the text format Vsim expects: one 32-bit
    little-endian word per line, as 8 hex chars."""
    data = fw_bin.read_bytes()
    # Pad to a multiple of 4 bytes so the trailing word is well-formed.
    pad = (-len(data)) % 4
    if pad:
        data += b"\x00" * pad
    with init_file.open("w") as f:
        for i in range(0, len(data), 4):
            w = int.from_bytes(data[i:i+4], "little")
            f.write(f"{w:08x}\n")


def first_time_sim_build(repo_root: Path, build_dir: Path, fw_bin: Path,
                         ram_size: str) -> Path:
    """Run litex_sim the 'normal' way once to produce obj_dir/Vsim.
    This is slow (~2 min on first build). Returns the Vsim path."""
    cmd = [
        sys.executable, "-m", "litex.tools.litex_sim",
        "--cpu-type=vexriscv",
        f"--integrated-main-ram-size={ram_size}",
        "--libc-mode=full",
        "--timer-uptime",
        f"--output-dir={build_dir}",
        f"--ram-init={fw_bin}",
        "--non-interactive",
    ]
    print("[run_sim] first-time simulator build — this takes a couple of minutes…")
    # Run via subprocess with Popen so we can stream output; litex_sim
    # will exit on its own once the BIOS hits the end or the firmware
    # halts (we don't wait for that — we only care that obj_dir/Vsim
    # was produced).
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, bufsize=1)
    vsim = build_dir / "gateware" / "obj_dir" / "Vsim"
    start = time.time()
    deadline = start + 1800  # 30 minutes is plenty
    try:
        while True:
            line = proc.stdout.readline()
            if line:
                sys.stdout.write(line)
                sys.stdout.flush()
            if vsim.exists() and time.time() - start > 5:
                # Give litex_sim a beat to finish writing the init files,
                # then tear it down — we drive Vsim ourselves from here.
                time.sleep(1.0)
                break
            if proc.poll() is not None:
                break
            if time.time() > deadline:
                print("[run_sim] first-time build timed out")
                break
    finally:
        if proc.poll() is None:
            proc.send_signal(signal.SIGTERM)
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
    if not vsim.exists():
        sys.exit("[run_sim] Vsim binary was not produced")
    return vsim


def watch(proc, timeout: float, expect: str | None, keep_running: bool) -> int:
    start = time.time()
    expect_seen = expect is None
    seen_done = False
    seen_fail = False
    try:
        while True:
            if proc.poll() is not None and proc.stdout.closed:
                break
            line = proc.stdout.readline()
            if not line:
                if proc.poll() is not None:
                    break
                time.sleep(0.01)
                continue
            sys.stdout.write(line)
            sys.stdout.flush()
            if DONE_MARKER in line:
                seen_done = True
                if expect_seen and not keep_running:
                    break
            if FAIL_MARKER in line:
                seen_fail = True
                break
            if expect and expect in line:
                expect_seen = True
                if seen_done and not keep_running:
                    break
            if time.time() - start > timeout:
                print(f"\n[run_sim] timeout after {timeout:.1f}s", file=sys.stderr)
                return 2
    finally:
        if proc.poll() is None:
            proc.send_signal(signal.SIGTERM)
            try:
                proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                proc.kill()

    if seen_fail:
        return 1
    if seen_done and expect_seen:
        return 0
    if expect and not expect_seen:
        print(f"[run_sim] expected string not seen: {expect!r}", file=sys.stderr)
        return 1
    return 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--script", type=Path, default=None,
                    help="JavaScript source (or mquickjs .bin bytecode) to embed. "
                         "Omit for REPL mode (use with --keep-running).")
    ap.add_argument("--output-dir", type=Path, default=None)
    ap.add_argument("--ram-size", default="0x01000000")
    ap.add_argument("--timeout", type=float, default=120.0,
                    help="Wall-clock seconds to wait for the DONE marker.")
    ap.add_argument("--expect", default=None,
                    help="Additional string that must appear for success.")
    ap.add_argument("--keep-running", action="store_true",
                    help="Don't stop the simulator on the DONE marker.")
    ap.add_argument("--heap-size", type=int, default=None,
                    help="Override LITEX_MQJS_HEAP_SIZE in bytes. Handy for "
                         "showing mquickjs in tight-memory mode "
                         "(try 65536 for hello, 262144 for mandelbrot).")
    args = ap.parse_args()

    repo_root = Path(__file__).resolve().parent.parent
    build_dir = (args.output_dir or (repo_root / "build" / "sim")).resolve()

    if not shutil.which("verilator"):
        sys.exit("[run_sim] verilator not found on PATH")

    # 1. Ensure SoC files (libc, libbase, headers) exist.
    marker = build_dir / "software" / "include" / "generated" / "variables.mak"
    if not marker.exists():
        run([sys.executable, str(repo_root / "sim" / "gen_soc.py"),
             f"--output-dir={build_dir}", f"--ram-size={args.ram_size}"])

    # 2. Build firmware.
    fw_bin = build_firmware(repo_root, build_dir, args.script, args.heap_size)

    # 3. Ensure Vsim exists. First time is slow; subsequent runs skip it.
    vsim = build_dir / "gateware" / "obj_dir" / "Vsim"
    if not vsim.exists():
        vsim = first_time_sim_build(repo_root, build_dir, fw_bin, args.ram_size)

    # 4. Refresh the main_ram memory image with the current firmware.
    init_file = build_dir / "gateware" / "sim_main_ram.init"
    write_mem_init(fw_bin, init_file)

    # 5. Run Vsim directly. cwd matters — it reads *.init from cwd.
    print(f"[run_sim] $ {vsim}")
    proc = subprocess.Popen(
        [str(vsim)],
        cwd=str(vsim.parent.parent),   # .../gateware
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        bufsize=1,
        text=True,
    )
    rc = watch(proc, args.timeout, args.expect, args.keep_running)
    print(f"[run_sim] exit {rc}")
    return rc


if __name__ == "__main__":
    sys.exit(main())
