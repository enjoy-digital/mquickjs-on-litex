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
./make.py sim-video examples/plasma.js
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
framebuffer.clear(color)
framebuffer.blit(buffer, width, height, x, y)
framebuffer.blitScale(buffer, width, height, x, y, scale)
```

`buffer` is normally a `Uint32Array` of RGB pixels. The optional
`width`, `height`, `x` and `y` arguments let scripts draw a smaller tile
inside the framebuffer. `blitScale` expands each source pixel to a
square block in C, so scripts stay small enough for the 1MHz simulator
while still producing a visible HDMI image on hardware.

The first hardware validation used ECPIX-5:

```sh
./make.py board-build --target litex_boards.targets.lambdaconcept_ecpix5 --build-dir build/ecpix5-video -- --with-video-framebuffer
./make.py firmware examples/plasma.js --build-dir build/ecpix5-video
./make.py board-load --target litex_boards.targets.lambdaconcept_ecpix5 --build-dir build/ecpix5-video
./make.py board-run --serial /dev/ttyUSB2
```

## Next Step

Once the visual path is solid, add a live code path:

- UART TCP first: easiest to debug in simulation.
- UDP over simulated Ethernet next: simple and board-like.
- HTTP/WebSocket browser editor later: nicest demo, but more moving
  parts.

The goal is that the same demo scripts remain portable across
simulation and LiteX board targets.
