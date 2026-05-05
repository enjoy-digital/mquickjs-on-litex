# Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
# SPDX-License-Identifier: BSD-2-Clause

import os
import subprocess

from conftest import REPO_ROOT


def test_sdcard_loader_parses_and_reaches_filesystem_binding():
    cmd = [
        str(REPO_ROOT / "sim" / "run_sim.py"),
        "--script", str(REPO_ROOT / "examples" / "sdcard_button_loader.js"),
        "--timeout", "10",
    ]
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True,
                              env=os.environ.copy(), timeout=60)
        out = proc.stdout + proc.stderr
    except subprocess.TimeoutExpired as e:
        stdout = e.stdout.decode() if isinstance(e.stdout, bytes) else (e.stdout or "")
        stderr = e.stderr.decode() if isinstance(e.stderr, bytes) else (e.stderr or "")
        out = stdout + stderr

    assert "[sd] auto-loading main.js from SDCard" in out, out
    assert "[sd] loading main.js run 1 (boot)" in out, out
    assert "SDCard support is not present in this SoC" in out, out
    assert "[mtag]" not in out, out
    assert "unexpected character in expression" not in out, out
