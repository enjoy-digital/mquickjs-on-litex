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
from pathlib import Path


STALE_ROOT_FILES = [
    "Image",
    "boot.json",
    "boot.vfat",
    "boot_rocket_rootfs_ram0.json",
    "boot_rootfs_mmcblk0p2.json",
    "boot_rootfs_ram0.json",
    "litex_ci.txt",
    "opensbi.bin",
    "rootfs.cpio",
    "sdcard.img",
    "soc.dtb",
    "soc_combined.dtb",
]


# Helpers ------------------------------------------------------------------------------------------

def fail(message):
    print(f"error: {message}", file=sys.stderr)
    return 1


def check_mount(path):
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


def copy_file(src, dst):
    if not src.exists():
        return fail(f"{src} does not exist")
    shutil.copy2(src, dst)
    with dst.open("rb") as f:
        os.fsync(f.fileno())
    return 0


# Main ---------------------------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="Prepare a FAT SDCard for the mquickjs on LiteX demo.")
    parser.add_argument("mount",       type=Path,           help="Mounted SDCard root.")
    parser.add_argument("--boot",      type=Path,           help="Firmware to copy as boot.bin.")
    parser.add_argument("--main",      type=Path,           help="main.js to copy to the card root.")
    parser.add_argument("--clean",     action="store_true", help="Remove known stale Linux-on-LiteX boot files.")
    args = parser.parse_args()

    mount = args.mount
    rc = check_mount(mount)
    if rc:
        return rc

    if args.clean:
        for name in STALE_ROOT_FILES:
            path = mount / name
            if path.exists():
                if path.is_dir():
                    return fail(f"refusing to remove directory {path}")
                path.unlink()
                print(f"removed {path}")

    if args.boot:
        rc = copy_file(args.boot, mount / "boot.bin")
        if rc:
            return rc
    if args.main:
        rc = copy_file(args.main, mount / "main.js")
        if rc:
            return rc
    os.sync()

    boot = mount / "boot.bin"
    main_js = mount / "main.js"
    if not boot.exists():
        return fail(f"{boot} is missing")
    if not main_js.exists():
        return fail(f"{main_js} is missing")
    print(f"boot.bin: {boot.stat().st_size} bytes")
    print(f"main.js:  {main_js.stat().st_size} bytes")

    extras = [name for name in STALE_ROOT_FILES if (mount / name).exists()]
    if extras:
        print("warning: stale root files still present: " + ", ".join(extras))
        print("         rerun with --clean to remove known stale boot files")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
