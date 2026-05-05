# mquickjs on LiteX

```
   __  _______       _     __      ______              __   _ __      _  __
  /  |/  / __ \__ __(_)___/ /____ / / __/ ___  ___    / /  (_) /____ | |/_/
 / /|_/ / /_/ / // / / __/  '_/ // /\ \  / _ \/ _ \  / /__/ / __/ -_)>  <
/_/  /_/\___\_\_,_/_/\__/_/\_\\___/___/  \___/_//_/ /____/_/\__/\__/_/|_|

Copyright (c) 2026 EnjoyDigital
```

Run Fabrice Bellard's [mquickjs](https://github.com/bellard/mquickjs)
JavaScript engine as a bare-metal firmware on a [LiteX](https://github.com/enjoy-digital/litex)
SoC, fully parsed and executed on the CPU — validated end-to-end in
`litex_sim` with a VexRiscv RISC-V soft core, no FPGA required.

```
--========= mquickjs on LiteX =========--
mquickjs heap:   1048576 bytes
CPU:             VexRiscv @ 1000000 Hz
running embedded script...
hello from mquickjs on LiteX!
[mqjs] done
```

Why mquickjs? It's a ~100 kB JavaScript engine with its own
tracing-compacting GC, no `malloc`, and no libc dependency. On a LiteX
SoC that's a pleasant fit: a 1 MiB heap handles the full ES5 standard
library, JSON, typed arrays, regex, `Math`, mandelbrot, and the
micro-benchmarks shipped upstream.

## What actually runs on the SoC

```
          build host                         LiteX SoC / VexRiscv
          ----------                         --------------------

 examples/hello.js  or  hello.bin
          |
          |  tools/embed_script.py
          v
 firmware/build/user_script.h
          |
          |  riscv64-unknown-elf-gcc
          v
 firmware/firmware.bin  --loaded to main_ram-->  _start()
                                                   |
                                                   v
                                           JS_NewContext(heap)
                                                   |
                                                   v
                                      JS_Eval(raw JS source)
                                      or JS_LoadBytecode(blob)
                                                   |
                                                   v
                                      mquickjs parser / compiler
                                      bytecode VM / GC / stdlib
                                                   |
                                                   v
                                    console.log -> LiteX UART
                                    litex.*     -> LiteX CSRs
```

The default build embeds the **raw `.js` bytes** and parses them on the
SoC. The host doesn't preprocess the source — parsing, bytecode
generation, GC, VM execution all happen on VexRiscv.

This demo repository was put in place with guided AI agents, with the
hardware flow validated on a real Digilent Arty A7.

Two alternate modes exist:

- **Bytecode**: host-side `tools/js2bin.sh hello.js` compiles to a
  relocatable bytecode blob, the firmware detects it via
  `JS_IsBytecode()` at runtime and skips parsing. Useful when
  boot-time parse cost matters or you want to ship something less
  readable. Then: `./sim/run_sim.py --script examples/hello.bin`.
- **REPL**: build the firmware without `SCRIPT=`, and source arrives
  live over the UART one line at a time.

## Quick start (simulation)

Requirements: `riscv64-unknown-elf-gcc` with `rv32im/ilp32` multilib,
`verilator`, `meson`/`ninja` (for picolibc), Python 3.10+, and
[LiteX](https://github.com/enjoy-digital/litex/wiki/Installation) on
`PYTHONPATH`. See [docs/building.md](docs/building.md) for exact
package names on Ubuntu.

```sh
git clone --recursive https://github.com/enjoy-digital/mquickjs-on-litex
cd mquickjs-on-litex

# Check the host tools and submodule.
make check-env

# Build & run the hello example end-to-end.
make sim SCRIPT=examples/hello.js
```

The first run takes a couple of minutes while Verilator compiles the
`Vsim` binary. After that `run_sim.py` just swaps `sim_main_ram.init`
and re-launches the cached simulator — seconds per run.

## Quick start (Digilent Arty A7)

The same firmware has been tested on a Digilent Arty A7-35T using the
board DDR as LiteX `main_ram` and the on-board FT2232 for JTAG/UART.
Build the board SoC without integrated main RAM, build one of the
examples against that SoC, load the bitstream, then upload
`firmware.bin` with `litex_term`.

```sh
make arty-gateware
make firmware SCRIPT=examples/arty_showcase.js
make arty-load
make arty-run ARTY_SERIAL=/dev/ttyUSB2
```

For the SDCard auto-boot/live-reload demo, put `boot.bin` and
`main.js` on a FAT-formatted SDCard, then:

```sh
make arty-gateware ARTY_BUILD_DIR=/tmp/arty_mqjs_sd ARTY_EXTRA="--with-sdcard --with-ethernet"
make firmware ARTY_BUILD_DIR=/tmp/arty_mqjs_sd SCRIPT=examples/sdcard_button_loader.js
make arty-sdcard-prepare ARTY_BUILD_DIR=/tmp/arty_mqjs_sd ARTY_SDCARD=/media/$USER/LITEX
make arty-load ARTY_BUILD_DIR=/tmp/arty_mqjs_sd
```

At reset, LiteX BIOS loads `boot.bin` from the SDCard, then the
firmware auto-runs `main.js`. Edit `main.js` on the card and press
BTN0 to reload it without rebuilding or uploading firmware.

Or run the same firmware manually with `litex_term`:

```sh
make arty-gateware ARTY_BUILD_DIR=/tmp/arty_mqjs_sd ARTY_EXTRA="--with-sdcard --with-ethernet"
make arty-load ARTY_BUILD_DIR=/tmp/arty_mqjs_sd
make firmware ARTY_BUILD_DIR=/tmp/arty_mqjs_sd SCRIPT=examples/sdcard_button_loader.js
litex_term /dev/ttyUSB2 --kernel=firmware/firmware.bin
```

The loader auto-runs `main.js` once at boot. Press BTN0 to reload and
execute it again from the card.

See [docs/hardware.md](docs/hardware.md) for the exact commands and
the tested hardware demos.

## Live JavaScript REPL

Omit `SCRIPT` and the firmware starts a line-based JavaScript REPL.
That is the most direct demo on hardware: type JavaScript over UART and
watch it control board peripherals.

```sh
make firmware SCRIPT=
make arty-load
make arty-run ARTY_SERIAL=/dev/ttyUSB2
```

Then enter one line at a time:

```js
litex.setLeds(0xa5)
for (var i = 0; i < 16; i++) { litex.setLeds(i); litex.delay(100); }
litex.getSwitches()
```

## Examples

```sh
./sim/run_sim.py --script examples/hello.js
./sim/run_sim.py --script examples/fib.js       --timeout 240
./sim/run_sim.py --script examples/leds.js      --timeout 120
./sim/run_sim.py --script examples/mandelbrot.js --timeout 600
```

| Script                   | What it exercises                               |
|--------------------------|-------------------------------------------------|
| `examples/hello.js`      | `console.log` — smallest sanity check           |
| `examples/fib.js`        | recursion + `performance.now()` timing          |
| `examples/json.js`       | `JSON.parse`/`stringify`, Array methods, Int32Array |
| `examples/leds.js`       | `litex.setLeds()` / `.getSwitches()` bindings   |
| `examples/arty_showcase.js` | visible Arty LED patterns + switch sampling  |
| `examples/mandelbrot.js` | soft-float through `libm`/`dtoa` + nested loops |
| `examples/unicode.js`    | UTF-8 comments (regression for the NUL-sentinel fix) |

Sample output from `fib.js`:

```
fib(20) = 6765
elapsed_ms = 28105
mqjs memory:         TAG    COUNT AVG_SIZE     SIZE    RATIO
                  object       60       14      828      45%
                 float64        1       12       12       1%
             value_array        2      328      656      36%
                  varref       42        8      336      18%
heap size=1832/1048204 stack_size=0
[mqjs] done
```

The `mqjs memory:` block is `JS_DumpMemory()` called after a final GC
pass, so the numbers reflect live state rather than the peak
watermark. `heap size=X/Y` means X bytes used of Y bytes arena.

## Tests

```sh
pip install pytest
pytest -v test/
```

Each test builds a firmware with one example embedded, runs it under
`litex_sim`, and asserts on the captured UART output via the
`[mqjs] done` / `[mqjs] fail` sentinel markers. See
[test/README.md](test/README.md).

## Repository layout

```
firmware/
    main.c                  boot + REPL / script runner
    mqjs_port.c             js_print, litex.* bindings, time, CSR
    mqjs_stdlib_litex.c     host-compiled stdlib generator
                            (trimmed mqjs_stdlib.c — no event loop,
                             adds a small `litex` hardware object)
    linker.ld               everything lands in main_ram
    Makefile                requires BUILD_DIRECTORY=<sim output>
    third_party/mquickjs    git submodule, pinned commit
examples/                   .js scripts ready to run
    README.md               example guide
sim/
    gen_soc.py              one-time SoC generation (VexRiscv)
    run_sim.py              build + fast re-run harness
test/                       pytest integration tests (drive the sim)
tools/embed_script.py       .js / .bin -> C byte-array header
tools/check_env.py          host tool sanity check
docs/                       longer-form documentation
    README.md               documentation map
    building.md             dependencies and build flow
    simulation.md           how the sim harness works
    hardware.md             running on a real board (Arty A7 reference)
    porting.md              internals: how mquickjs is wired to LiteX
.github/workflows/ci.yml    Ubuntu CI runs the full sim test matrix
```

## Hardware bindings

Scripts can poke the SoC through a small `litex` global object:

```js
litex.setLeds(0xa5);                   // write CSR_LEDS
var s = litex.getSwitches();           // read CSR_SWITCHES
var b = litex.getButtons();            // read CSR_BUTTONS
var t = litex.millis();                // uptime in ms
litex.delay(10);                       // busy-wait
var src = litex.readFile("main.js");   // FAT/SDCard file read
litex.load("main.js");                 // read + JS_Eval from SDCard
var x = litex.csrRead32(0xf0001000);   // raw CSR poke
litex.csrWrite32(0xf0001000, 0x42);
litex.reboot();                        // CSR_CTRL reset
```

Bindings that target a CSR not present in the SoC degrade gracefully —
`setLeds` is a no-op if `CSR_LEDS_BASE` is undefined, for example. So
the same firmware binary builds and runs both against the minimal
`litex_sim` and against a fully-instanced board build.

Adding your own bindings is a two-file change in `firmware/mqjs_port.c`
and `firmware/mqjs_stdlib_litex.c` — see
[docs/porting.md](docs/porting.md).

## Tunables

The JS heap is sized at compile time, but the Makefile lets you
override it without editing the header:

```sh
# Showcase mquickjs's low-memory claim: hello fits in 32 KiB.
./sim/run_sim.py --script examples/hello.js --heap-size 32768

# Default value (fine for everything including mandelbrot).
./sim/run_sim.py --script examples/mandelbrot.js  # 1 MiB heap
```

| Variable (in `firmware/mqjs_config.h`) | Default | Override with                 |
|----------------------------------------|---------|-------------------------------|
| `LITEX_MQJS_HEAP_SIZE`                 | 1 MiB   | `make HEAP_SIZE=<bytes>` / `run_sim.py --heap-size <bytes>` |
| `LITEX_MQJS_LINE_MAX`                  | 1024    | edit the header (REPL only)   |

SoC knobs are exposed by `sim/gen_soc.py`: `--ram-size`,
`--output-dir`, `--force`.

## Caveats

- The simulated VexRiscv runs at **1 MHz**. Scripts that are trivially
  fast on a real 100 MHz board take seconds here — mandelbrot is a
  couple of minutes of wall time.
- First `litex_sim` invocation builds the Verilator simulator
  (`obj_dir/Vsim`), which takes a couple of minutes. `run_sim.py`
  caches it and reuses it across subsequent runs.
- mquickjs is not standard ES. See the upstream
  [stricter-mode notes](https://github.com/bellard/mquickjs#stricter-mode):
  no array holes, only global `eval`, no value boxing, ASCII-only
  case-folding.
- No event loop — no `setTimeout`/`clearTimeout`. Adding one is a
  matter of hooking a cooperative scheduler on the LiteX timer IRQ; see
  [docs/porting.md](docs/porting.md#things-intentionally-not-ported).

## License

BSD-2-Clause for everything in this repository. The vendored mquickjs
sources retain their upstream MIT license (see
`firmware/third_party/mquickjs/LICENSE`).
