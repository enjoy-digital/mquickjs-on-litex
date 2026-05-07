// This file is Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef LITEX_MQJS_CONFIG_H
#define LITEX_MQJS_CONFIG_H

/* Size of the mquickjs memory arena (JS heap). The JS engine allocates
 * everything inside this buffer via its own allocator — the C library
 * malloc() is not used. Sized for comfortable runs of the demos, the
 * REPL parser, and a few kilobytes of user script. Fits easily
 * inside litex_sim's default 16 MiB main_ram.
 *
 * Override at build time with `./make.py sim --heap-size <bytes>` or
 * `make -C firmware HEAP_SIZE=<bytes>`. The firmware Makefile forwards
 * that as `-DLITEX_MQJS_HEAP_SIZE=<bytes>`, which wins over the default
 * below thanks to the #ifndef guard. Useful for showing how tight the
 * engine can run. */
#ifndef LITEX_MQJS_HEAP_SIZE
#define LITEX_MQJS_HEAP_SIZE (1u << 20)   /* 1 MiB */
#endif

/* Maximum length of a REPL input line. Multi-line input is not
 * supported — one expression per line. */
#ifndef LITEX_MQJS_LINE_MAX
#define LITEX_MQJS_LINE_MAX 1024
#endif

/* Keep the default demo log short. Define this to 1 when debugging
 * allocator/GC behaviour. */
#ifndef LITEX_MQJS_DUMP_MEMORY
#define LITEX_MQJS_DUMP_MEMORY 0
#endif

/* Marker printed when the embedded script has finished executing.
 * The test harness scrapes UART output for this string. */
#define LITEX_MQJS_DONE_MARKER "[mqjs] done"
#define LITEX_MQJS_FAIL_MARKER "[mqjs] fail"

#endif
