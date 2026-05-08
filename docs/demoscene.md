<!--
Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
SPDX-License-Identifier: BSD-2-Clause
-->

# Live JavaScript Demoscene

The long-term demo idea is simple: edit JavaScript live and let a
bare-metal LiteX SoC draw pixels on HDMI/VGA through mquickjs.

The practical path is to build it in small pieces, fully testable in
`litex_sim`:

1. Run a LiteX simulator with SDRAM and a video framebuffer.
2. Expose a tiny `framebuffer` object to JavaScript.
3. Render classic effects in JavaScript.
4. Add live reload over UART TCP or Ethernet.
5. Reuse the same JS API on hardware targets with a video framebuffer.

## First Prototype

```sh
./make.py sim-video
```

This builds a separate simulator in `build/sim-video` with:

```text
litex_sim --with-sdram --with-video-framebuffer
```

The JavaScript API is intentionally small:

```js
framebuffer.width
framebuffer.height
framebuffer.depth
framebuffer.doubleBuffered
framebuffer.begin()
framebuffer.present()
framebuffer.clear(color)
framebuffer.fillRect(x, y, width, height, color)
framebuffer.copyRect(srcX, srcY, width, height, dstX, dstY)
framebuffer.blit(buffer, width, height, x, y)
framebuffer.blitScale(buffer, width, height, x, y, scale)
framebuffer.blitIndexedScale(indexes, palette, width, height, x, y, scale)
```

`buffer` is normally a `Uint32Array` of 24-bit RGB pixels. The firmware
packs colors to the active framebuffer format, so the same JavaScript
works with 32-bit RGB888 and 16-bit RGB565 framebuffers. The optional
dirty rectangle lets scripts update only part of the source tile.
`blitScale` expands each source pixel to a square block in C, so the
simulator demo stays fast while still producing a visible HDMI image on
hardware.

On hardware, avoid drawing multi-step animations directly into the
visible scanout buffer when `framebuffer.doubleBuffered` is true:

```js
var page = framebuffer.begin();
framebuffer.clear(0x020406);
framebuffer.fillRect(40, 40, 120, 60, 0x12bdf2);
framebuffer.present();
```

`begin()` returns the hidden page index, which lets sparse animations
remember what was last drawn on each page and update only dirty regions
before `present()`. The Logo preset uses this to get clean HDMI output
without clearing the full 640 x 480 frame every tick.

`blitIndexedScale` is the hardware-friendly variant for retro effects:
JavaScript fills a `Uint8Array` tile and a 256-entry `Uint32Array`
palette, then C expands the indexed pixels to the framebuffer. The
effect remains scripted, but the repeated pixel copy is no longer a
property-by-property JavaScript loop. `paletteOffset` supports cheap
palette-cycling effects without rewriting the index buffer.

Both scaled blitters also accept optional dirty rectangles:

```text
blitScale(..., dirtyX, dirtyY, dirtyW, dirtyH)
blitIndexedScale(..., paletteOffset, dirtyX, dirtyY, dirtyW, dirtyH)
```

`fillRect` and `copyRect` are bulk primitives for text consoles, games,
scrolling, and small UI overlays. They are intentionally generic, so
other demos can reuse them without pulling in plasma-specific code.

`examples/showcase.js` is the default video demo. It runs plasma, fire
and tunnel as a playlist. In 1MHz simulation it uses tiny one-frame
settings; on hardware it automatically uses a larger animated tile.

The individual demos are useful while tuning one effect:

```sh
./make.py sim-video examples/plasma.js
./make.py sim-video examples/fire.js
./make.py sim-video examples/tunnel.js
./make.py firmware examples/plasma_animated.js --build-dir build/ecpix5-video
```

For hardware, use the showcase firmware:

```sh
./make.py board-build --target litex_boards.targets.lambdaconcept_ecpix5 --build-dir build/ecpix5-video -- --with-video-framebuffer --uart-baudrate=1000000 --uart-fifo-depth=512
./make.py firmware examples/showcase.js --build-dir build/ecpix5-video
./make.py board-load --target litex_boards.targets.lambdaconcept_ecpix5 --build-dir build/ecpix5-video
./make.py board-run --serial /dev/ttyUSB2 --baudrate 1000000
```

## Live Editing

The live-editing path is intentionally small:

```text
Browser UI JS -> HTTP POST -> LiteX board -> mquickjs script.js -> framebuffer
```

The board serves the browser page from a tiny lwIP HTTP endpoint in the
firmware. The browser JavaScript only manages the UI and HTTP requests.
The script in the editor is sent to `/run`, executes in mquickjs on the
LiteX CPU, and draws through the framebuffer bindings. `Run` replaces
the current script, `setup()` runs once and `frame(t)` keeps animating
from the firmware poll loop.

Build an Ethernet + video target, then build live firmware:

```sh
./make.py board-build --target litex_boards.targets.lambdaconcept_ecpix5 --build-dir build/ecpix5-live -- --with-ethernet --with-video-framebuffer --uart-baudrate=1000000 --uart-fifo-depth=512
./make.py live --build-dir build/ecpix5-live
./make.py board-load --target litex_boards.targets.lambdaconcept_ecpix5 --build-dir build/ecpix5-live
./make.py board-run --serial /dev/ttyUSB2 --baudrate 1000000
```

Open `http://192.168.1.50/`, choose a preset, edit the JavaScript and
press `Run`. Scripts are bounded to 16 KiB in the firmware, so keep live
experiments compact. The peripheral panel posts the same kind of script:
the interactive I/O demo reads switches/buttons, drives LEDs and redraws
the framebuffer without any LED-specific browser magic.

For longer editing sessions, use `tools/live_editor.html` from the host.
It keeps the firmware page small but adds a richer preset list, slot
save/load controls and shareable URL hashes.

The older UDP host bridge is still present as a fallback:

```sh
./make.py live --live-mode udp --build-dir build/ecpix5-live
./tools/live_bridge.py --board 192.168.1.50
```

The goal is that the same demo scripts remain portable across
simulation and LiteX board targets.
