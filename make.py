#!/usr/bin/env python3

#
# This file is part of mquickjs on LiteX.
#
# Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
# SPDX-License-Identifier: BSD-2-Clause

import os
import sys
import shutil
import argparse
import importlib
import subprocess
from pathlib import Path


DEFAULT_SCRIPT  = Path("examples/hello.js")
SIM_BUILD_DIR   = Path("build/sim")
VIDEO_BUILD_DIR = Path("build/sim-video")
BOARD_BUILD_DIR = Path("build/board")
SIM_RUNNER      = Path("sim/run_sim.py")
FIRMWARE_DIR    = Path("firmware")
FIRMWARE_BIN    = FIRMWARE_DIR / "firmware.bin"
SDCARD_LOADER   = Path("examples/sdcard/loader.js")
SDCARD_MAIN     = Path("examples/sdcard/main.js")
UART_BAUDRATE   = 115200


# Helpers ------------------------------------------------------------------------------------------

def repo_root() -> Path:
    return Path(__file__).resolve().parent


def fail(message):
    print(f"error: {message}", file=sys.stderr)
    return 1


def run(cmd, cwd=None):
    cmd = [str(c) for c in cmd]
    print("[make.py] $ " + " ".join(cmd), flush=True)
    return subprocess.run(cmd, cwd=cwd).returncode


def run_target_module(target, argv, video_framebuffer_format=None):
    cmd = [sys.executable, "-m", target] + argv
    print("[make.py] $ " + " ".join(cmd), flush=True)

    if video_framebuffer_format is None:
        return subprocess.run(cmd).returncode

    from litex.soc.integration.soc_core import SoCCore

    original_argv = sys.argv[:]
    original_add_video_framebuffer = SoCCore.add_video_framebuffer

    def add_video_framebuffer(self, *args, **kwargs):
        kwargs["format"] = video_framebuffer_format
        return original_add_video_framebuffer(self, *args, **kwargs)

    try:
        SoCCore.add_video_framebuffer = add_video_framebuffer
        sys.argv = [target] + argv
        module = importlib.import_module(target)
        try:
            rc = module.main()
        except SystemExit as e:
            rc = e.code
        if rc is None:
            return 0
        return rc if isinstance(rc, int) else 1
    finally:
        sys.argv = original_argv
        SoCCore.add_video_framebuffer = original_add_video_framebuffer


def firmware_cmd(args, script=None):
    root      = repo_root()
    build_dir = args.build_dir.resolve()
    js        = script if script is not None else getattr(args, "script", None)
    cmd       = [
        "make", "-C", str(root / FIRMWARE_DIR),
        f"BUILD_DIRECTORY={build_dir}",
        "-j",
    ]

    if js is not None:
        cmd.append(f"SCRIPT={js.resolve()}")
    if args.heap_size is not None:
        cmd.append(f"HEAP_SIZE={args.heap_size}")
    if args.memory_dump:
        cmd.append("MEMORY_DUMP=1")

    return cmd


def firmware_clean_cmd():
    root = repo_root()
    return ["make", "-C", str(root / FIRMWARE_DIR), "clean"]


def build_firmware(args, script=None):
    rc = run(firmware_clean_cmd())
    if rc:
        return rc
    return run(firmware_cmd(args, script=script))


def target_args(args):
    extra = list(getattr(args, "target_args", []))
    if extra and extra[0] == "--":
        extra = extra[1:]
    return extra


# SDCard -------------------------------------------------------------------------------------------

def check_sdcard_mount(path):
    if not path.exists():
        return fail(f"{path} does not exist")
    if not path.is_dir():
        return fail(f"{path} is not a directory")
    probe = path / ".mquickjs_write_test"
    try:
        probe.write_text("ok\n", encoding="utf-8")
        probe.unlink()
    except OSError as e:
        return fail(f"{path} is not writable ({e})")
    return 0


def copy_sdcard_file(src, dst):
    if not src.exists():
        return fail(f"{src} does not exist")
    shutil.copy2(src, dst)
    with dst.open("rb") as f:
        os.fsync(f.fileno())
    return 0


def prepare_sdcard(mount, boot, main_js):
    rc = check_sdcard_mount(mount)
    if rc:
        return rc

    boot_json = mount / "boot.json"
    if boot_json.exists():
        if boot_json.is_dir():
            return fail(f"refusing to remove directory {boot_json}")
        boot_json.unlink()
        print(f"removed {boot_json}")

    for src, dst in [
        (boot,    mount / "boot.bin"),
        (main_js, mount / "main.js"),
    ]:
        rc = copy_sdcard_file(src, dst)
        if rc:
            return rc
    os.sync()

    boot_dst = mount / "boot.bin"
    main_dst = mount / "main.js"
    if not boot_dst.exists():
        return fail(f"{boot_dst} is missing")
    if not main_dst.exists():
        return fail(f"{main_dst} is missing")
    print(f"boot.bin: {boot_dst.stat().st_size} bytes")
    print(f"main.js:  {main_dst.stat().st_size} bytes")

    return 0


# Commands -----------------------------------------------------------------------------------------

def cmd_sim(args):
    root = repo_root()
    cmd  = [
        root / SIM_RUNNER,
        "--output-dir", args.output_dir,
        "--script",     args.script,
        "--timeout",    args.timeout,
    ]
    if args.heap_size is not None:
        cmd += ["--heap-size", args.heap_size]
    if args.memory_dump:
        cmd += ["--memory-dump"]
    return run(cmd)


def cmd_sim_repl(args):
    root = repo_root()
    cmd  = [
        root / SIM_RUNNER,
        "--output-dir", args.output_dir,
        "--keep-running",
    ]
    return run(cmd)


def cmd_sim_video(args):
    root = repo_root()
    cmd  = [
        root / SIM_RUNNER,
        "--output-dir", args.output_dir,
        "--script",     args.script,
        "--timeout",    args.timeout,
        "--with-sdram",
        "--with-video-framebuffer",
    ]
    if args.with_ethernet:
        cmd += ["--with-ethernet"]
    if args.heap_size is not None:
        cmd += ["--heap-size", args.heap_size]
    if args.memory_dump:
        cmd += ["--memory-dump"]
    return run(cmd)


def cmd_firmware(args):
    return build_firmware(args)


def cmd_board_build(args):
    target_argv = [
        "--build",
        f"--cpu-type={args.cpu_type}",
        "--libc-mode=full",
        "--timer-uptime",
        f"--output-dir={args.build_dir}",
    ]
    if args.cpu_variant is not None:
        target_argv.append(f"--cpu-variant={args.cpu_variant}")
    target_argv += target_args(args)
    return run_target_module(
        target                   = args.target,
        argv                     = target_argv,
        video_framebuffer_format = args.video_framebuffer_format,
    )


def cmd_board_load(args):
    cmd = [
        sys.executable, "-m", args.target,
        "--load",
        f"--output-dir={args.build_dir}",
    ] + target_args(args)
    return run(cmd)


def cmd_board_run(args):
    root = repo_root()
    return run([
        "litex_term", args.serial,
        f"--speed={args.baudrate}",
        f"--kernel={root / FIRMWARE_BIN}",
    ])


def cmd_sdcard(args):
    root = repo_root()

    rc = build_firmware(args, script=root / SDCARD_LOADER)
    if rc:
        return rc

    return prepare_sdcard(
        mount   = args.mount,
        boot    = root / FIRMWARE_BIN,
        main_js = root / SDCARD_MAIN,
    )


def cmd_clean(args):
    return run(firmware_clean_cmd())


# Parser -------------------------------------------------------------------------------------------

def add_build_options(parser, default_build_dir):
    parser.add_argument("--build-dir",   type=Path, default=Path(default_build_dir),
                        help="LiteX build directory.")
    parser.add_argument("--heap-size",   type=int,  default=None,
                        help="Override mquickjs heap size.")
    parser.add_argument("--memory-dump", action="store_true",
                        help="Print mquickjs heap stats.")


def main():
    parser = argparse.ArgumentParser(description="Build and run mquickjs on LiteX demos.")
    subparsers = parser.add_subparsers(dest="command", required=True)

    sim = subparsers.add_parser("sim", help="Build and run a JavaScript file in litex_sim.")
    sim.add_argument("script",        nargs="?",  type=Path,  default=DEFAULT_SCRIPT,
                     help="JavaScript source.")
    sim.add_argument("--output-dir",  type=Path,  default=SIM_BUILD_DIR,
                     help="Simulator output directory.")
    sim.add_argument("--timeout",     type=float, default=120.0,
                     help="Seconds to wait for DONE marker.")
    sim.add_argument("--heap-size",   type=int,   default=None,
                     help="Override mquickjs heap size.")
    sim.add_argument("--memory-dump", action="store_true",
                     help="Print mquickjs heap stats.")
    sim.set_defaults(func=cmd_sim)

    sim_repl = subparsers.add_parser(
        "sim-repl",
        help="Run litex_sim and keep it alive for serial interaction.")
    sim_repl.add_argument("--output-dir", type=Path, default=SIM_BUILD_DIR,
                          help="LiteX simulator output directory.")
    sim_repl.set_defaults(func=cmd_sim_repl)

    sim_video = subparsers.add_parser(
        "sim-video",
        help="Run a JavaScript framebuffer demo in litex_sim.")
    sim_video.add_argument("script",        nargs="?",  type=Path,
                           default=Path("examples/plasma.js"),
                           help="JavaScript source.")
    sim_video.add_argument("--output-dir",  type=Path,  default=VIDEO_BUILD_DIR,
                           help="Simulator output directory.")
    sim_video.add_argument("--timeout",     type=float, default=240.0,
                           help="Seconds to wait for DONE marker.")
    sim_video.add_argument("--heap-size",   type=int,   default=None,
                           help="Override mquickjs heap size.")
    sim_video.add_argument("--memory-dump", action="store_true",
                           help="Print mquickjs heap stats.")
    sim_video.add_argument("--with-ethernet", action="store_true",
                           help="Also enable simulated Ethernet.")
    sim_video.set_defaults(func=cmd_sim_video)

    firmware = subparsers.add_parser(
        "firmware",
        help="Build firmware for an existing LiteX build directory.")
    firmware.add_argument("script", nargs="?", type=Path, default=DEFAULT_SCRIPT,
                          help="JavaScript source.")
    add_build_options(firmware, SIM_BUILD_DIR)
    firmware.set_defaults(func=cmd_firmware)

    board_build = subparsers.add_parser("board-build", help="Build a LiteX-Boards target.")
    board_build.add_argument("--target",    required=True,
                             help="LiteX-Boards target module.")
    board_build.add_argument("--build-dir", type=Path,                default=BOARD_BUILD_DIR,
                             help="Board build directory.")
    board_build.add_argument("--cpu-type",  default="vexriscv",
                             help="LiteX CPU type.")
    board_build.add_argument("--cpu-variant", default=None,
                             help="LiteX CPU variant.")
    board_build.add_argument("--video-framebuffer-format",
                             choices=["rgb888", "rgb565"], default=None,
                             help="Force framebuffer format on targets that use "
                                  "LiteX add_video_framebuffer().")
    board_build.add_argument("target_args", nargs=argparse.REMAINDER,
                             help="Extra target arguments after --.")
    board_build.set_defaults(func=cmd_board_build)

    board_load = subparsers.add_parser("board-load", help="Load a LiteX-Boards target bitstream.")
    board_load.add_argument("--target",    required=True,
                            help="LiteX-Boards target module.")
    board_load.add_argument("--build-dir", type=Path,                default=BOARD_BUILD_DIR,
                            help="Board build directory.")
    board_load.add_argument("target_args", nargs=argparse.REMAINDER,
                            help="Extra target arguments after --.")
    board_load.set_defaults(func=cmd_board_load)

    board_run = subparsers.add_parser("board-run", help="Upload firmware with litex_term.")
    board_run.add_argument("--serial",   required=True, help="Serial port used by litex_term.")
    board_run.add_argument("--baudrate", type=int, default=UART_BAUDRATE,
                           help="Serial baudrate. Must match the SoC UART baudrate.")
    board_run.set_defaults(func=cmd_board_run)

    sdcard = subparsers.add_parser("sdcard", help="Prepare a FAT SDCard for standalone boot.")
    sdcard.add_argument("--mount", required=True, type=Path, help="Mounted SDCard root.")
    add_build_options(sdcard, BOARD_BUILD_DIR)
    sdcard.set_defaults(func=cmd_sdcard)

    clean = subparsers.add_parser("clean", help="Clean firmware build outputs.")
    clean.set_defaults(func=cmd_clean)

    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
