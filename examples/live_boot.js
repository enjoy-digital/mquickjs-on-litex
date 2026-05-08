// Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
// SPDX-License-Identifier: BSD-2-Clause
//
// Initial screen for the live JavaScript demoscene firmware.

console.log("[live] framebuffer =", framebuffer.width, "x", framebuffer.height, "x", framebuffer.depth);

if (framebuffer.width !== 0 && framebuffer.height !== 0) {
    var w = framebuffer.width;
    var h = framebuffer.height;
    var url = "HTTP://192.168.1.50/";
    var board = litex.getIdentifier();

    var bg      = 0x020406;
    var panel   = 0x06080b;
    var field   = 0x080c10;
    var border  = 0x30343a;
    var divider = 0x10384d;
    var cyan    = 0x12bdf2;
    var cyanDim = 0x0b87bd;
    var text    = 0xf7f9fb;
    var soft    = 0xdce8f2;
    var muted   = 0x8fa3b2;
    var ok      = 0x8ed94f;

    var textWidth = function(s, scale) {
        return s.length * 8 * scale;
    };

    var drawPanel = function(x, y, pw, ph, title, tag) {
        framebuffer.fillRect(x, y, pw, ph, panel);
        framebuffer.line(x, y, x + pw - 1, y, border);
        framebuffer.line(x, y + ph - 1, x + pw - 1, y + ph - 1, border);
        framebuffer.line(x, y, x, y + ph - 1, border);
        framebuffer.line(x + pw - 1, y, x + pw - 1, y + ph - 1, border);
        framebuffer.line(x + 1, y + 28, x + pw - 2, y + 28, divider);
        framebuffer.text(x + 10, y + 10, title, cyan, 1);
        if (tag.length !== 0) {
            framebuffer.text(x + pw - textWidth(tag, 1) - 10, y + 10, tag, muted, 1);
        }
    };

    framebuffer.clear(bg);

    if (w < 560 || h < 420) {
        framebuffer.text(8, 10, "MQUICKJS ON LITEX", text, 1);
        framebuffer.text(8, 30, "OPEN " + url, ok, 1);
        framebuffer.text(8, 50, "BROWSER EDITOR", cyan, 1);
    } else {
        var m = 24;
        var gap = 16;
        var leftW = (((w - 2 * m - gap) * 3) / 5) | 0;
        var rightX = m + leftW + gap;
        var rightW = w - rightX - m;
        var topY = 92;
        var panelH = h - 228;
        var bottomY = h - 88;

        framebuffer.line(m, 70, w - m, 70, cyanDim);
        framebuffer.text(m, 26, "MQUICKJS", text, 3);
        framebuffer.text(m + textWidth("MQUICKJS", 3) + 18, 26, "ON LITEX", cyan, 3);
        framebuffer.text(m + 2, 62, "BROWSER EDITOR > MQUICKJS > FRAMEBUFFER", cyan, 1);

        framebuffer.fillRect(w - m - 96, 33, 96, 26, field);
        framebuffer.line(w - m - 96, 33, w - m - 1, 33, cyanDim);
        framebuffer.line(w - m - 96, 58, w - m - 1, 58, cyanDim);
        framebuffer.line(w - m - 96, 33, w - m - 96, 58, cyanDim);
        framebuffer.line(w - m - 1, 33, w - m - 1, 58, cyanDim);
        framebuffer.text(w - m - 76, 42, "READY", ok, 1);

        drawPanel(m, topY, leftW, panelH, "SCRIPT.JS", "MQUICKJS");
        framebuffer.fillRect(m + 12, topY + 42, leftW - 24, panelH - 54, bg);
        framebuffer.text(m + 28, topY + 54, "01", muted, 1);
        framebuffer.text(m + 58, topY + 54, "FUNCTION FRAME(T) {", cyan, 1);
        framebuffer.text(m + 28, topY + 78, "02", muted, 1);
        framebuffer.text(m + 58, topY + 78, "  DEMO.CONTROLS();", soft, 1);
        framebuffer.text(m + 28, topY + 102, "03", muted, 1);
        framebuffer.text(m + 58, topY + 102, "  C = DEMO.HUE(T);", soft, 1);
        framebuffer.text(m + 28, topY + 126, "04", muted, 1);
        framebuffer.text(m + 58, topY + 126, "  FB.FILLRECT(X,Y,12,12,C);", soft, 1);
        framebuffer.text(m + 28, topY + 150, "05", muted, 1);
        framebuffer.text(m + 58, topY + 150, "  DEMO.OVERLAY(\"LIVE\",T);", soft, 1);
        framebuffer.text(m + 28, topY + 174, "06", muted, 1);
        framebuffer.text(m + 58, topY + 174, "}", cyan, 1);

        drawPanel(rightX, topY, rightW, 112, "BOARD", "STATUS");
        framebuffer.text(rightX + 12, topY + 44, "CPU VEXRISCV", soft, 1);
        framebuffer.text(rightX + 12, topY + 66, "FB " + w + "X" + h + "X" + framebuffer.depth, muted, 1);
        framebuffer.text(rightX + 12, topY + 88, "ETH 192.168.1.50", ok, 1);

        drawPanel(rightX, topY + 128, rightW, panelH - 128, "RUNTIME", "HTTP");
        framebuffer.text(rightX + 12, topY + 172, "BROWSER EDITOR", soft, 1);
        framebuffer.text(rightX + 12, topY + 194, "RUN / STOP / SAVE", muted, 1);
        framebuffer.text(rightX + 12, topY + 216, "JS EXECUTES HERE", cyan, 1);

        drawPanel(m, bottomY, w - 2 * m, 64, "CONSOLE", "READY");
        framebuffer.text(m + 14, bottomY + 34, "OPEN " + url, ok, 1);
        framebuffer.text(m + 14, bottomY + 52, board, muted, 1);
    }
}

console.log("[live] ready for JavaScript over Ethernet");
