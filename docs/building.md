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

1. `sim/gen_soc.py` calls `python -m litex.tools.litex_sim` with
   `--no-compile-gateware` — this generates the SoC Verilog, the
   `variables.mak`, the linker region script, and builds the LiteX
   software stack (`libc`, `libbase`, `libcompiler_rt`, BIOS). Takes a
   minute or so.
2. `sim/run_sim.py` first invokes `make -C firmware` with the
   requested script embedded, then calls `litex_sim` once more — on
   that pass LiteX compiles the Verilator simulator. After that,
   `obj_dir/Vsim` is cached and re-used.

The `make sim` target is only a convenience wrapper. The equivalent
manual commands are:

```sh
./sim/gen_soc.py
./sim/run_sim.py --script examples/hello.js
```

## Manual firmware build

If you want to build the firmware without running the simulator:

```sh
make -C firmware \
     BUILD_DIRECTORY=$(pwd)/build/sim \
     SCRIPT=$(pwd)/examples/hello.js
```

`BUILD_DIRECTORY` must point to a directory populated by `gen_soc.py`
(the Makefile reads `software/include/generated/variables.mak` from it).

`SCRIPT` can point at a `.js` source or at an already-compiled `.bin`
bytecode blob; the firmware detects which it is at runtime.

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
