// Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
// SPDX-License-Identifier: BSD-2-Clause
//
// Shared browser-side live demo assets. The strings below are sent to
// mquickjs and execute on the LiteX CPU; this file itself executes in
// the browser and is reused by the board web UI and the host editor.

(function(global) {
"use strict";

// This helper is prepended to every script sent to mquickjs. It runs on
// the LiteX CPU, not in the browser, and provides a small demo toolkit.
const helper = `
var params = {speed:32, scale:6, count:96, hue:0, overlay:1};
var demo = {
  rgb: function(r, g, b) {
    return ((r & 255) << 16) | ((g & 255) << 8) | (b & 255);
  },
  hue: function(h) {
    h &= 255;
    return demo.rgb((h * 5) & 255, (255 - h) & 255, (h * 3) & 255);
  },
  clear: function(c) {
    if (framebuffer.width) framebuffer.clear(c);
  },
  beginFrame: function(c) {
    if (!framebuffer.width) return;
    if (framebuffer.doubleBuffered) framebuffer.begin();
    framebuffer.clear(c);
  },
  endFrame: function() {
    if (framebuffer.width && framebuffer.doubleBuffered) framebuffer.present();
  },
  led: function(n) {
    litex.setLeds(1 << (n & 3));
  },
  centerX: function(w) {
    return ((framebuffer.width - w) / 2) | 0;
  },
  centerY: function(h) {
    return ((framebuffer.height - h) / 2) | 0;
  },
  blitIndexed: function(idx, pal, w, h, s) {
    framebuffer.blitIndexedScale(idx, pal, w, h, demo.centerX(w * s), demo.centerY(h * s), s);
  },
  controls: function() {
    var sw = litex.getSwitches();
    var btn = litex.getButtons();
    if (sw) params.hue = (sw * 17) & 255;
    if (btn & 1) params.speed = 64;
    if (btn & 2) params.count = 192;
  },
  overlay: function(name, t) {
    if (!params.overlay || !framebuffer.width) return;
    var ww = framebuffer.width;
    framebuffer.fillRect(0, 0, ww, 26, 0x020406);
    if (ww > 16) framebuffer.fillRect(8, 5, ww - 16, 16, 0x06080b);
    framebuffer.line(0, 25, ww - 1, 25, 0x10384d);
    framebuffer.text(16, 9, name + " " + ((t / 1000) | 0) + "S", 0x12bdf2, 1);
    if (ww > 220) {
      framebuffer.text(ww - 210, 9, "SPD " + params.speed + " SW " + litex.getSwitches(), 0x8fa3b2, 1);
    }
  },
  panel: function(x, y, pw, ph, title, tag) {
    framebuffer.fillRect(x, y, pw, ph, 0x06080b);
    framebuffer.line(x, y, x + pw - 1, y, 0x30343a);
    framebuffer.line(x, y + ph - 1, x + pw - 1, y + ph - 1, 0x30343a);
    framebuffer.line(x, y, x, y + ph - 1, 0x30343a);
    framebuffer.line(x + pw - 1, y, x + pw - 1, y + ph - 1, 0x30343a);
    framebuffer.line(x + 1, y + 28, x + pw - 2, y + 28, 0x10384d);
    framebuffer.text(x + 10, y + 10, title, 0x12bdf2, 1);
    if (tag) framebuffer.text(x + pw - tag.length * 6 - 10, y + 10, tag, 0x8fa3b2, 1);
  },
  scriptPreview: function(status, lines) {
    var w = framebuffer.width;
    var h = framebuffer.height;
    if (!w || !h) {
      console.log("[preview] no framebuffer");
      return;
    }

    demo.beginFrame(0x020406);
    if (w < 560 || h < 420) {
      framebuffer.text(8, 10, "SCRIPT.JS", 0xf7f9fb, 1);
      framebuffer.text(8, 30, status, 0x12bdf2, 1);
      demo.endFrame();
      return;
    }

    var m = 24;
    var gap = 16;
    var leftW = (((w - 2 * m - gap) * 3) / 5) | 0;
    var rightX = m + leftW + gap;
    var rightW = w - rightX - m;
    var topY = 92;
    var panelH = h - 228;
    var bottomY = h - 88;

    framebuffer.line(m, 70, w - m, 70, 0x0b87bd);
    framebuffer.text(m, 26, "MQUICKJS", 0xf7f9fb, 3);
    framebuffer.text(m + 162, 26, "ON LITEX", 0x12bdf2, 3);
    framebuffer.text(m + 2, 62, "JAVASCRIPT > MQUICKJS > LITEX SOC", 0x12bdf2, 1);

    framebuffer.fillRect(w - m - 96, 33, 96, 26, 0x080c10);
    framebuffer.line(w - m - 96, 33, w - m - 1, 33, 0x0b87bd);
    framebuffer.line(w - m - 96, 58, w - m - 1, 58, 0x0b87bd);
    framebuffer.line(w - m - 96, 33, w - m - 96, 58, 0x0b87bd);
    framebuffer.line(w - m - 1, 33, w - m - 1, 58, 0x0b87bd);
    framebuffer.text(w - m - 76, 42, status, 0x8ed94f, 1);

    demo.panel(m, topY, leftW, panelH, "SCRIPT.JS", "LOADED");
    framebuffer.fillRect(m + 12, topY + 42, leftW - 24, panelH - 54, 0x020406);
    for (var i = 0; i < lines.length && i < 7; i++) {
      var y = topY + 54 + i * 22;
      var n = i + 1;
      framebuffer.text(m + 28, y, n < 10 ? "0" + n : "" + n, 0x8fa3b2, 1);
      framebuffer.text(m + 58, y, lines[i], 0xdce8f2, 1);
    }

    demo.panel(rightX, topY, rightW, 112, "BOARD", "STATUS");
    framebuffer.text(rightX + 12, topY + 44, litex.getIdentifier(), 0xdce8f2, 1);
    framebuffer.text(rightX + 12, topY + 66, "FB " + w + "X" + h + "X" + framebuffer.depth, 0x8fa3b2, 1);
    framebuffer.text(rightX + 12, topY + 88, "ETH 192.168.1.50", 0x8ed94f, 1);

    demo.panel(rightX, topY + 128, rightW, panelH - 128, "RUNTIME", "IDLE");
    framebuffer.text(rightX + 12, topY + 172, "BROWSER EDITOR", 0xdce8f2, 1);
    framebuffer.text(rightX + 12, topY + 194, "RUN TO EXECUTE", 0x8fa3b2, 1);
    framebuffer.text(rightX + 12, topY + 216, "JS EXECUTES HERE", 0x12bdf2, 1);

    demo.panel(m, bottomY, w - 2 * m, 64, "CONSOLE", "READY");
    framebuffer.text(m + 14, bottomY + 34, "OPEN HTTP://192.168.1.50/", 0x8ed94f, 1);
    framebuffer.text(m + 14, bottomY + 52, "SCRIPT.JS PREVIEW REFRESHED FROM WEB UI", 0x8fa3b2, 1);
    demo.endFrame();
    console.log("[preview] " + status + " - script.js shown on framebuffer");
  }
};
`;

const defaultScript = `// Browser edited JavaScript, executed by mquickjs on LiteX.
var t0 = 0;

function setup() {
  t0 = litex.millis();
  demo.clear(0x020406);
  console.log("[live] setup done");
}

function frame(t) {
  var phase = (((t - t0) * params.speed) / 256) | 0;
  var maxX = framebuffer.width  > 18 ? framebuffer.width  - 18 : 0;
  var maxY = framebuffer.height > 18 ? framebuffer.height - 18 : 0;

  demo.beginFrame(demo.hue(phase + params.hue));
  for (var i = 0; i < params.count; i++) {
    var x = maxX ? (i * 17 + phase * 5) % maxX : 0;
    var y = maxY ? (i * 11 + phase * 3) % maxY : 0;
    framebuffer.fillRect(x, y, 12, 12, demo.hue(phase + i * 3));
  }
  demo.endFrame();

  demo.led(phase >> 3);
}`;

// Presets are scripts sent to mquickjs. Keep them readable: users will
// edit these directly in the script.js textarea.
const presets = {
  bars: `// Fast sanity pattern: one 8-pixel color stripe per row.
var start = 0;

function setup() {
  start = litex.millis();
  console.log("[bars] running");
}

function frame(t) {
  demo.controls();
  var phase = (((t - start) * params.speed) / 256) | 0;

  demo.beginFrame(0x020406);
  for (var y = 0; y <= framebuffer.height - 8; y += 8) {
    var c = demo.hue(y + phase + params.hue);
    framebuffer.fillRect(0, y, framebuffer.width, 8, c);
  }

  demo.overlay("BARS", t);
  demo.endFrame();
  demo.led(phase >> 4);
}`,

  plasma: `// Low-resolution indexed plasma, scaled by the C framebuffer helper.
var w = 80;
var h = 60;
var pixels, palette, start;

function setup() {
  start = litex.millis();
  pixels = new Uint8Array(w * h);
  palette = new Uint32Array(256);
  for (var i = 0; i < 256; i++) palette[i] = demo.hue(i);
  console.log("[plasma] low-res indexed plasma");
}

function frame(t) {
  demo.controls();
  var phase = (((t - start) * params.speed) / 128) | 0;

  for (var y = 0; y < h; y++) {
    for (var x = 0; x < w; x++) {
      pixels[y * w + x] = ((x * x + y * y + x * phase + y * (phase >> 1)) >> 5) & 255;
    }
  }

  demo.beginFrame(0x020406);
  demo.blitIndexed(pixels, palette, w, h, params.scale);
  demo.overlay("PLASMA", t);
  demo.endFrame();
  demo.led(phase >> 4);
}`,

  spark: `// Cheap particle field with fading trails instead of full clears.
var start = 0;

function setup() {
  start = litex.millis();
  demo.clear(0x020406);
  console.log("[spark] particles");
}

function frame(t) {
  demo.controls();
  var phase = (((t - start) * params.speed) / 192) | 0;
  var maxX = framebuffer.width  > 6 ? framebuffer.width  - 6 : 0;
  var maxY = framebuffer.height > 6 ? framebuffer.height - 6 : 0;

  framebuffer.fade(44);
  for (var i = 0; i < params.count; i++) {
    var x = maxX ? (i * 23 + phase * 9) % maxX : 0;
    var y = maxY ? (i * i  + phase * 13) % maxY : 0;
    framebuffer.fillRect(x, y, 6, 6, demo.hue(params.hue + phase + i));
  }

  demo.overlay("SPARK", t);
  demo.led(phase >> 3);
}`,

  tunnel: `// Classic tunnel effect using distance from the center.
var w = 80;
var h = 60;
var pixels, palette, start;

function setup() {
  start = litex.millis();
  pixels = new Uint8Array(w * h);
  palette = new Uint32Array(256);
  for (var i = 0; i < 256; i++) palette[i] = demo.hue(i + params.hue);
  console.log("[tunnel] integer tunnel");
}

function frame(t) {
  demo.controls();
  var phase = (((t - start) * params.speed) / 160) | 0;
  var cx = w >> 1;
  var cy = h >> 1;

  for (var y = 0; y < h; y++) {
    for (var x = 0; x < w; x++) {
      var dx = x - cx;
      var dy = y - cy;
      var d = dx * dx + dy * dy + 1;
      pixels[y * w + x] = ((4096 / d) + x * 3 + y * 5 + phase) & 255;
    }
  }

  demo.beginFrame(0x020406);
  demo.blitIndexed(pixels, palette, w, h, params.scale);
  demo.overlay("TUNNEL", t);
  demo.endFrame();
  demo.led(phase >> 4);
}`,

  fire: `// Very small fire simulation: seed the bottom row, blur upward.
var w = 80;
var h = 60;
var heat, palette, start;

function setup() {
  start = litex.millis();
  heat = new Uint8Array(w * h);
  palette = new Uint32Array(256);
  for (var i = 0; i < 256; i++) {
    palette[i] = demo.rgb(i, i > 96 ? 255 : i * 2, i > 180 ? (i - 180) * 3 : 0);
  }
  console.log("[fire] running");
}

function frame(t) {
  demo.controls();
  var phase = (((t - start) * params.speed) / 256) | 0;

  for (var x = 0; x < w; x++) heat[(h - 1) * w + x] = (x * 17 + phase * 13) & 255;
  for (var y = 0; y < h - 1; y++) {
    for (var x = 1; x < w - 1; x++) {
      var below = y + 2 < h ? y + 2 : h - 1;
      var value = (heat[(y + 1) * w + x - 1] + heat[(y + 1) * w + x] +
                   heat[(y + 1) * w + x + 1] + heat[below * w + x]) >> 2;
      heat[y * w + x] = value > 2 ? value - 2 : 0;
    }
  }

  demo.beginFrame(0x020406);
  demo.blitIndexed(heat, palette, w, h, params.scale);
  demo.overlay("FIRE", t);
  demo.endFrame();
  demo.led(phase >> 4);
}`,

  stars: `// Simple star field. Lower z means closer to the viewer.
var starX = [], starY = [], starZ = [];

function setup() {
  for (var i = 0; i < 256; i++) {
    starX[i] = (i * 37) % 640 - 320;
    starY[i] = (i * 71) % 480 - 240;
    starZ[i] = (i % 64) + 1;
  }
  demo.clear(0);
  console.log("[stars] running");
}

function frame(t) {
  demo.controls();
  framebuffer.fade(36);

  for (var i = 0; i < params.count; i++) {
    starZ[i] -= params.speed / 24;
    if (starZ[i] < 1) starZ[i] = 64;

    var x = (framebuffer.width  / 2 + (starX[i] * 48) / starZ[i]) | 0;
    var y = (framebuffer.height / 2 + (starY[i] * 48) / starZ[i]) | 0;
    framebuffer.fillRect(x, y, 2, 2, demo.hue(params.hue + starZ[i] * 4));
  }

  demo.overlay("STARS", t);
}`,

  life: `// Conway-style cellular automaton rendered through an indexed buffer.
var w = 96;
var h = 64;
var cells, nextCells, palette;

function setup() {
  cells = new Uint8Array(w * h);
  nextCells = new Uint8Array(w * h);
  palette = new Uint32Array(256);
  palette[0] = 0x020406;
  palette[1] = 0x12bdf2;

  for (var i = 0; i < w * h; i++) cells[i] = ((i * i + i * 7) & 13) == 0;
  console.log("[life] running");
}

function frame(t) {
  demo.controls();
  for (var y = 1; y < h - 1; y++) {
    for (var x = 1; x < w - 1; x++) {
      var n = cells[(y - 1) * w + x - 1] + cells[(y - 1) * w + x] + cells[(y - 1) * w + x + 1] +
              cells[y * w + x - 1]                         + cells[y * w + x + 1] +
              cells[(y + 1) * w + x - 1] + cells[(y + 1) * w + x] + cells[(y + 1) * w + x + 1];
      var alive = cells[y * w + x];
      nextCells[y * w + x] = (n == 3) || (alive && n == 2);
    }
  }

  var tmp = cells;
  cells = nextCells;
  nextCells = tmp;
  demo.beginFrame(0x020406);
  demo.blitIndexed(cells, palette, w, h, params.scale);
  demo.overlay("LIFE", t);
  demo.endFrame();
}`,

  vector: `// Vector-scope style lines with framebuffer fading trails.
var start = 0;

function setup() {
  start = litex.millis();
  demo.clear(0);
  console.log("[vector] running");
}

function frame(t) {
  demo.controls();
  var phase = (((t - start) * params.speed) / 128) | 0;
  var cx = framebuffer.width >> 1;
  var cy = framebuffer.height >> 1;

  framebuffer.fade(28);
  for (var i = 0; i < 16; i++) {
    var x = (i * 43 + phase * 7) % framebuffer.width;
    var y = (i * 29 + phase * 5) % framebuffer.height;
    framebuffer.line(cx, cy, x, y, demo.hue(params.hue + phase + i * 16));
  }

  framebuffer.circle(cx, cy, (phase % 90) + 20, demo.hue(params.hue + phase), 0);
  demo.overlay("VECTOR", t);
}`,

  logo: `// DVD-style bouncing badge with color changes on wall hits.
var start = 0;
var x = 42;
var y = 58;
var dx = 3;
var dy = 2;
var hue = 48;
var accent = 0x12bdf2;
var accent2 = 0x8ed94f;
var phase = 0;
var lastX = -1;
var lastY = -1;
var lastStep = 0;
var badgeW = 318;
var badgeH = 104;
var pageX = [-1, -1];
var pageY = [-1, -1];

function setup() {
  start = litex.millis();
  lastStep = start;
  updateAccent();
  drawFrame();
  lastX = x;
  lastY = y;
  console.log("[logo] running");
}

function updateAccent() {
  accent = demo.hue(hue);
  accent2 = demo.hue(hue + 96);
}

function outline(x, y, w, h, color) {
  framebuffer.line(x, y, x + w - 1, y, color);
  framebuffer.line(x, y + h - 1, x + w - 1, y + h - 1, color);
  framebuffer.line(x, y, x, y + h - 1, color);
  framebuffer.line(x + w - 1, y, x + w - 1, y + h - 1, color);
}

function drawBadge(x, y) {
  var ix = x + 8;
  var iy = y + 8;
  var iw = badgeW - 16;
  var ih = badgeH - 16;
  var tick = phase % 7;

  framebuffer.fillRect(x + 6, y + 7, badgeW - 12, badgeH - 12, 0x010203);
  framebuffer.fillRect(ix, iy, iw, ih, 0x06080b);
  outline(ix, iy, iw, ih, accent);

  framebuffer.fillRect(ix + 12, iy + 12, 6, 6, accent2);
  framebuffer.fillRect(ix + 24, iy + 12, 6, 6, tick & 1 ? accent : 0x10384d);
  framebuffer.fillRect(ix + 36, iy + 12, 6, 6, tick & 2 ? accent : 0x10384d);
  framebuffer.text(ix + 54, iy + 11, "LIVE JAVASCRIPT", accent, 1);

  framebuffer.text(ix + 18, iy + 36, "MQUICKJS", 0xf7f9fb, 3);
  framebuffer.text(ix + 176, iy + 42, "ON LITEX", accent, 2);
  framebuffer.text(ix + 19, iy + 74, "BROWSER JS > MQUICKJS > LITEX SOC", 0x8fa3b2, 1);

  for (var i = 0; i < 7; i++) {
    var c = i == tick ? accent2 : 0x10384d;
    framebuffer.fillRect(ix + iw - 76 + i * 10, iy + ih - 15, 6, 6, c);
  }
}

function clearRect(x, y, w, h) {
  if (w > 0 && h > 0) framebuffer.fillRect(x, y, w, h, 0x020406);
}

function eraseOldBadge(oldX, oldY, newX, newY) {
  var oldR = oldX + badgeW;
  var oldB = oldY + badgeH;
  var newR = newX + badgeW;
  var newB = newY + badgeH;
  var ix0 = oldX > newX ? oldX : newX;
  var iy0 = oldY > newY ? oldY : newY;
  var ix1 = oldR < newR ? oldR : newR;
  var iy1 = oldB < newB ? oldB : newB;

  if (ix0 >= ix1 || iy0 >= iy1) {
    clearRect(oldX, oldY, badgeW, badgeH);
    return;
  }

  clearRect(oldX, oldY, badgeW, iy0 - oldY);
  clearRect(oldX, iy1, badgeW, oldB - iy1);
  clearRect(oldX, iy0, ix0 - oldX, iy1 - iy0);
  clearRect(ix1, iy0, oldR - ix1, iy1 - iy0);
}

function drawFrame() {
  if (framebuffer.doubleBuffered) {
    var page = framebuffer.begin();
    if (pageX[page] < 0) {
      framebuffer.clear(0x020406);
    } else {
      clearRect(pageX[page], pageY[page], badgeW, badgeH);
    }
    drawBadge(x, y);
    framebuffer.present();
    pageX[page] = x;
    pageY[page] = y;
    return;
  }

  if (lastX < 0) {
    demo.clear(0x020406);
  } else {
    eraseOldBadge(lastX, lastY, x, y);
  }
  drawBadge(x, y);
}

function frame(t) {
  demo.controls();
  if (!framebuffer.width || (t - lastStep) < 40) return;
  lastStep = t;
  phase = (((t - start) * params.speed) / 256) | 0;

  var maxX = framebuffer.width > badgeW ? framebuffer.width - badgeW : 1;
  var maxY = framebuffer.height > badgeH ? framebuffer.height - badgeH : 1;
  var hit = 0;

  x += dx;
  y += dy;

  if (x <= 0) {
    x = 0;
    dx = 3;
    hit = 1;
  } else if (x >= maxX) {
    x = maxX;
    dx = -3;
    hit = 1;
  }
  if (y <= 0) {
    y = 0;
    dy = 2;
    hit = 1;
  } else if (y >= maxY) {
    y = maxY;
    dy = -2;
    hit = 1;
  }
  if (hit) {
    hue = (hue + 53) & 255;
    updateAccent();
  }

  drawFrame();
  lastX = x;
  lastY = y;
  demo.led((x + y) >> 5);
}`
};

const presetInfo = {
  bars: {
    title: "Bars",
    description: "Fast color stripes for checking the framebuffer and palette path.",
  },
  plasma: {
    title: "Plasma",
    description: "Low-resolution indexed plasma scaled by the C framebuffer helper.",
  },
  spark: {
    title: "Spark",
    description: "Tiny particle field with fading trails and switch/button controls.",
  },
  tunnel: {
    title: "Tunnel",
    description: "Classic distance-field tunnel using compact integer math.",
  },
  fire: {
    title: "Fire",
    description: "Small fire buffer seeded in JavaScript and expanded to video.",
  },
  stars: {
    title: "Stars",
    description: "Star field with depth values and live speed/count controls.",
  },
  life: {
    title: "Life",
    description: "Cellular automaton rendered through an indexed framebuffer.",
  },
  vector: {
    title: "Vector",
    description: "Vector-scope lines with framebuffer fading trails.",
  },
  logo: {
    title: "Logo",
    description: "Minimal DVD-style badge with bounce-triggered color changes.",
  },
};

global.LiveDemo = {
  helper: helper,
  defaultScript: defaultScript,
  presetInfo: presetInfo,
  presets: presets,
};
})(typeof window !== "undefined" ? window : globalThis);
