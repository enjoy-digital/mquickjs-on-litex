<!--
Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
SPDX-License-Identifier: BSD-2-Clause
-->

# JavaScript API

This is the small API exposed by the firmware to scripts running inside
mquickjs on the LiteX CPU. The same API is available to embedded scripts,
SDCard scripts and browser-submitted live scripts.

## Runtime Model

A normal script runs once at boot:

```js
console.log("hello from mquickjs on LiteX!");
```

The live browser demo also recognizes two optional functions:

```js
function setup() {
    // Called once after the script is accepted by /run.
}

function frame(t) {
    // Called repeatedly while the script is active.
    // t is litex.millis() in milliseconds.
}
```

If `setup()` throws, the new live script is rejected and the previous
script keeps running. If `frame(t)` throws, the live loop stops and the
error state is reported by `/info`.

## `litex`

`litex` exposes generic LiteX board features. Optional peripherals
return neutral values when absent, so the same script can run on many
targets.

```js
litex.getIdentifier()          // SoC identifier string
litex.clockFrequency()         // system clock in Hz
litex.millis()                 // monotonic milliseconds when timer uptime exists
litex.delay(ms)                // busy wait

litex.setLeds(mask)            // no-op when LEDs are absent
litex.getSwitches()            // 0 when switches are absent
litex.getButtons()             // 0 when buttons are absent

litex.getScratch()             // 0 when ctrl scratch is absent
litex.setScratch(value)        // no-op when ctrl scratch is absent

litex.csrRead32(addr)          // raw CSR/bus access
litex.csrWrite32(addr, value)
```

File helpers are present when the firmware has SDCard/FatFS support:

```js
litex.readFile("main.js")
litex.writeFile("main.js", source)
litex.load("main.js")          // evaluate another JavaScript file
```

Without SDCard support, the file helpers throw an error.

## `framebuffer`

`framebuffer` is available when the LiteX target has a video framebuffer.
When no framebuffer is present, `width`, `height` and `depth` are `0`.

```js
framebuffer.width
framebuffer.height
framebuffer.depth
framebuffer.doubleBuffered

framebuffer.clear(color)
framebuffer.fillRect(x, y, width, height, color)
framebuffer.copyRect(srcX, srcY, width, height, dstX, dstY)
framebuffer.line(x0, y0, x1, y1, color)
framebuffer.circle(x, y, radius, color, fill)
framebuffer.text(x, y, text, color, scale)
framebuffer.fade(amount)

framebuffer.blit(buffer, width, height, x, y)
framebuffer.blitScale(buffer, width, height, x, y, scale)
framebuffer.blitIndexedScale(indexes, palette, width, height, x, y, scale)
```

Colors are `0xRRGGBB`. The firmware converts them to the target
framebuffer format when needed.

## Double Buffering

When the framebuffer memory is large enough for two pages,
`framebuffer.doubleBuffered` is true. Use this pattern for clean video:

```js
function frame(t) {
    if (framebuffer.doubleBuffered) {
        framebuffer.begin();
    }

    framebuffer.clear(0x020406);
    framebuffer.text(24, 24, "mquickjs on LiteX", 0x12bdf2, 2);

    if (framebuffer.doubleBuffered) {
        framebuffer.present();
    }
}
```

`begin()` selects the hidden page and returns its page index. `present()`
flushes the CPU/L2 caches and switches the video DMA base at a safer
scanout point. Scripts that only update a few small regions can still
draw directly to the visible page, but full-frame demos should use
double buffering when available.

## Live Browser Helper

The browser UI prepends a small helper to each live script. It is not a
firmware ABI, but it is useful for demos:

```js
params.speed
params.scale
params.count
params.hue
params.overlay

demo.rgb(r, g, b)
demo.hue(h)
demo.clear(color)
demo.beginFrame(color)
demo.endFrame()
demo.led(n)
demo.centerX(width)
demo.centerY(height)
demo.blitIndexed(indexes, palette, width, height, scale)
demo.controls()
demo.overlay(name, t)
demo.panel(x, y, width, height, title, tag)
demo.scriptPreview(status, lines)
```

The helper lives in `tools/live_assets.js`, which is also embedded in the
firmware and served as `/live_assets.js`.

## Portability Rules

Use feature checks when a script should run everywhere:

```js
if (framebuffer.width) {
    framebuffer.clear(0);
}

if (litex.getButtons() & 1) {
    litex.setLeds(0xf);
}
```

Keep live `frame(t)` functions bounded. They run in the Ethernet polling
loop, so a long frame makes the web UI less responsive. Prefer compact
tiles plus `blitIndexedScale()` for visual effects, and reserve raw
per-pixel loops over the full framebuffer for experiments.
