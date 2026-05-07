// Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
// SPDX-License-Identifier: BSD-2-Clause
//
// Indexed-color fire demo. JavaScript computes a compact heat tile; the
// C framebuffer helper only expands the final indexed pixels.

var fbWidth  = framebuffer.width;
var fbHeight = framebuffer.height;

function rgb(r, g, b) {
    return ((r & 0xff) << 16) | ((g & 0xff) << 8) | (b & 0xff);
}

function clamp(v, max) {
    return v > max ? max : v;
}

function makePalette() {
    var palette = new Uint32Array(256);

    for (var i = 0; i < 256; i++) {
        var r = clamp(i * 3, 255);
        var g = clamp((i - 48) * 3, 255);
        var b = clamp((i - 160) * 3, 255);

        if (g < 0)
            g = 0;
        if (b < 0)
            b = 0;
        palette[i] = rgb(r, g, b);
    }

    return palette;
}

function renderFire(heat, width, height) {
    var sum = 0;

    for (var y = 0; y < height; y++) {
        var row      = y * width;
        var vertical = ((height - 1 - y) * 255 / height) >> 0;

        for (var x = 0; x < width; x++) {
            var wave  = ((x * 37) ^ (y * 29) ^ ((x + y) * 13)) & 63;
            var taper = ((x - (width >> 1)) * (x - (width >> 1))) >> 1;
            var heatv = vertical + wave - taper;

            if (heatv < 0)
                heatv = 0;
            if (heatv > 255)
                heatv = 255;
            heat[row + x] = heatv;
            sum = (sum + heatv) >>> 0;
        }
    }

    return sum;
}

if (fbWidth === 0 || fbHeight === 0) {
    console.log("[fire] no framebuffer in this SoC");
    console.log("[fire] try: ./make.py sim-video examples/fire.js");
} else {
    var width   = Math.min(32, fbWidth);
    var height  = Math.min(24, fbHeight);
    var scale   = Math.max(1, Math.min(8,
        Math.min((fbWidth / width) >> 0, (fbHeight / height) >> 0)));
    var x0      = (fbWidth  - width  * scale) >> 1;
    var y0      = (fbHeight - height * scale) >> 1;
    var heat    = new Uint8Array(width * height);
    var palette = makePalette();
    var sum;

    console.log("[fire] framebuffer =", fbWidth, "x", fbHeight, "x", framebuffer.depth);
    console.log("[fire] tile =", width, "x", height, "x", scale);

    sum = renderFire(heat, width, height);

    framebuffer.clear(0x000000);
    framebuffer.blitIndexedScale(heat, palette, width, height, x0, y0, scale);

    console.log("[fire] frame checksum = 0x" + sum.toString(16));
    console.log("[fire] done");
}
