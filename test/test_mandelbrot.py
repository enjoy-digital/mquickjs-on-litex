# Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
# SPDX-License-Identifier: BSD-2-Clause

from conftest import run_script, REPO_ROOT


def test_mandelbrot():
    # Exercises the soft-float path (no hardware FPU on VexRiscv rv32im).
    # At the simulator's 1 MHz clock this takes a minute or two of wall
    # time — longer than the other tests, but still well within CI
    # budget.
    rc, out = run_script(REPO_ROOT / "examples" / "mandelbrot.js", timeout=600)
    assert rc == 0, f"sim failed (rc={rc}):\n{out}"
    # The image includes a dense block of '#' characters in the middle
    # of the set. If soft-float or the loop got mangled we'd get an
    # all-spaces or all-'#' output.
    assert "#" in out, "mandelbrot set members not present"
    assert "." in out or ":" in out, "mandelbrot escape-time shades missing"
