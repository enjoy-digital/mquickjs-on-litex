// Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
// SPDX-License-Identifier: BSD-2-Clause
//
// Initial screen for the live JavaScript demoscene firmware.

console.log("[live] framebuffer =", framebuffer.width, "x", framebuffer.height, "x", framebuffer.depth);

if (framebuffer.width !== 0 && framebuffer.height !== 0) {
    var w = framebuffer.width;
    var h = framebuffer.height;
    var cx = (w / 2) | 0;
    var cy = (h / 2) | 0;
    var url = "HTTP://192.168.1.50/";
    var board = litex.getIdentifier();

    framebuffer.clear(0x05070a);

    for (var y = 0; y < h; y += 12) {
        var c = ((y * 2) & 255) << 16 | ((255 - y) & 255) << 8 | ((y * 5) & 255);
        framebuffer.fillRect(0, y, w, 6, c);
    }

    framebuffer.fillRect(34, 34, w - 68, h - 68, 0x05070a);
    framebuffer.line(52, 64, w - 52, 64, 0xffffff);
    framebuffer.line(52, h - 64, w - 52, h - 64, 0xffffff);
    framebuffer.circle(cx - 220, cy, 54, 0x33dd88);
    framebuffer.circle(cx + 220, cy, 54, 0xffcc33);
    framebuffer.circle(cx, cy + 42, 150, 0x1f6feb);

    framebuffer.text(cx - 177, cy - 88, "MQUICKJS", 0xffffff, 4);
    framebuffer.text(cx - 192, cy - 42, "ON LITEX", 0xffc857, 4);
    framebuffer.text(cx - 180, cy + 22, "OPEN " + url, 0x33dd88, 2);
    framebuffer.text(cx - 186, cy + 48, "EDIT JS LIVE IN BROWSER", 0xd6deeb, 2);
    framebuffer.text(52, h - 48, board, 0x8b949e, 1);
}

console.log("[live] ready for JavaScript over Ethernet");
