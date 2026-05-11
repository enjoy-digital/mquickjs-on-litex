# Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
# SPDX-License-Identifier: BSD-2-Clause

from conftest import run_script, REPO_ROOT


def test_fib():
    rc, out = run_script(REPO_ROOT / "examples" / "fib.js", timeout=480)
    assert rc == 0, f"sim failed (rc={rc}):\n{out}"
    # fib(20) = 6765
    assert "fib(20) = 6765" in out, out
