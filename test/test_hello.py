# Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
# SPDX-License-Identifier: BSD-2-Clause

from conftest import run_script, REPO_ROOT


def test_hello():
    rc, out = run_script(REPO_ROOT / "examples" / "hello.js", timeout=240)
    assert rc == 0, f"sim failed (rc={rc}):\n{out}"
    assert "hello from mquickjs on litex!" in out, out
