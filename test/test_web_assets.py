# Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
# SPDX-License-Identifier: BSD-2-Clause

import re
import shutil
import subprocess

import pytest

from conftest import REPO_ROOT


def extract_script(html):
    start = html.index("<script>") + len("<script>")
    end = html.index("</script>", start)
    return html[start:end]


def node_check(source, tmp_path, name):
    path = tmp_path / name

    path.write_text(source, encoding="utf-8")
    subprocess.run(["node", "--check", str(path)], check=True)


@pytest.mark.skipif(shutil.which("node") is None, reason="node is not installed")
def test_browser_assets_parse(tmp_path):
    node_check((REPO_ROOT / "tools" / "live_assets.js").read_text(encoding="utf-8"),
               tmp_path, "live_assets.js")

    for relpath in [
        "firmware/live_http_index.html",
        "tools/live_editor.html",
    ]:
        html = (REPO_ROOT / relpath).read_text(encoding="utf-8")
        node_check(extract_script(html), tmp_path,
                   relpath.replace("/", "_") + ".js")


def test_live_preset_table_is_consistent():
    source = (REPO_ROOT / "tools" / "live_assets.js").read_text(encoding="utf-8")
    presets = re.findall(r"^  ([a-z][a-z0-9_]*): `", source, re.MULTILINE)
    preset_info = re.findall(r"^  ([a-z][a-z0-9_]*): \{", source, re.MULTILINE)

    assert presets, "no live presets found"
    assert sorted(presets) == sorted(set(presets)), "duplicate live preset names"
    assert sorted(presets) == sorted(preset_info), "preset metadata does not match presets"
