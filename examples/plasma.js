// Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
// SPDX-License-Identifier: BSD-2-Clause
//
// Fast framebuffer smoke demo: draw one small plasma tile from JavaScript,
// then scale it through the LiteX framebuffer helper.

var fbWidth  = framebuffer.width;
var fbHeight = framebuffer.height;

function rgb(r, g, b) {
    return ((r & 0xff) << 16) | ((g & 0xff) << 8) | (b & 0xff);
}

if (fbWidth === 0 || fbHeight === 0) {
    console.log("[plasma] no framebuffer in this SoC");
    console.log("[plasma] try: ./make.py sim-video examples/plasma.js");
} else {
    var width  = Math.min(32, fbWidth);
    var height = Math.min(24, fbHeight);
    var scale  = Math.max(1, Math.min((fbWidth / width) >> 0, (fbHeight / height) >> 0, 8));
    var x0     = (fbWidth  - width  * scale) >> 1;
    var y0     = (fbHeight - height * scale) >> 1;
    var pixels = new Uint32Array(width * height);
    var sum    = 0;

    console.log("[plasma] framebuffer =", fbWidth, "x", fbHeight, "x", framebuffer.depth);
    console.log("[plasma] rendering", width, "x", height, "tile x", scale);

    for (var y = 0; y < height; y++) {
        for (var x = 0; x < width; x++) {
            var n = ((x * 17) ^ (y * 31) ^ ((x + y + 5) * 9)) & 0xff;
            var c = rgb(n * 5, 96 + n * 3, 192 - n);

            pixels[y * width + x] = c;
            sum = (sum + c) >>> 0;
        }
    }

    framebuffer.clear(0x000000);
    framebuffer.blitScale(pixels, width, height, x0, y0, scale);

    console.log("[plasma] frame checksum = 0x" + sum.toString(16));
    console.log("[plasma] done");
}
