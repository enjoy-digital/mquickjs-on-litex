<!--
Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
SPDX-License-Identifier: BSD-2-Clause
-->

# Architecture

mquickjs on LiteX is a small bare-metal firmware that embeds Fabrice
Bellard's mquickjs engine in a LiteX SoC. The project keeps the split
simple: LiteX provides the SoC, C exposes a few hardware bindings, and
JavaScript drives the demo logic.

## Normal Flow

```text
JavaScript file
      |
      v
firmware/embed_script.py -> user_script.h
      |
      v
firmware.elf / firmware.bin
      |
      v
LiteX BIOS loads firmware into main_ram
      |
      v
VexRiscv runs mquickjs on bare metal
      |
      v
JavaScript calls litex.* and framebuffer.*
```

`make.py` is the user entry point. It builds the LiteX simulator or a
board target, then rebuilds only the firmware when the JavaScript file
changes.

## Firmware Layers

```text
main.c
  - boot banner
  - creates the mquickjs context and heap
  - runs embedded JavaScript or the UART REPL

mqjs_port.c
  - console.log / print
  - litex.* bindings
  - framebuffer.* bindings
  - optional SDCard/FatFS load()

live_http.c
  - tiny HTTP server for the live browser demo
  - POST /run evaluates JavaScript and returns console output

liteeth_lwip.c
  - LiteEth SRAM MAC <-> lwIP netif bridge
  - polling RX, blocking TX
```

The JavaScript heap is static and sized by `LITEX_MQJS_HEAP_SIZE`
(default 1 MiB). mquickjs does not call `malloc()` for JavaScript
objects.

## JavaScript APIs

The `litex` object is intentionally generic and degrades when optional
CSRs are missing:

```js
litex.getIdentifier()
litex.clockFrequency()
litex.millis()
litex.delay(ms)
litex.setLeds(mask)
litex.getSwitches()
litex.getButtons()
litex.getScratch()
litex.setScratch(value)
litex.csrRead32(addr)
litex.csrWrite32(addr, value)
litex.readFile(path)
litex.load(path)
```

The `framebuffer` object is present when the SoC has a LiteX video
framebuffer:

```js
framebuffer.width
framebuffer.height
framebuffer.depth
framebuffer.clear(color)
framebuffer.fillRect(x, y, width, height, color)
framebuffer.copyRect(srcX, srcY, width, height, dstX, dstY)
framebuffer.blit(buffer, width, height, x, y)
framebuffer.blitScale(buffer, width, height, x, y, scale)
framebuffer.blitIndexedScale(indexes, palette, width, height, x, y, scale)
```

## Live Browser Demo

With Ethernet and a video framebuffer enabled, the firmware can serve the
live demo page directly from the board:

```text
Browser
  GET  /      -> board-served editor page
  POST /run  -> JavaScript source
                  |
                  v
              mquickjs context reset
                  |
                  v
              script evaluation
                  |
                  v
              HTTP response with OK/ERR and console output
```

The HTTP path uses lwIP in `NO_SYS=1` mode. There is no OS, socket API,
filesystem or dynamic web server; the page is a static C string and the
HTTP parser accepts only the small request set needed by the demo.

For robustness while iterating, each `/run` request creates a fresh
mquickjs context. This keeps failed scripts from poisoning the next run.
The browser utility buttons simply post short JavaScript snippets that
use the same `litex.*` and `framebuffer.*` APIs as user scripts.

## Boot Options

Serial boot is the fast iteration path:

```text
LiteX BIOS -> serial loader -> firmware.bin -> main_ram -> mquickjs
```

SDCard boot is the standalone path:

```text
LiteX BIOS -> boot.bin -> mquickjs loader -> main.js from FAT SDCard
```

Both paths run the same firmware code; only the script source changes.

## Boundaries

The live HTTP server is a demo interface for a trusted local lab network.
It intentionally evaluates whatever JavaScript is posted to `/run`.
Keep it behind a private network while experimenting.

The C helpers should stay generic. Demoscene acceleration belongs in
bulk primitives such as `fillRect` and scaled blitters, not in
effect-specific C implementations.
