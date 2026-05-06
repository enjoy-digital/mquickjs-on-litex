<!--
Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
SPDX-License-Identifier: BSD-2-Clause
-->

# Simulation

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

Install LiteX following the
[upstream instructions](https://github.com/enjoy-digital/litex/wiki/Installation).
The RISC-V toolchain must provide the rv32im/ilp32 multilib:

```sh
riscv64-unknown-elf-gcc -march=rv32im -mabi=ilp32 -print-libgcc-file-name
```

## Run

```sh
make sim
make sim SCRIPT=examples/demo.js
./sim/run_sim.py --script examples/hello.js
```

The first run asks `litex_sim` to generate the SoC, LiteX software
libraries and Verilator simulator. Later runs reuse `Vsim`, rebuild the
firmware with the selected JavaScript embedded, refresh
`sim_main_ram.init`, and relaunch the simulator.

Useful options:

```sh
make sim HEAP_SIZE=262144
make sim MEMORY_DUMP=1
./sim/run_sim.py --keep-running
```

`MEMORY_DUMP=1` prints mquickjs heap statistics at exit. `--keep-running`
is useful for the REPL:

```sh
make sim-repl
```

## Firmware Only

To build the firmware without running simulation, point it at a LiteX
build directory containing `software/include/generated/variables.mak`:

```sh
make -C firmware \
    BUILD_DIRECTORY=$(pwd)/build/sim \
    SCRIPT=$(pwd)/examples/hello.js
```

Outputs are `firmware/firmware.elf` and `firmware/firmware.bin`.

## Tests

```sh
pip install pytest
pytest -v test/
```

The tests call `sim/run_sim.py`, so they exercise the same flow as a
manual simulation run.
