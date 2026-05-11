# Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
# SPDX-License-Identifier: BSD-2-Clause
#
# Verify the precompiled-bytecode path: compile hello.js to .bin on the
# host with mqjs, embed the blob into the firmware, and assert the
# firmware's bytecode loader executes it correctly.

import subprocess
from pathlib import Path

import pytest

from conftest import run_script, REPO_ROOT


def _build_host_mqjs() -> Path:
    """Build the `mqjs` host interpreter from the vendored submodule.
    Returns the path to the executable. Cached across test invocations
    by the submodule's own Makefile (no-op rebuilds)."""
    mqjs_dir = REPO_ROOT / "firmware" / "third_party" / "mquickjs"
    subprocess.run(
        ["make", "-C", str(mqjs_dir), "mqjs", "-j"],
        check=True,
        capture_output=True,
    )
    return mqjs_dir / "mqjs"


def test_bytecode(tmp_path):
    mqjs = _build_host_mqjs()
    src = REPO_ROOT / "examples" / "hello.js"
    bc  = tmp_path / "hello.bin"
    # -m32 matches the 32-bit JSW of the rv32 target so bytecode is
    # word-layout-compatible with the firmware.
    subprocess.run([str(mqjs), "-m32", "-o", str(bc), str(src)], check=True)
    assert bc.exists() and bc.stat().st_size > 16

    rc, out = run_script(bc, timeout=240)
    assert rc == 0, f"sim failed (rc={rc}):\n{out}"
    assert "hello from mquickjs on litex!" in out, out
