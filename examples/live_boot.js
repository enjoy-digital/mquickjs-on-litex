// Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
// SPDX-License-Identifier: BSD-2-Clause
//
// Initial screen for the live JavaScript demoscene firmware.

console.log("[live] framebuffer =", framebuffer.width, "x", framebuffer.height, "x", framebuffer.depth);

if (framebuffer.width !== 0 && framebuffer.height !== 0) {
    framebuffer.clear(0x05070a);

    var w = framebuffer.width;
    var h = framebuffer.height;
    var colors = [
        0xff3355,
        0xffcc33,
        0x33dd88,
        0x33aaff,
        0xcc66ff,
    ];

    for (var i = 0; i < colors.length; i++) {
        var x = ((w / colors.length) * i) >> 0;
        var next_x = ((w / colors.length) * (i + 1)) >> 0;
        var cw = next_x - x;
        framebuffer.fillRect(x, 0, cw, h, colors[i]);
    }

    var margin = 32;
    framebuffer.fillRect(margin, margin, w - 2 * margin, h - 2 * margin, 0x05070a);
    framebuffer.fillRect(margin + 8, margin + 8, w - 2 * margin - 16, 8, 0xffffff);
    framebuffer.fillRect(margin + 8, h - margin - 16, w - 2 * margin - 16, 8, 0xffffff);
}

console.log("[live] ready for JavaScript over Ethernet");
