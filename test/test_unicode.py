# Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
# SPDX-License-Identifier: BSD-2-Clause
#
# Regression guard for the NUL-sentinel fix in tools/embed_script.py.
# Before the fix, mquickjs's parser could read one byte past src_len
# and land on stray .rodata, reporting a phantom "unexpected character"
# error on the line past EOF. UTF-8 punctuation in a comment was a
# reliable trigger on some builds because it offset the blob end by
# one or two bytes and changed which stale byte the lookahead saw.

from conftest import run_script, REPO_ROOT


def test_unicode_comments_parse():
    rc, out = run_script(REPO_ROOT / "test" / "scripts" / "unicode.js", timeout=180)
    assert rc == 0, f"sim failed (rc={rc}):\n{out}"
    assert "unicode ok" in out
    assert "SyntaxError" not in out
