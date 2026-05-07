// Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
// SPDX-License-Identifier: BSD-2-Clause
//
// Integer tunnel/moire demo. It keeps simulation fast while showing a
// different visual style from plasma and fire.

var fbWidth  = framebuffer.width;
var fbHeight = framebuffer.height;

function tri(n) {
    n &= 255;
    return n < 128 ? n * 2 : 510 - n * 2;
}

function rgb(r, g, b) {
    return ((r & 0xff) << 16) | ((g & 0xff) << 8) | (b & 0xff);
}

function makePalette() {
    var palette = new Uint32Array(256);

    for (var i = 0; i < 256; i++)
        palette[i] = rgb(tri(i + 32), tri(i * 2), tri(i + 160));

    return palette;
}

function renderTunnel(indexes, width, height) {
    var cx  = width >> 1;
    var cy  = height >> 1;
    var sum = 0;

    for (var y = 0; y < height; y++) {
        var dy  = y - cy;
        var row = y * width;

        for (var x = 0; x < width; x++) {
            var dx     = x - cx;
            var dist   = dx * dx + dy * dy + 1;
            var rings  = (4096 / dist) >> 0;
            var twist  = ((dx * dy) >> 1) + x * 9 - y * 7;
            var index  = (rings + twist) & 255;

            indexes[row + x] = index;
            sum = (sum + index) >>> 0;
        }
    }

    return sum;
}

if (fbWidth === 0 || fbHeight === 0) {
    console.log("[tunnel] no framebuffer in this SoC");
    console.log("[tunnel] try: ./make.py sim-video examples/tunnel.js");
} else {
    var width   = Math.min(32, fbWidth);
    var height  = Math.min(24, fbHeight);
    var scale   = Math.max(1, Math.min(8,
        Math.min((fbWidth / width) >> 0, (fbHeight / height) >> 0)));
    var x0      = (fbWidth  - width  * scale) >> 1;
    var y0      = (fbHeight - height * scale) >> 1;
    var indexes = new Uint8Array(width * height);
    var palette = makePalette();
    var sum;

    console.log("[tunnel] framebuffer =", fbWidth, "x", fbHeight, "x", framebuffer.depth);
    console.log("[tunnel] tile =", width, "x", height, "x", scale);

    sum = renderTunnel(indexes, width, height);

    framebuffer.clear(0x000000);
    framebuffer.blitIndexedScale(indexes, palette, width, height, x0, y0, scale);

    console.log("[tunnel] frame checksum = 0x" + sum.toString(16));
    console.log("[tunnel] done");
}
