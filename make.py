#!/usr/bin/env python3

#
# This file is part of mquickjs on LiteX.
#
# Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
# SPDX-License-Identifier: BSD-2-Clause

import sys
import argparse
import subprocess
from pathlib import Path


# Helpers ------------------------------------------------------------------------------------------

def repo_root() -> Path:
    return Path(__file__).resolve().parent


def run(cmd, cwd=None):
    cmd = [str(c) for c in cmd]
    print("[make.py] $ " + " ".join(cmd), flush=True)
    return subprocess.run(cmd, cwd=cwd).returncode


def firmware_cmd(args, script=None):
    root      = repo_root()
    build_dir = args.build_dir.resolve()
    js        = script if script is not None else getattr(args, "script", None)
    cmd       = ["make", "-C", str(root / "firmware"), f"BUILD_DIRECTORY={build_dir}", "-j"]

    if js is not None:
        cmd.append(f"SCRIPT={js.resolve()}")
    if args.heap_size is not None:
        cmd.append(f"HEAP_SIZE={args.heap_size}")
    if args.memory_dump:
        cmd.append("MEMORY_DUMP=1")

    return cmd


def target_args(args):
    extra = list(getattr(args, "target_args", []))
    if extra and extra[0] == "--":
        extra = extra[1:]
    return extra


# Commands -----------------------------------------------------------------------------------------

def cmd_sim(args):
    root = repo_root()
    cmd  = [
        root / "sim" / "run_sim.py",
        "--output-dir", args.output_dir,
        "--script",     args.script,
        "--timeout",    args.timeout,
    ]
    if args.heap_size is not None:
        cmd += ["--heap-size", args.heap_size]
    if args.memory_dump:
        cmd += ["--memory-dump"]
    return run(cmd)


def cmd_repl(args):
    root = repo_root()
    cmd  = [root / "sim" / "run_sim.py", "--output-dir", args.output_dir, "--keep-running"]
    return run(cmd)


def cmd_firmware(args):
    return run(firmware_cmd(args))


def cmd_board_build(args):
    cmd = [
        sys.executable, "-m", args.target,
        "--build",
        "--cpu-type=vexriscv",
        "--libc-mode=full",
        "--timer-uptime",
        f"--output-dir={args.build_dir}",
    ] + target_args(args)
    return run(cmd)


def cmd_board_load(args):
    cmd = [
        sys.executable, "-m", args.target,
        "--load",
        f"--output-dir={args.build_dir}",
    ] + target_args(args)
    return run(cmd)


def cmd_board_run(args):
    root = repo_root()
    return run(["litex_term", args.serial, f"--kernel={root / 'firmware' / 'firmware.bin'}"])


def cmd_sdcard(args):
    root = repo_root()

    rc = run(firmware_cmd(args, script=root / "examples" / "sdcard_loader.js"))
    if rc:
        return rc

    cmd = [
        root / "tools" / "prepare_sdcard.py",
        args.mount,
        "--clean",
        "--boot", root / "firmware" / "firmware.bin",
        "--main", root / "examples" / "sdcard" / "main.js",
    ]
    return run(cmd)


def cmd_clean(args):
    root = repo_root()
    return run(["make", "-C", str(root / "firmware"), "clean"])


# Parser -------------------------------------------------------------------------------------------

def add_build_options(parser, default_build_dir):
    parser.add_argument("--build-dir",   type=Path, default=Path(default_build_dir), help="LiteX build directory.")
    parser.add_argument("--heap-size",   type=int,  default=None,                    help="Override mquickjs heap size.")
    parser.add_argument("--memory-dump", action="store_true",                       help="Print mquickjs heap statistics.")


def main():
    parser = argparse.ArgumentParser(description="Build and run mquickjs on LiteX demos.")
    subparsers = parser.add_subparsers(dest="command", required=True)

    sim = subparsers.add_parser("sim", help="Build and run a JavaScript file in litex_sim.")
    sim.add_argument("script",       nargs="?", type=Path, default=Path("examples/hello.js"), help="JavaScript source.")
    sim.add_argument("--output-dir", type=Path, default=Path("build/sim"),                    help="LiteX simulator output directory.")
    sim.add_argument("--timeout",    type=float, default=120.0,                               help="Seconds to wait for DONE marker.")
    sim.add_argument("--heap-size",  type=int,   default=None,                                help="Override mquickjs heap size.")
    sim.add_argument("--memory-dump", action="store_true",                                   help="Print mquickjs heap statistics.")
    sim.set_defaults(func=cmd_sim)

    repl = subparsers.add_parser("repl", help="Run litex_sim and keep it alive for serial interaction.")
    repl.add_argument("--output-dir", type=Path, default=Path("build/sim"), help="LiteX simulator output directory.")
    repl.set_defaults(func=cmd_repl)

    firmware = subparsers.add_parser("firmware", help="Build firmware for an existing LiteX build directory.")
    firmware.add_argument("script", nargs="?", type=Path, default=Path("examples/hello.js"), help="JavaScript source.")
    add_build_options(firmware, "build/sim")
    firmware.set_defaults(func=cmd_firmware)

    board_build = subparsers.add_parser("board-build", help="Build a LiteX-Boards target.")
    board_build.add_argument("--target",    required=True, help="LiteX-Boards target module.")
    board_build.add_argument("--build-dir", type=Path, default=Path("build/board"), help="Board build directory.")
    board_build.add_argument("target_args", nargs=argparse.REMAINDER, help="Extra target arguments after --.")
    board_build.set_defaults(func=cmd_board_build)

    board_load = subparsers.add_parser("board-load", help="Load a LiteX-Boards target bitstream.")
    board_load.add_argument("--target",    required=True, help="LiteX-Boards target module.")
    board_load.add_argument("--build-dir", type=Path, default=Path("build/board"), help="Board build directory.")
    board_load.add_argument("target_args", nargs=argparse.REMAINDER, help="Extra target arguments after --.")
    board_load.set_defaults(func=cmd_board_load)

    board_run = subparsers.add_parser("board-run", help="Upload firmware with litex_term.")
    board_run.add_argument("--serial", required=True, help="Serial port used by litex_term.")
    board_run.set_defaults(func=cmd_board_run)

    sdcard = subparsers.add_parser("sdcard", help="Prepare a FAT SDCard for standalone boot.")
    sdcard.add_argument("--mount", required=True, type=Path, help="Mounted SDCard root.")
    add_build_options(sdcard, "build/board")
    sdcard.set_defaults(func=cmd_sdcard)

    clean = subparsers.add_parser("clean", help="Clean firmware build outputs.")
    clean.set_defaults(func=cmd_clean)

    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
