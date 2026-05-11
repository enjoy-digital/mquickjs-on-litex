#!/usr/bin/env python3
"""
Generate a LiteX/VexRiscv simulator SoC suitable for mquickjs on LiteX.

Usage:
    sim/gen_soc.py [--output-dir PATH] [--ram-size BYTES] [--force]

Defaults:
    --output-dir  build/sim        (relative to repo root)
    --ram-size    0x01000000       (16 MiB)

Skips regeneration if the directory already has `software/include/generated/variables.mak`
unless --force is passed. The first run is slow (Verilator compiles the
simulator C++); subsequent runs are fast.

Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
SPDX-License-Identifier: BSD-2-Clause
"""
import argparse
import subprocess
import sys
from pathlib import Path


def main():
    repo_root = Path(__file__).resolve().parent.parent

    ap = argparse.ArgumentParser()
    ap.add_argument("--output-dir", default=str(repo_root / "build" / "sim"))
    ap.add_argument("--ram-size",   default="0x01000000",
                    help="Integrated main-RAM size in bytes (hex ok). Default 16 MiB.")
    ap.add_argument("--force", action="store_true",
                    help="Regenerate even if the output directory already looks valid.")
    args = ap.parse_args()

    out = Path(args.output_dir).resolve()
    marker = out / "software" / "include" / "generated" / "variables.mak"
    if marker.exists() and not args.force:
        print(f"[gen_soc] reusing existing SoC at {out}")
        return 0

    out.mkdir(parents=True, exist_ok=True)

    cmd = [
        sys.executable, "-m", "litex.tools.litex_sim",
        "--cpu-type=vexriscv",
        f"--integrated-main-ram-size={args.ram_size}",
        "--libc-mode=full",
        "--timer-uptime",
        f"--output-dir={out}",
        # We let LiteX compile the software stack (libc, libbase,
        # libcompiler_rt, BIOS) so the picolibc / generated headers we
        # need when cross-compiling firmware are materialised. We skip
        # only the gateware build here to keep this step fast — the
        # simulator binary is built lazily by run_sim.py on first use.
        "--no-compile-gateware",
        "--non-interactive",
    ]
    print("[gen_soc]", " ".join(cmd))
    return subprocess.call(cmd)


if __name__ == "__main__":
    sys.exit(main())
