# litex_mquickjs

Run Fabrice Bellard's [mquickjs](https://github.com/bellard/mquickjs)
JavaScript engine as a bare-metal firmware on a [LiteX](https://github.com/enjoy-digital/litex)
SoC — validated end-to-end in `litex_sim` with a VexRiscv CPU, no FPGA
required.

```
--========= litex_mquickjs =========--
mquickjs heap:   1048576 bytes
CPU:             VexRiscv @ 1000000 Hz
running embedded script...
hello from mquickjs on litex!
[mqjs] done
```

mquickjs is a ~100 KB JavaScript engine with its own tracing-compacting
GC, no `malloc`, and no libc dependency. That makes it a good fit for
the small SoCs people build with LiteX: a 1 MiB heap is enough to run
the mandelbrot example, the micro-benchmark suite, and the tests shipped
with the engine.

## Quick start (simulation)

Requirements: `riscv64-unknown-elf-gcc` (rv32im/ilp32 multilib), `verilator`,
Python 3.10+, and [LiteX](https://github.com/enjoy-digital/litex/wiki/Installation)
on `PYTHONPATH`.

```sh
git clone --recursive https://github.com/enjoy-digital/litex_mquickjs
cd litex_mquickjs

# 1. Generate the simulated SoC (VexRiscv, 16 MiB main_ram, libc-mode=full).
./sim/gen_soc.py

# 2. Build & run the hello example end-to-end.
./sim/run_sim.py --script examples/hello.js
```

The first run is slow (~2 min) while Verilator builds the `Vsim`
binary. Subsequent runs only refresh `sim_main_ram.init` and re-launch
the cached simulator — a handful of seconds each.

## Layout

```
firmware/                bare-metal firmware
    main.c               boot + REPL / script runner
    mqjs_port.c          glue: js_print, litex.* bindings, time, CSR
    mqjs_stdlib_litex.c  host-compiled stdlib generator (trimmed mqjs_stdlib.c)
    linker.ld            everything lands in main_ram
    third_party/mquickjs submodule, pinned commit
examples/                .js scripts ready to run
sim/                     gen_soc.py, run_sim.py
test/                    pytest-driven integration tests
docs/                    longer-form documentation
.github/workflows/ci.yml Ubuntu CI: builds, boots the simulator, runs tests
```

See `docs/` for:
- [`docs/building.md`](docs/building.md)    — dependencies and how to build from source
- [`docs/simulation.md`](docs/simulation.md) — how the sim harness works and how to extend it
- [`docs/hardware.md`](docs/hardware.md)    — running on a real board (Arty A7 reference)
- [`docs/porting.md`](docs/porting.md)      — how mquickjs is wired into LiteX (internals)

## Examples

| Script                     | What it does                                      |
|----------------------------|---------------------------------------------------|
| `examples/hello.js`        | console.log — smallest sanity check               |
| `examples/fib.js`          | recursive fib(20), times it via `performance.now` |
| `examples/mandelbrot.js`   | ASCII Mandelbrot; exercises soft-float            |
| `examples/leds.js`         | LED patterns via the `litex.*` hardware binding   |

Build and run any example with:

```sh
./sim/run_sim.py --script examples/fib.js --timeout 480
```

## Host-side bytecode

mquickjs compiles JS to a relocatable bytecode you can ship instead of
source. The firmware detects the `JS_BYTECODE_MAGIC` header at runtime
and calls `JS_RelocateBytecode` + `JS_LoadBytecode`.

```sh
make -C firmware/third_party/mquickjs mqjs       # host-side mqjs
firmware/third_party/mquickjs/mqjs -m32 -o hello.bin examples/hello.js
./sim/run_sim.py --script hello.bin
```

`-m32` forces 32-bit word layout so the bytecode is binary-compatible
with the rv32 target.

## License

This repository is BSD-2-Clause. The vendored mquickjs sources retain
their upstream MIT license (see
`firmware/third_party/mquickjs/LICENSE`).
