```
              __  _______       _     __      ______              __   _ __      _  __
             /  |/  / __ \__ __(_)___/ /____ / / __/ ___  ___    / /  (_) /____ | |/_/
            / /|_/ / /_/ / // / / __/  '_/ // /\ \  / _ \/ _ \  / /__/ / __/ -_)>  <
           /_/  /_/\___\_\_,_/_/\__/_/\_\\___/___/  \___/_//_/ /____/_/\__/\__/_/|_|
                                      Copyright (c) 2026, EnjoyDigital
                                              Powered by LiteX
```

[![](https://github.com/enjoy-digital/litex_mquickjs/workflows/sim/badge.svg)](https://github.com/enjoy-digital/litex_mquickjs/actions)
![License](https://img.shields.io/badge/License-BSD%202--Clause-orange.svg)
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/enjoy-digital/litex_mquickjs)

## [> Intro

mquickjs on LiteX puts Fabrice Bellard's
[mquickjs](https://github.com/bellard/mquickjs) JavaScript engine in a
bare-metal [LiteX](https://github.com/enjoy-digital/litex) SoC.

<p align="center">
  <img src="docs/mquickjs-on-litex-intro.png" alt="mquickjs on LiteX intro" width="620">
</p>

This project is an experiment to run a compact JavaScript engine on a
small RISC-V CPU inside an FPGA. LiteX creates the SoC around the
VexRiscv CPU and provides the usual infrastructure and peripherals:
UART, timer, CSR bus, LEDs, SDCard, Ethernet, and external memory when
the board has it.

The nice part is that the JavaScript is not converted to C and it is
not a host-side trick. The CPU boots a firmware, creates a JavaScript
heap, parses the script, runs the bytecode VM, and talks to LiteX
peripherals through a small `litex` object.

It runs in `litex_sim`, should be usable on LiteX boards with enough
`main_ram`, and has been validated on a real Digilent Arty A7 with
SDCard boot.

```
--========= mquickjs on LiteX =========--
mquickjs heap:   1048576 bytes
CPU:             VexRiscv @ 100000000 Hz
[sd] auto-loading main.js from SDCard
[main.js] hello from SDCard
[main.js] identifier = LiteX SoC on Arty A7 2026-05-05 15:06:23
[main.js] scratch test = 0x51c0ffee OK
[main.js] LED scanner
[sd] done
```

Why mquickjs? It is a small JavaScript engine, roughly 100 kB, with its
own tracing-compacting GC, no `malloc`, and no libc dependency. That is
a good match for a softcore SoC: small enough to be believable, complete
enough to run ES5, JSON, typed arrays, regex, `Math`, and little demos
that blink LEDs.

This demo repository was put in place with guided AI agents, with the
hardware flow validated on a real Digilent Arty A7.

## [> What runs where?

```
  Host PC / SDCard                         FPGA / LiteX SoC
  ----------------                         ----------------

  Simple flow:

      examples/hello.js
             |
             | embedded in firmware.bin
             v
      firmware.bin  ---------------------> main_ram

  SDCard boot flow:

      boot.bin       ---------------------> LiteX BIOS loads firmware
      main.js        -- read at runtime --> litex.load("main.js")
                                                   |
                                                   v
                                      +---------------------------+
                                      | VexRiscv bare-metal app   |
                                      |---------------------------|
                                      | mquickjs heap             |
                                      | parser / bytecode VM      |
                                      | GC + small stdlib         |
                                      | litex.* hardware object   |
                                      +---------------------------+
                                           |             |
                                           v             v
                                      LiteX UART     LiteX CSRs
                                      console.log    LEDs,
                                                     scratch, SDCard
```

In the simple flow, the `.js` source is embedded in the firmware image
and parsed after boot. In the SDCard flow, LiteX BIOS boots `boot.bin`,
then the firmware reads and evaluates `main.js` from FAT. Edit
`main.js` on the card and reset the board to run the new version.
Bytecode mode is also supported when you want to precompile on the host,
and REPL mode lets you type JavaScript over the UART.

## [> Try it in simulation

Requirements are the usual LiteX simulation tools:
`riscv64-unknown-elf-gcc`, `verilator`, `meson`, `ninja`, Python, and
LiteX on `PYTHONPATH`. Exact package names are in
[docs/building.md](docs/building.md).

```sh
git clone --recursive https://github.com/enjoy-digital/mquickjs-on-litex
cd mquickjs-on-litex

make check-env
make sim SCRIPT=examples/hello.js
```

The first run takes a couple of minutes while Verilator compiles the
simulator. After that, scripts relaunch in seconds.

```sh
./sim/run_sim.py --script examples/json.js
./sim/run_sim.py --script examples/leds.js
./sim/run_sim.py --script examples/mandelbrot.js --timeout 600
```

## [> Try it on hardware

The default hardware helper uses the upstream LiteX-Boards Digilent
Arty target, but the same flow can point at another LiteX-Boards target
with `BOARD_TARGET`. On Arty, it uses board DDR as LiteX `main_ram` and
the on-board FT2232 for JTAG/UART.

```sh
make board-gateware
make firmware SCRIPT=examples/board_showcase.js
make board-load
make board-run BOARD_SERIAL=/dev/ttyUSB2
```

For the more playful demo, boot from SDCard and keep the JavaScript file
editable:

```sh
make board-gateware BOARD_BUILD_DIR=/tmp/mqjs_sd BOARD_EXTRA="--with-sdcard --with-ethernet"
make firmware BOARD_BUILD_DIR=/tmp/mqjs_sd SCRIPT=examples/sdcard_loader.js
make board-sdcard-prepare BOARD_BUILD_DIR=/tmp/mqjs_sd BOARD_SDCARD=/media/$USER/LITEX
make board-load BOARD_BUILD_DIR=/tmp/mqjs_sd
```

At reset, LiteX BIOS loads `boot.bin` from the SDCard, then the
firmware auto-runs `main.js`. Edit `main.js` on the card and reset the
board to run the new script. No rebuild, no firmware upload, just a
tiny JavaScript engine bossing around LiteX CSRs.

See [docs/hardware.md](docs/hardware.md) for manual `litex_term`
loading, SDCard preparation checks, and using other LiteX-Boards
targets.

## [> Talk to the board

JavaScript gets a small `litex` object:

```js
console.log(litex.getIdentifier());

var old = litex.getScratch();
litex.setScratch(0x51c0ffee);
console.log(litex.getScratch().toString(16));
litex.setScratch(old);

for (var i = 0; i < 16; i++) {
    litex.setLeds(i);
    litex.delay(100);
}
```

Useful bindings include LEDs, switches, buttons, the LiteX identifier,
the scratch register, uptime/delay helpers, raw CSR access, reboot, and
SDCard `readFile()` / `load()` when the SoC has SDCard support.

## [> Examples

| Script | What it shows |
|--------|---------------|
| `examples/hello.js` | smallest end-to-end sanity check |
| `examples/json.js` | JSON, arrays, typed arrays |
| `examples/fib.js` | recursion and timing |
| `examples/leds.js` | LiteX CSR bindings in simulation |
| `examples/board_showcase.js` | visible board LED demo |
| `examples/sdcard_loader.js` | SDCard `boot.bin` loader |
| `examples/sdcard/main.js` | SDCard-edited script: identifier, scratch, LEDs |
| `examples/mandelbrot.js` | soft-float, `Math`, nested loops |

## [> Details when you want them

- [docs/building.md](docs/building.md): host dependencies and build flow.
- [docs/simulation.md](docs/simulation.md): how the simulator harness works.
- [docs/hardware.md](docs/hardware.md): board bring-up, SDCard boot, other LiteX targets.
- [docs/porting.md](docs/porting.md): mquickjs/LiteX integration notes.
- [examples/README.md](examples/README.md): what each script is for.
- [test/README.md](test/README.md): pytest simulation coverage.

For CI/local regression:

```sh
pip install pytest
pytest -v test/
```

The repository is intentionally small:

```
firmware/     mquickjs port, LiteX bindings, linker script
examples/     JavaScript demos
sim/          LiteX simulation helper
tools/        embedding, bytecode, SDCard helpers
docs/         deeper explanations
test/         end-to-end simulation tests
```

## [> License

BSD-2-Clause for everything in this repository. The vendored mquickjs
sources retain their upstream MIT license, see
`firmware/third_party/mquickjs/LICENSE`.
