// Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
// SPDX-License-Identifier: BSD-2-Clause
//
// Lightweight mandelbrot: smaller than the one that ships with
// mquickjs so it finishes quickly on a 1 MHz simulated VexRiscv.
// Produces ASCII output; useful as a floating-point smoke test.
var W = 40, H = 16, MAX = 32;
var out = "";
for (var y = 0; y < H; y++) {
    var ci = (y - H / 2) * (3.0 / H);
    var row = "";
    for (var x = 0; x < W; x++) {
        var cr = (x - W * 0.75) * (3.0 / W);
        var zr = 0.0, zi = 0.0, n = 0;
        while (n < MAX && zr * zr + zi * zi < 4.0) {
            var t = zr * zr - zi * zi + cr;
            zi = 2 * zr * zi + ci;
            zr = t;
            n++;
        }
        row += n === MAX ? "#" : " .:-=+*%@"[(n >> 1) % 9];
    }
    out += row + "\n";
}
console.log(out);
