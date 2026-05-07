// Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
// SPDX-License-Identifier: BSD-2-Clause
//
// First framebuffer demo: draw a small plasma tile from JavaScript and
// blit it to the LiteX video framebuffer.

var fbWidth  = framebuffer.width;
var fbHeight = framebuffer.height;

if (fbWidth === 0 || fbHeight === 0) {
    console.log("[plasma] no framebuffer in this SoC");
    console.log("[plasma] try: ./make.py sim-video examples/plasma.js");
} else {
    var width  = Math.min(16, fbWidth);
    var height = Math.min(12, fbHeight);
    var x0     = (fbWidth  - width)  >> 1;
    var y0     = (fbHeight - height) >> 1;
    var pixels = new Uint32Array(width * height);
    var t      = 5;
    var sum    = 0;

    console.log("[plasma] framebuffer =", fbWidth, "x", fbHeight, "x", framebuffer.depth);
    console.log("[plasma] rendering", width, "x", height, "tile");

    for (var y = 0; y < height; y++) {
        for (var x = 0; x < width; x++) {
            var n  = ((x * 17) ^ (y * 31) ^ ((x + y + t) * 9)) & 0xff;
            var r  = (n * 5) & 0xff;
            var g  = (96 + n * 3) & 0xff;
            var b  = (192 - n) & 0xff;
            var c  = (r << 16) | (g << 8) | b;
            pixels[y * width + x] = c;
            sum = (sum + c) >>> 0;
        }
    }

    framebuffer.blit(pixels, width, height, x0, y0);
    console.log("[plasma] frame checksum = 0x" + sum.toString(16));
    console.log("[plasma] done");
}
