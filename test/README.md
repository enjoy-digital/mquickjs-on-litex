# Tests

Pytest-style tests that build the firmware with a known script, run it
under `litex_sim`, and assert on the captured UART output.

```sh
# From the repo root:
pip install pytest
pytest -v test/
```

The first test run is slow because `litex_sim` has to compile its
Verilator simulator (~2 min). Subsequent runs reuse the cached
`obj_dir/Vsim` and complete in seconds each.

Set `LITEX_SIM_BUILD_DIR` to share a single simulator build directory
between invocations (defaults to `<repo>/build/sim`).

Small scripts that exist only to guard regressions live in
`test/scripts/`; user-facing demos stay in `examples/`.
