#!/usr/bin/env python3
"""Check the host tools commonly needed by litex_mquickjs demos."""

import importlib.util
import shutil
import subprocess
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent


def have_module(name: str) -> bool:
    return importlib.util.find_spec(name) is not None


def have_rv32_multilib() -> bool:
    gcc = shutil.which("riscv64-unknown-elf-gcc")
    if gcc is None:
        return False
    try:
        result = subprocess.run(
            [gcc, "-march=rv32im", "-mabi=ilp32", "-print-libgcc-file-name"],
            check=True,
            capture_output=True,
            text=True,
        )
    except (OSError, subprocess.CalledProcessError):
        return False
    return "rv32im" in result.stdout and "ilp32" in result.stdout


def main() -> int:
    checks = [
        ("python3", shutil.which("python3") is not None, "install Python 3"),
        ("make", shutil.which("make") is not None, "install build-essential"),
        ("gcc", shutil.which("gcc") is not None, "install build-essential"),
        ("riscv64-unknown-elf-gcc", shutil.which("riscv64-unknown-elf-gcc") is not None,
         "install a RISC-V bare-metal GCC toolchain"),
        ("rv32im/ilp32 libgcc", have_rv32_multilib(),
         "install a RISC-V GCC with rv32im/ilp32 multilib support"),
        ("litex Python package", have_module("litex"),
         "install LiteX or put it on PYTHONPATH"),
        ("litex_boards Python package", have_module("litex_boards"),
         "install litex-boards or put it on PYTHONPATH"),
        ("mquickjs submodule",
         (REPO_ROOT / "firmware" / "third_party" / "mquickjs" / "mquickjs.c").exists(),
         "run: git submodule update --init --recursive"),
    ]

    optional = [
        ("verilator", shutil.which("verilator") is not None,
         "needed for simulation"),
        ("litex_term", shutil.which("litex_term") is not None,
         "needed for hardware serial boot"),
        ("openFPGALoader", shutil.which("openFPGALoader") is not None,
         "recommended for Arty FT2232 bitstream loading"),
        ("vivado", shutil.which("vivado") is not None,
         "needed to build Arty gateware with the default toolchain"),
        ("pytest", have_module("pytest"),
         "needed to run the regression tests"),
    ]

    failed = False
    print("Required:")
    for name, ok, hint in checks:
        print(f"  {'OK  ' if ok else 'MISS'} {name}")
        if not ok:
            print(f"       {hint}")
            failed = True

    print("\nOptional:")
    for name, ok, hint in optional:
        print(f"  {'OK  ' if ok else 'MISS'} {name}")
        if not ok:
            print(f"       {hint}")

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
