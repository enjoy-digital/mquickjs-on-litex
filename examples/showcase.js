// Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
// SPDX-License-Identifier: BSD-2-Clause
//
// Hardware video showcase: run the visual demos as a small playlist.
// The effects are still JavaScript; C only expands indexed tiles to the
// LiteX framebuffer.

var fbWidth  = framebuffer.width;
var fbHeight = framebuffer.height;
var sysClk   = litex.clockFrequency();
var isSim    = sysClk <= 1000000;

function tri(n) {
    n &= 255;
    return n < 128 ? n * 2 : 510 - n * 2;
}

function rgb(r, g, b) {
    return ((r & 0xff) << 16) | ((g & 0xff) << 8) | (b & 0xff);
}

function makePalette(phase) {
    var palette = new Uint32Array(256);

    for (var i = 0; i < 256; i++)
        palette[i] = rgb(tri(i + phase), tri(i * 2 + phase), tri(i + 160 + phase));

    return palette;
}

function makeFirePalette() {
    var palette = new Uint32Array(256);

    for (var i = 0; i < 256; i++) {
        var r = i * 3;
        var g = (i - 48) * 3;
        var b = (i - 160) * 3;

        if (r > 255)
            r = 255;
        if (g < 0)
            g = 0;
        if (g > 255)
            g = 255;
        if (b < 0)
            b = 0;
        if (b > 255)
            b = 255;
        palette[i] = rgb(r, g, b);
    }

    return palette;
}

function makeWaves() {
    var waves = new Uint8Array(256);

    for (var i = 0; i < 256; i++)
        waves[i] = tri(i);

    return waves;
}

function renderPlasma(indexes, width, height, frame, waves) {
    var sum = 0;
    var t0  = frame * 5;
    var t1  = frame * 3;
    var t2  = frame * 9;

    for (var y = 0; y < height; y++) {
        var row = y * width;
        var yy  = y - (height >> 1);
        var yw  = waves[(y * 7 + t1) & 255];

        for (var x = 0; x < width; x++) {
            var xx = x - (width >> 1);
            var v  = waves[(x * 8 + t0) & 255];

            v += yw;
            v += waves[(((xx * xx + yy * yy) >> 3) + t0) & 255];
            v += waves[((x + y) * 5 + t2) & 255] >> 1;
            v &= 255;

            indexes[row + x] = v;
            sum = (sum + v) >>> 0;
        }
    }

    return sum;
}

function renderFire(indexes, width, height, frame) {
    var sum = 0;

    for (var y = 0; y < height; y++) {
        var row      = y * width;
        var vertical = ((height - 1 - y) * 255 / height) >> 0;

        for (var x = 0; x < width; x++) {
            var center = x - (width >> 1);
            var wave   = ((x * 37 + frame * 11) ^ (y * 29) ^ ((x + y) * 13)) & 63;
            var taper  = (center * center) >> 1;
            var heat   = vertical + wave - taper;

            if (heat < 0)
                heat = 0;
            if (heat > 255)
                heat = 255;
            indexes[row + x] = heat;
            sum = (sum + heat) >>> 0;
        }
    }

    return sum;
}

function renderTunnel(indexes, width, height, frame) {
    var cx  = width >> 1;
    var cy  = height >> 1;
    var sum = 0;

    for (var y = 0; y < height; y++) {
        var dy  = y - cy;
        var row = y * width;

        for (var x = 0; x < width; x++) {
            var dx    = x - cx;
            var dist  = dx * dx + dy * dy + 1;
            var rings = (4096 / dist) >> 0;
            var twist = ((dx * dy) >> 1) + x * 9 - y * 7 + frame * 6;
            var index = (rings + twist) & 255;

            indexes[row + x] = index;
            sum = (sum + index) >>> 0;
        }
    }

    return sum;
}

function runEffect(name, render, palette, indexes, width, height, x0, y0, scale, frames) {
    var sum = 0;
    var started = litex.millis();

    console.log("[showcase]", name, "frames =", frames);
    for (var frame = 0; frame < frames; frame++) {
        sum = render(indexes, width, height, frame);
        framebuffer.blitIndexedScale(indexes, palette, width, height,
            x0, y0, scale, frame * 2);
        litex.delay(isSim ? 1 : 20);
    }
    console.log("[showcase]", name, "checksum = 0x" + sum.toString(16),
        "time =", litex.millis() - started, "ms");
}

if (fbWidth === 0 || fbHeight === 0) {
    console.log("[showcase] no framebuffer in this SoC");
    console.log("[showcase] build with --with-video-framebuffer");
} else {
    var width   = Math.min(isSim ? 16 : 48, fbWidth);
    var height  = Math.min(isSim ? 12 : 36, fbHeight);
    var scale   = Math.max(1, Math.min(8,
        Math.min((fbWidth / width) >> 0, (fbHeight / height) >> 0)));
    var x0      = (fbWidth  - width  * scale) >> 1;
    var y0      = (fbHeight - height * scale) >> 1;
    var indexes = new Uint8Array(width * height);
    var waves   = makeWaves();
    var frames  = isSim ? 1 : 24;

    console.log("[showcase] framebuffer =", fbWidth, "x", fbHeight, "x", framebuffer.depth);
    console.log("[showcase] clock =", sysClk, "Hz");
    console.log("[showcase] tile =", width, "x", height, "x", scale);

    framebuffer.clear(0x000000);
    runEffect("plasma", function (buf, w, h, frame) {
        return renderPlasma(buf, w, h, frame, waves);
    }, makePalette(0), indexes, width, height, x0, y0, scale, frames);

    runEffect("fire", renderFire, makeFirePalette(), indexes, width, height,
        x0, y0, scale, frames);

    runEffect("tunnel", renderTunnel, makePalette(64), indexes, width, height,
        x0, y0, scale, frames);

    console.log("[showcase] done");
}
