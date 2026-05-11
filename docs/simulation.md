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
./make.py sim
./make.py sim examples/demo.js
```

The first run asks `litex_sim` to generate the SoC, LiteX software
libraries and Verilator simulator. Later runs reuse `Vsim`, rebuild the
firmware with the selected JavaScript embedded, refresh
`sim_main_ram.init`, and relaunch the simulator.

Useful options:

```sh
./make.py sim --heap-size 262144
./make.py sim --memory-dump
```

`--memory-dump` prints mquickjs heap statistics at exit. For an
interactive serial session:

```sh
./make.py sim-repl
```

## Firmware Only

To build the firmware without running simulation, point it at a LiteX
build directory containing `software/include/generated/variables.mak`:

```sh
./make.py firmware examples/hello.js --build-dir build/sim
```

Outputs are `firmware/firmware.elf` and `firmware/firmware.bin`.

## Tests

```sh
pip install pytest
pytest -v test/
```

The tests call `./make.py sim`, so they exercise the same flow as a
manual user run.
