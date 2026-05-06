<!--
Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
SPDX-License-Identifier: BSD-2-Clause
-->

# Tests

Pytest-style tests that build the firmware with a known script, run it
under `litex_sim`, and assert on the captured UART output.

```sh
# From the repo root:
pip install pytest
pytest -v test/
```

The first test run is slow because `litex_sim` has to compile its
Verilator simulator (~2 min). Subsequent tests reuse the cached
`obj_dir/Vsim`.

Set `LITEX_SIM_BUILD_DIR` to share a single simulator build directory
between invocations (defaults to `<repo>/build/sim`).

The tests intentionally follow the public examples so CI remains close
to what users run manually.
