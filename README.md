# litex_mquickjs

Run Fabrice Bellard's [mquickjs](https://github.com/bellard/mquickjs)
JavaScript engine as a bare-metal firmware on a [LiteX](https://github.com/enjoy-digital/litex)
SoC, fully parsed and executed on the CPU — validated end-to-end in
`litex_sim` with a VexRiscv RISC-V soft core, no FPGA required.

```
--========= litex_mquickjs =========--
mquickjs heap:   1048576 bytes
CPU:             VexRiscv @ 1000000 Hz
running embedded script...
hello from mquickjs on litex!
[mqjs] done
```

Why mquickjs? It's a ~100 kB JavaScript engine with its own
tracing-compacting GC, no `malloc`, and no libc dependency. On a LiteX
SoC that's a pleasant fit: a 1 MiB heap handles the full ES5 standard
library, JSON, typed arrays, regex, `Math`, mandelbrot, and the
micro-benchmarks shipped upstream.

## What actually runs on the SoC

```
host side                              |   target side (VexRiscv in sim)
---------------------------------------+----------------------------------
examples/hello.js       (source)       |
   |                                   |
   v                                   |
tools/embed_script.py   (just wraps    |
   |                     bytes in a    |
   v                     C array)      |
firmware/build/user_script.h           |
   |                                   |
   v                                   |
make / riscv-gcc                       |   firmware.bin loaded to main_ram
   |                                   |          |
   v                                   |          v
firmware.bin                           |   JS_Eval(ctx, user_script, len)
                                       |    -> tokenize + parse + compile
                                       |    -> VM executes bytecode
                                       |    -> console.log -> UART
```

The default build embeds the **raw `.js` bytes** and parses them on the
SoC. The host doesn't preprocess the source — parsing, bytecode
generation, GC, VM execution all happen on VexRiscv.

Two alternate modes exist:

- **Bytecode**: host-side `mqjs -m32 -o hello.bin hello.js` compiles to
  a relocatable bytecode blob, the firmware detects it via
  `JS_IsBytecode()` and skips parsing. Useful when boot-time parse cost
  matters or you want to ship something less readable.
- **REPL**: build the firmware without `SCRIPT=`, and source arrives
  live over the UART one line at a time.

## Quick start (simulation)

Requirements: `riscv64-unknown-elf-gcc` with `rv32im/ilp32` multilib,
`verilator`, `meson`/`ninja` (for picolibc), Python 3.10+, and
[LiteX](https://github.com/enjoy-digital/litex/wiki/Installation) on
`PYTHONPATH`. See [docs/building.md](docs/building.md) for exact
package names on Ubuntu.

```sh
git clone --recursive https://github.com/enjoy-digital/litex_mquickjs
cd litex_mquickjs

# 1. Generate the simulated SoC (VexRiscv, 16 MiB main_ram, timer-uptime).
./sim/gen_soc.py

# 2. Build & run the hello example end-to-end.
./sim/run_sim.py --script examples/hello.js
```

The first run takes a couple of minutes while Verilator compiles the
`Vsim` binary. After that `run_sim.py` just swaps `sim_main_ram.init`
and re-launches the cached simulator — seconds per run.

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
| `examples/leds.js`       | `litex.setLeds()` / `.getSwitches()` bindings   |
| `examples/mandelbrot.js` | soft-float through `libm`/`dtoa` + nested loops |

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
                            (trimmed mqjs_stdlib.c — no load/setTimeout,
                             adds a small `litex` hardware object)
    linker.ld               everything lands in main_ram
    Makefile                requires BUILD_DIRECTORY=<sim output>
    third_party/mquickjs    git submodule, pinned commit
examples/                   .js scripts ready to run
sim/
    gen_soc.py              one-time SoC generation (VexRiscv)
    run_sim.py              build + fast re-run harness
test/                       pytest integration tests (drive the sim)
tools/embed_script.py       .js / .bin -> C byte-array header
docs/                       longer-form documentation
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
var t = litex.millis();                // uptime in ms
litex.delay(10);                       // busy-wait
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

| Variable (in `firmware/mqjs_config.h`) | Default | Notes                                                    |
|----------------------------------------|---------|----------------------------------------------------------|
| `LITEX_MQJS_HEAP_SIZE`                 | 1 MiB   | Sized for mandelbrot + REPL + user script; 256 KiB works for the smaller demos |
| `LITEX_MQJS_LINE_MAX`                  | 1024    | REPL line buffer                                         |

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
- No filesystem, no event loop — no `load()`, no
  `setTimeout`/`clearTimeout`. Adding either is a matter of hooking
  libfatfs (for the former) or a cooperative scheduler on the LiteX
  timer IRQ (for the latter); see
  [docs/porting.md](docs/porting.md#things-intentionally-not-ported).

## License

BSD-2-Clause for everything in this repository. The vendored mquickjs
sources retain their upstream MIT license (see
`firmware/third_party/mquickjs/LICENSE`).
