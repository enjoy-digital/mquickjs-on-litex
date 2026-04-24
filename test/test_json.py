# Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
# SPDX-License-Identifier: BSD-2-Clause

from conftest import run_script, REPO_ROOT


def test_json():
    rc, out = run_script(REPO_ROOT / "examples" / "json.js", timeout=240)
    assert rc == 0, f"sim failed (rc={rc}):\n{out}"
    assert "name = litex" in out
    assert "VEXRISCV, NAXRISCV, NEORV32" in out
    assert "sum = 36" in out
    assert '"sum":36' in out and '"year":2026' in out
    assert '[1,3,6,10,15]' in out
