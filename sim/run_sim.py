#!/usr/bin/env python3

#
# This file is part of mquickjs on LiteX.
#
# Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
# SPDX-License-Identifier: BSD-2-Clause

import os
import sys
import time
import shutil
import signal
import json
import argparse
import subprocess
import selectors
from pathlib import Path


DONE_MARKER = "[mqjs] done"
FAIL_MARKER = "[mqjs] fail"


# Helpers ------------------------------------------------------------------------------------------

def run(cmd, cwd=None, env=None, check=True):
    print(f"[run_sim] $ {' '.join(str(c) for c in cmd)}")
    r = subprocess.run(cmd, cwd=cwd, env=env)
    if check and r.returncode != 0:
        sys.exit(r.returncode)
    return r.returncode


def check_output(cmd):
    return subprocess.check_output(cmd, text=True).strip()


def build_firmware(repo_root: Path, build_dir: Path, script: Path | None,
                   heap_size: int | None = None, memory_dump: bool = False) -> Path:
    fw_dir = repo_root / "firmware"
    run(["make", "-C", str(fw_dir), "clean"])
    cmd = ["make", "-C", str(fw_dir), f"BUILD_DIRECTORY={build_dir}", "-j"]
    if script is not None:
        cmd.append(f"SCRIPT={script.resolve()}")
    if heap_size is not None:
        cmd.append(f"HEAP_SIZE={heap_size}")
    if memory_dump:
        cmd.append("MEMORY_DUMP=1")
    run(cmd)
    fw_bin = fw_dir / "firmware.bin"
    if not fw_bin.exists():
        sys.exit("[run_sim] firmware.bin not found after build")
    return fw_bin


# Memory Init --------------------------------------------------------------------------------------

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


# Simulator Build ----------------------------------------------------------------------------------

def litex_args(args):
    cmd = [
        "--cpu-type=vexriscv",
        "--libc-mode=full",
        "--timer-uptime",
    ]
    if args.with_sdram:
        cmd.append("--with-sdram")
    else:
        cmd.append(f"--integrated-main-ram-size={args.ram_size}")
    if args.with_video_framebuffer:
        cmd.append("--with-video-framebuffer")
    if args.with_ethernet:
        cmd.append("--with-ethernet")
    return cmd


def firmware_init_arg(profile, fw_bin: Path) -> str:
    if "--with-sdram" in profile:
        return f"--sdram-init={fw_bin}"
    return f"--ram-init={fw_bin}"


def write_profile_marker(build_dir: Path, profile):
    marker = build_dir / ".mquickjs-sim-profile"
    marker.write_text("\n".join(profile) + "\n", encoding="utf-8")


def profile_matches(build_dir: Path, profile) -> bool:
    marker = build_dir / ".mquickjs-sim-profile"
    if not marker.exists():
        return False
    return marker.read_text(encoding="utf-8") == "\n".join(profile) + "\n"


def clean_stale_profile(build_dir: Path, profile):
    if profile_matches(build_dir, profile):
        return
    gateware_dir = build_dir / "gateware"
    for path in [gateware_dir / "modules", gateware_dir / "obj_dir"]:
        if path.exists():
            print(f"[run_sim] removing stale {path}")
            shutil.rmtree(path)


def sim_env(profile):
    env = os.environ.copy()
    if "--with-video-framebuffer" not in profile:
        return env

    try:
        sdl_libs = check_output(["pkg-config", "--libs", "sdl2"])
    except (subprocess.CalledProcessError, FileNotFoundError):
        try:
            sdl_libs = check_output(["sdl2-config", "--libs"])
        except (subprocess.CalledProcessError, FileNotFoundError):
            sys.exit("[run_sim] SDL2 development files not found")

    env["LDFLAGS"] = (env.get("LDFLAGS", "") + " " + sdl_libs).strip()
    if "DISPLAY" not in env and "WAYLAND_DISPLAY" not in env:
        env.setdefault("SDL_VIDEODRIVER", "dummy")
    return env


def ensure_sim_soc(build_dir: Path, profile):
    clean_stale_profile(build_dir, profile)

    marker = build_dir / "software" / "include" / "generated" / "variables.mak"
    required = [
        marker,
        build_dir / "software" / "libc" / "libc.a",
        build_dir / "software" / "libcompiler_rt" / "libcompiler_rt.a",
        build_dir / "software" / "libbase" / "libbase.a",
    ]
    if profile_matches(build_dir, profile) and all(path.exists() for path in required):
        return False

    build_dir.mkdir(parents=True, exist_ok=True)
    cmd = [
        sys.executable, "-m", "litex.tools.litex_sim",
        *profile,
        f"--output-dir={build_dir}",
        "--no-compile-gateware",
        "--non-interactive",
    ]
    run(cmd, env=sim_env(profile))
    write_profile_marker(build_dir, profile)
    return True


def first_time_sim_build(repo_root: Path, build_dir: Path, fw_bin: Path,
                         profile) -> Path:
    """Run litex_sim the 'normal' way once to produce obj_dir/Vsim.
    This is slow (~2 min on first build). Returns the Vsim path."""
    cmd = [
        sys.executable, "-m", "litex.tools.litex_sim",
        *profile,
        f"--output-dir={build_dir}",
        firmware_init_arg(profile, fw_bin),
        "--non-interactive",
    ]
    print("[run_sim] first-time simulator build - this takes a couple of minutes...")
    # Run via subprocess with Popen so we can stream output; litex_sim
    # will exit on its own once the BIOS hits the end or the firmware
    # halts (we don't wait for that: we only care that obj_dir/Vsim
    # was produced).
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, bufsize=1, env=sim_env(profile))
    vsim = build_dir / "gateware" / "obj_dir" / "Vsim"
    start = time.time()
    deadline = start + 1800  # 30 minutes is plenty
    try:
        while True:
            line = proc.stdout.readline()
            if line:
                sys.stdout.write(line)
                sys.stdout.flush()
            if simulator_ready(build_dir) and time.time() - start > 5:
                # Give litex_sim a beat to finish writing the init files,
                # then tear it down: we drive Vsim ourselves from here.
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


def refresh_firmware_init(build_dir: Path, fw_bin: Path, profile):
    if "--with-sdram" not in profile:
        init_file = main_ram_init_file(build_dir)
        write_mem_init(fw_bin, init_file)
        return

    cmd = [
        sys.executable, "-m", "litex.tools.litex_sim",
        *profile,
        f"--output-dir={build_dir}",
        firmware_init_arg(profile, fw_bin),
        "--no-compile-gateware",
        "--non-interactive",
    ]
    run(cmd, env=sim_env(profile))


def simulator_ready(build_dir: Path) -> bool:
    gateware_dir = build_dir / "gateware"
    vsim = gateware_dir / "obj_dir" / "Vsim"
    modules_dir = gateware_dir / "modules"
    if not (vsim.exists() and modules_dir.is_dir() and any(modules_dir.glob("*.so"))):
        return False
    return True


def main_ram_init_file(build_dir: Path) -> Path:
    gateware_dir = build_dir / "gateware"
    for filename in ["sim_main_ram.init", "sim_mem.init"]:
        path = gateware_dir / filename
        if path.exists():
            return path
    return gateware_dir / "sim_main_ram.init"


def cleanup_unused_modules(build_dir: Path):
    config = build_dir / "gateware" / "sim_config.js"
    modules_dir = build_dir / "gateware" / "modules"
    if not config.exists() or not modules_dir.exists():
        return

    with config.open(encoding="utf-8") as f:
        data = json.load(f)
    used = {entry["module"] for entry in data if "module" in entry}
    for module in modules_dir.glob("*.so"):
        if module.stem not in used:
            print(f"[run_sim] ignoring stale module {module.name}")
            module.unlink()


# Run / Watch --------------------------------------------------------------------------------------

def watch(proc, timeout: float, expect: str | None, keep_running: bool) -> int:
    start = time.time()
    expect_seen = expect is None
    seen_done = False
    seen_fail = False
    selector = selectors.DefaultSelector()
    selector.register(proc.stdout, selectors.EVENT_READ)
    try:
        while True:
            if proc.poll() is not None and proc.stdout.closed:
                break
            if time.time() - start > timeout:
                print(f"\n[run_sim] timeout after {timeout:.1f}s", file=sys.stderr)
                return 2
            events = selector.select(timeout=0.1)
            if not events:
                continue
            line = proc.stdout.readline()
            if not line:
                if proc.poll() is not None:
                    break
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


# Main ---------------------------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Build firmware and run it inside litex_sim.")
    parser.add_argument("--script",       type=Path, default=None,         help="JavaScript source to embed.")
    parser.add_argument("--output-dir",   type=Path, default=None,         help="LiteX simulator output directory.")
    parser.add_argument("--ram-size",     default="0x01000000",            help="Integrated main-RAM size.")
    parser.add_argument("--timeout",      type=float, default=120.0,       help="Seconds to wait for DONE marker.")
    parser.add_argument("--expect",       default=None,                    help="Additional required output string.")
    parser.add_argument("--keep-running", action="store_true",            help="Do not stop on DONE marker.")
    parser.add_argument("--heap-size",    type=int, default=None,          help="Override LITEX_MQJS_HEAP_SIZE.")
    parser.add_argument("--memory-dump",  action="store_true",
                        help="Print mquickjs heap statistics at exit.")
    parser.add_argument("--with-sdram",   action="store_true",            help="Use simulated SDRAM as main RAM.")
    parser.add_argument("--with-video-framebuffer", action="store_true",
                        help="Enable LiteX video framebuffer.")
    parser.add_argument("--with-ethernet", action="store_true",
                        help="Enable simulated Ethernet.")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parent.parent
    build_dir = (args.output_dir or (repo_root / "build" / "sim")).resolve()
    profile = litex_args(args)

    if not shutil.which("verilator"):
        sys.exit("[run_sim] verilator not found on PATH")

    # 1. Ensure SoC files (libc, libbase, headers) exist.
    soc_regenerated = ensure_sim_soc(build_dir, profile)

    # 2. Build firmware.
    fw_bin = build_firmware(repo_root, build_dir, args.script, args.heap_size, args.memory_dump)

    # 3. Ensure Vsim exists. First time is slow; subsequent runs skip it.
    vsim = build_dir / "gateware" / "obj_dir" / "Vsim"
    if soc_regenerated or not simulator_ready(build_dir):
        vsim = first_time_sim_build(repo_root, build_dir, fw_bin, profile)
    cleanup_unused_modules(build_dir)

    # 4. Refresh the main RAM/SDRAM memory image with the current firmware.
    refresh_firmware_init(build_dir, fw_bin, profile)

    # 5. Run Vsim directly. cwd matters: it reads *.init from cwd.
    print(f"[run_sim] $ {vsim}")
    proc = subprocess.Popen(
        [str(vsim)],
        cwd=str(vsim.parent.parent),   # .../gateware
        env=sim_env(profile),
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
