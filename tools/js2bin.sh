#!/bin/sh
# Compile a JavaScript source file to a mquickjs bytecode blob that the
# LiteX firmware can load directly (skipping parse on the target).
#
# Usage:
#   tools/js2bin.sh input.js [output.bin]
#
# If output is omitted, writes alongside input with a .bin suffix.
#
# Builds the host-side `mqjs` interpreter on first use; subsequent
# invocations reuse the cached binary.
#
# Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
# SPDX-License-Identifier: BSD-2-Clause
set -eu

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
    echo "usage: $0 input.js [output.bin]" >&2
    exit 2
fi

here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/.." && pwd)
mqjs_dir="$repo/firmware/third_party/mquickjs"
mqjs="$mqjs_dir/mqjs"

# Build the host interpreter if it's not there or is stale.
if [ ! -x "$mqjs" ] || [ "$mqjs_dir/mqjs.c" -nt "$mqjs" ]; then
    echo "[js2bin] building host mqjs..." >&2
    make -C "$mqjs_dir" mqjs -j >/dev/null
fi

input=$1
output=${2:-${input%.js}.bin}

# -m32 forces 32-bit word layout to match the rv32 target.
"$mqjs" -m32 -o "$output" "$input"
echo "[js2bin] $input -> $output ($(wc -c < "$output") bytes)"
