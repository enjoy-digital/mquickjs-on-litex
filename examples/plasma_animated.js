// Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
// SPDX-License-Identifier: BSD-2-Clause
//
// Hardware-oriented animated plasma. JavaScript owns the effect and fills
// an indexed color tile; the framebuffer helper only expands it to RGB.

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
        palette[i] = rgb(tri(i), tri(i + 85), tri(i + 170));

    return palette;
}

function makeWaves() {
    var waves = new Uint8Array(256);

    for (var i = 0; i < 256; i++)
        waves[i] = tri(i);

    return waves;
}

function makePhases(width, height) {
    var phases = {
        x:      new Uint8Array(width),
        y:      new Uint8Array(height),
        radial: new Uint8Array(width * height),
        diag:   new Uint8Array(width * height),
    };

    for (var x = 0; x < width; x++)
        phases.x[x] = (x * 8) & 255;

    for (var y = 0; y < height; y++)
        phases.y[y] = (y * 7) & 255;

    for (var y = 0; y < height; y++) {
        var yy = y - (height >> 1);

        for (var x = 0; x < width; x++) {
            var xx = x - (width >> 1);
            var i  = y * width + x;

            phases.radial[i] = ((xx * xx + yy * yy) >> 3) & 255;
            phases.diag[i]   = ((x + y) * 5) & 255;
        }
    }

    return phases;
}

function renderFrame(indexes, width, height, phases, waves, frame) {
    var sum = 0;
    var t0  = frame * 5;
    var t1  = frame * 3;
    var t2  = frame * 9;
    var base = frame * 2;

    for (var y = 0; y < height; y++) {
        var yw  = waves[(phases.y[y] + t1) & 255];
        var row = y * width;

        for (var x = 0; x < width; x++) {
            var i = row + x;
            var v = waves[(phases.x[x] + t0) & 255];

            v += yw;
            v += waves[(phases.radial[i] + t0) & 255];
            v += waves[(phases.diag[i] + t2) & 255] >> 1;

            var index = (v + base) & 255;
            indexes[i] = index;
            sum = (sum + index) >>> 0;
        }
    }

    return sum;
}

if (fbWidth === 0 || fbHeight === 0) {
    console.log("[plasma] no framebuffer in this SoC");
} else {
    var width   = Math.min(48, fbWidth);
    var height  = Math.min(36, fbHeight);
    var scale   = Math.max(1, Math.min(8,
        Math.min((fbWidth / width) >> 0, (fbHeight / height) >> 0)));
    var x0      = (fbWidth  - width  * scale) >> 1;
    var y0      = (fbHeight - height * scale) >> 1;
    var indexes = new Uint8Array(width * height);
    var palette = makePalette();
    var waves   = makeWaves();
    var phases  = makePhases(width, height);
    var frames  = 64;
    var started = litex.millis();
    var sum     = 0;

    console.log("[plasma] framebuffer =", fbWidth, "x", fbHeight, "x", framebuffer.depth);
    console.log("[plasma] animated tile =", width, "x", height, "x", scale);
    console.log("[plasma] frames =", frames);

    framebuffer.clear(0x000000);

    for (var frame = 0; frame < frames; frame++) {
        sum = renderFrame(indexes, width, height, phases, waves, frame);
        framebuffer.blitIndexedScale(indexes, palette, width, height, x0, y0, scale);

        if ((frame & 15) === 15)
            console.log("[plasma] frame", frame + 1, "checksum = 0x" + sum.toString(16));

        litex.delay(5);
    }

    console.log("[plasma] final checksum = 0x" + sum.toString(16));
    console.log("[plasma] animation took", litex.millis() - started, "ms");
    console.log("[plasma] done");
}
