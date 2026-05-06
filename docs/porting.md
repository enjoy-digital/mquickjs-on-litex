<!--
Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
SPDX-License-Identifier: BSD-2-Clause
-->

# How mquickjs is wired into LiteX

This is the tour of the port. Read `docs/building.md` for the
mechanical steps; this page focuses on *why* the code looks the way it
does.

## The mquickjs C API shape

mquickjs is deliberately allocator-free: you hand it a buffer at
context-creation time and the engine allocates everything inside it.

```c
static uint8_t mqjs_heap[1 << 20] __attribute__((aligned(16)));

JSContext *ctx = JS_NewContext(mqjs_heap, sizeof(mqjs_heap), &js_stdlib);
```

Two consequences for the port:

1. **No libc malloc** — we can build the firmware against picolibc in
   `libc-mode=full` without worrying about heap growth. The 1 MiB
   buffer is the only significant allocation and it lives in `.bss`.
2. **JSValue identities move** — the GC is compacting. Anything that
   holds a `JSValue` across a mquickjs call has to go through
   `JS_PushGCRef` / `JS_PopGCRef`. Our main-loop doesn't hold any
   `JSValue` across calls, but future hardware bindings must.

## Host-side stdlib generation

`firmware/mqjs_stdlib_litex.c` is a trimmed copy of upstream
`mqjs_stdlib.c`: no `setTimeout`/`clearTimeout`, plus a small `litex`
object with LED/switch/button, identifier, scratch-register, raw CSR,
and optional SDCard file-loading helpers.

At build time it is compiled **natively** as a host tool — see the
`$(STDLIB_GEN)` rule in the Makefile — and then executed twice to
produce two C headers:

- `build/mquickjs_atom.h` — interned atom table, `#include`d by
  `mquickjs.c`
- `build/mqjs_stdlib.h`   — the `js_stdlib` struct referenced by
  `JS_NewContext`

We pass the `-m32` flag at runtime so the table layout matches the
rv32 target regardless of the host word size.

Two build details are important when extending the stdlib:

- `mquickjs_build.c` uses `ATOM_ALIGN` for the ROM atom table. If you
  add enough global properties to the stdlib generator, the old
  alignment can corrupt keyword atom offsets and embedded source will
  fail with errors such as `[mtag]: unexpected character in
  expression`. The pinned submodule uses a larger alignment for this
  LiteX stdlib.
- A host-side `make mqjs` in the submodule also generates
  `third_party/mquickjs/mquickjs_atom.h`. The firmware Makefile removes
  that stale host header before compiling `mquickjs.o`, so the target
  always uses `firmware/build/mquickjs_atom.h` generated for the
  embedded stdlib.

## Firmware entry and UART plumbing

`firmware/main.c` is short. In order:

1. Mask all interrupts, enable global IE, call `uart_init()` from
   libbase.
2. Print a banner (via `printf`, routed through picolibc →
   `litex_putc` → UART ringbuffer + ISR).
3. Create a `JSContext` over `mqjs_heap`.
4. Detect whether `user_script[]` is source or bytecode and dispatch
   to either `JS_Eval` or `JS_LoadBytecode`.
5. Emit `[mqjs] done` or `[mqjs] fail` and halt.

The log sink installed via `JS_SetLogFunc` routes mquickjs'
pretty-printer output back to the UART, so `console.log(obj)` prints
the full object tree rather than `[object Object]`.

## Linker script caveat

`firmware/linker.ld` places every section in `main_ram`. The integrated
SRAM region defined by LiteX (8 KiB) is too small to host the JS heap.

**Gotcha worth calling out**: the `.data` output section uses
`.data : ALIGN(16)` rather than `. = ALIGN(16); .data : { … }`. The
latter puts the alignment pad *inside* the section before `_fdata`, so
`LOADADDR(.data) != _fdata`. crt0 then copies `SIZEOF(.data)` bytes
starting from `_fdata_rom` into `_fdata`, which silently clobbers the
first words of `.data` — most visibly the picolibc `__stdio` FILE
object, so `puts()` and `printf()` produce no output. A 4-byte offset
in a linker script is easy to write and can take a while to find.

## Memory size choices

| Setting                    | Default    | Notes                                          |
|----------------------------|------------|------------------------------------------------|
| `LITEX_MQJS_HEAP_SIZE`     | 1 MiB      | Enough for mandelbrot + REPL parser + scripts  |
| `integrated-main-ram-size` | 16 MiB     | Comfortable for firmware + heap + stack        |
| UART baudrate              | 115200     | Default for LiteX; change in gen_soc.py if needed |

On a real board with external DRAM, bumping `LITEX_MQJS_HEAP_SIZE` to
8 or 16 MiB lets you run the upstream micro-benchmarks (`tests/microbench.js`).

## Embedding a script blob safely

The mquickjs parser does one-byte lookahead past `src_len`, even though
`JS_Eval` takes an explicit length. If the byte at `src[src_len]`
happens to be a visible ASCII character, you'll see a phantom syntax
error on a line one past EOF — e.g. `SyntaxError: unexpected character
in expression at user_script.js:30:1` for a 29-line source.

`tools/embed_script.py` guards against this by always writing one
extra `0x00` byte after the last source byte; `user_script_len` is
still the original source size. Upstream does the same implicitly by
allocating `malloc(len + 1)` and setting `buf[len] = '\0'`.

## Optional filesystem bindings

`litex.readFile(path)` and `litex.load(path)` are available when the
SoC exposes `CSR_SDCARD_BASE` or `CSR_SPISDCARD_BASE`. The binding
mounts the FAT volume on first use, reads files up to 64 KiB into a
static buffer, and evaluates source with `JS_Eval`. On a simulation or
minimal board build without SDCard support, the functions throw a
JavaScript exception instead of failing at link time.

## Things intentionally not ported

- `setTimeout` / `clearTimeout` — no event loop. If you add a cooperative
  scheduler you can lift the upstream implementation verbatim; it's
  ~50 lines and uses the same `performance.now()` clock we already
  expose via `now_ms()`.
- `readline` / terminal colors — the REPL is line-based and colour-free
  to keep the firmware small. The host `mqjs` has the nice syntax
  highlighting.
