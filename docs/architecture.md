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

live_http_index.html
  - browser-side editor UI served by the firmware
  - sends script.js to the board over HTTP

liteeth_lwip.c
  - LiteEth SRAM MAC <-> lwIP netif bridge
  - polling RX, blocking TX
```

The JavaScript heap is static and sized by `LITEX_MQJS_HEAP_SIZE`
(default 1 MiB). mquickjs does not call `malloc()` for JavaScript
objects.

## JavaScript APIs

The project intentionally exposes a small API: `litex.*` for generic
board/CSR access and `framebuffer.*` for video demos. See
[js-api.md](js-api.md) for the binding reference, live `setup()` /
`frame(t)` lifecycle and portability rules.

## Live Browser Demo

With Ethernet and a video framebuffer enabled, the firmware can serve the
live demo page directly from the board:

```text
Browser
  GET  /       -> board-served editor page
  GET  /info   -> target features, framebuffer size, live stats
  POST /run    -> reset mquickjs context, evaluate source
  POST /eval   -> evaluate a small snippet in the active context
  POST /control-> stop, pause, resume or reset the active script
  GET  /load   -> read main.js from SDCard when available
  POST /save   -> write main.js to SDCard when available
```

Two JavaScript runtimes are involved:

```text
Browser JavaScript  -> UI, buttons, HTTP requests
Editor script.js    -> mquickjs on the LiteX CPU
```

The HTTP path uses lwIP in `NO_SYS=1` mode. There is no OS, socket API,
filesystem or dynamic web server. The page source lives in
`firmware/live_http_index.html`; the firmware build embeds it as a
generated C header, and the HTTP parser accepts only the small request
set needed by the demo.

For robustness while iterating, each `/run` request creates a fresh
mquickjs context on an inactive heap. The new context is evaluated and
`setup()` is tested before it replaces the previous context, so a syntax
error or setup-time exception does not stop the animation already
running on the board. If the script defines `frame(t)`, the firmware
keeps calling it from the Ethernet poll loop and reports frame time/FPS
through `/info`. Scripts without `frame(t)` still run once.

The browser utility buttons post short JavaScript snippets that use the
same `litex.*` and `framebuffer.*` APIs as user scripts. The I/O demo is
just another mquickjs script: it waits for switches/buttons when present,
drives LEDs, tests the scratch register and redraws the framebuffer.

When SDCard support is present, the browser can save the current script
as `main.js` and load it back later. `litex.writeFile()` exposes the
same write path to JavaScript.

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
