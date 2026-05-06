<!--
Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
SPDX-License-Identifier: BSD-2-Clause
-->

# Simulation

## Two-phase flow

LiteX's default `litex_sim` flow regenerates and rebuilds the Verilator
simulator on every run — `build_sim.sh` starts with `rm -rf obj_dir/`.
For a test harness that re-runs dozens of times per CI pass that's
prohibitive, so `sim/run_sim.py` splits the work:

1. **First invocation** — run the full `litex_sim` pipeline once,
   which produces `build/sim/gateware/obj_dir/Vsim` (a compiled
   Verilator simulator, ~50 MB).
2. **Subsequent invocations** — convert `firmware.bin` into the text
   memory-image file Vsim reads at reset (`sim_main_ram.init`) and
   re-launch the cached `Vsim` directly. Seconds instead of minutes.

The harness watches UART output for the sentinel markers:

- `[mqjs] done` — firmware finished successfully (exit 0)
- `[mqjs] fail` — firmware reported an error (exit 1)
- `--timeout` elapsed — exit 2

Use `--expect STR` to also require a specific string in the output
before declaring success. Use `--keep-running` to stay attached after
`done` — useful for the REPL.

The top-level Makefile wraps the common cases:

```sh
make sim SCRIPT=examples/hello.js
make sim SCRIPT=examples/board_showcase.js
make sim-repl
```

## Custom SoC options

`sim/gen_soc.py` accepts:

- `--ram-size HEX` — size of `integrated_main_ram` (default `0x01000000` = 16 MiB)
- `--output-dir PATH` — where to drop the generated SoC (default `build/sim`)
- `--force` — regenerate even if the directory looks valid

If you need to change the CPU variant, peripherals, or other LiteX
knobs, edit `gen_soc.py` (or wrap it) — it just shells out to
`python -m litex.tools.litex_sim`. All of that tool's flags are
available.

## The `sim_main_ram.init` format

One 32-bit little-endian word per line, as 8 hex characters. The helper
in `sim/run_sim.py` does:

```python
for i in range(0, len(firmware_bin), 4):
    w = int.from_bytes(firmware_bin[i:i+4], "little")
    f.write(f"{w:08x}\n")
```

Vsim reads this file at startup via `$readmemh`. The memory base is set
by LiteX's regions (`MAIN_RAM_BASE = 0x40000000`), so the first word in
the file is placed at `main_ram[0]` — which corresponds to the
firmware's reset vector `_start`.

## Determinism

The simulated system clock is 1 MHz. That's slow for wall-clock
comparisons but identical from run to run — tests can rely on
`performance.now()` returning the same monotonic value given the same
boot sequence.

## Hooks for your own tests

`test/conftest.py` exposes `run_script(path, timeout=...)` which
handles the build + sim cycle and returns `(returncode, captured)`.
Drop a new `test_*.py` next to the existing ones and pytest picks it
up automatically.
