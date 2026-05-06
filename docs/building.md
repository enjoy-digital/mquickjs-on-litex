<!--
Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
SPDX-License-Identifier: BSD-2-Clause
-->

# Building

## Dependencies

Ubuntu 22.04 / 24.04:

```sh
sudo apt-get install -y \
    build-essential \
    gcc-riscv64-unknown-elf \
    libevent-dev \
    libjson-c-dev \
    picolibc-riscv64-unknown-elf \
    verilator \
    meson ninja-build \
    python3 python3-pip python3-setuptools
```

`libevent-dev` and `libjson-c-dev` are needed by LiteX's native
simulator modules. Without them, recent LiteX trees can fail while
building the Verilator simulator with missing `event2/listener.h` or
`json-c/json.h`.

The `gcc-riscv64-unknown-elf` multilib **must** ship the rv32im/ilp32
variant of `libgcc.a` — that's what links the soft-float conversion
helpers (`__extendsfdf2`, `__truncdfsf2`, `__clzdi2`, …) used by
mquickjs. Verify it's there with:

```sh
riscv64-unknown-elf-gcc -march=rv32im -mabi=ilp32 -print-libgcc-file-name
# should print .../gcc/riscv64-unknown-elf/<ver>/rv32im/ilp32/libgcc.a
```

Install LiteX into your Python environment following the
[upstream instructions](https://github.com/enjoy-digital/litex/wiki/Installation),
or clone the sibling repositories into a directory on `PYTHONPATH`.

## One-shot build

```sh
git clone --recursive https://github.com/enjoy-digital/mquickjs-on-litex
cd mquickjs-on-litex
make check-env
make sim SCRIPT=examples/hello.js
```

The first run bootstraps everything:

1. `sim/run_sim.py` calls `python -m litex.tools.litex_sim` with
   `--no-compile-gateware`. This generates the SoC files,
   `variables.mak`, the linker region script, and the LiteX software
   stack needed to compile firmware.
2. It invokes `make -C firmware` with the requested script embedded.
3. It calls `litex_sim` once more to build the Verilator simulator.
   After that, `obj_dir/Vsim` is cached and reused.

The `make sim` target is only a convenience wrapper. The equivalent
manual commands are:

```sh
./sim/run_sim.py --script examples/hello.js
```

## Manual firmware build

If you want to build the firmware without running the simulator:

```sh
make -C firmware \
     BUILD_DIRECTORY=$(pwd)/build/sim \
     SCRIPT=$(pwd)/examples/hello.js
```

`BUILD_DIRECTORY` must point to a LiteX build directory containing
`software/include/generated/variables.mak`. `./sim/run_sim.py` creates
this automatically for simulation.

`SCRIPT` points at the `.js` source to embed in the firmware.

Outputs: `firmware/firmware.elf`, `firmware/firmware.bin` (raw image
loaded by the simulator or by `litex_term --kernel=…` on real
hardware).

## REPL mode

Omit `SCRIPT` and the firmware is built in line-based REPL mode:

```sh
make firmware SCRIPT=
make sim-repl
```

Each line is evaluated, the result printed. Type `.exit` to quit or
`.gc` to run the garbage collector on demand.
