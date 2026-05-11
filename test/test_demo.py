# Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
# SPDX-License-Identifier: BSD-2-Clause

from conftest import REPO_ROOT, run_script


def test_hello():
    rc, out = run_script(REPO_ROOT / "examples" / "hello.js", timeout=240)
    assert rc == 0, f"sim failed (rc={rc}):\n{out}"
    assert "hello from mquickjs on LiteX!" in out
    assert "[mqjs] done" in out


def test_demo():
    rc, out = run_script(REPO_ROOT / "examples" / "demo.js", timeout=360)
    assert rc == 0, f"sim failed (rc={rc}):\n{out}"
    assert "[demo] binary counter" in out
    assert "[demo] done" in out
    assert "[mqjs] done" in out


def test_sdcard_loader_reports_missing_filesystem():
    rc, out = run_script(REPO_ROOT / "examples" / "sdcard" / "loader.js", timeout=240)
    assert rc == 0, f"sim failed (rc={rc}):\n{out}"
    assert "[sd] auto-loading main.js from SDCard" in out
    assert "SDCard support is not present" in out
    assert "[mqjs] done" in out
