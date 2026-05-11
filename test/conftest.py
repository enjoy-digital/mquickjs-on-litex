# Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
# SPDX-License-Identifier: BSD-2-Clause
#
# Pytest fixtures shared by the simulation tests. Each test invokes
# sim/run_sim.py via run_script() with a specific example JS file and
# asserts on the captured UART output.

import os
import subprocess
from pathlib import Path

import pytest


REPO_ROOT = Path(__file__).resolve().parent.parent


@pytest.fixture(scope="session")
def repo_root() -> Path:
    return REPO_ROOT


def run_script(script: Path, timeout: float = 240.0, extra_args=None):
    """Build the firmware with `script` embedded and run it in litex_sim.
    Returns (returncode, captured_output)."""
    cmd = [
        str(REPO_ROOT / "sim" / "run_sim.py"),
        "--script", str(script),
        "--timeout", str(timeout),
    ]
    if extra_args:
        cmd.extend(extra_args)
    env = os.environ.copy()
    proc = subprocess.run(cmd, capture_output=True, text=True, env=env, timeout=timeout + 60)
    return proc.returncode, proc.stdout + proc.stderr
