// Port glue between mquickjs and LiteX: implementations of the C
// functions referenced by the embedded stdlib (see mqjs_stdlib_litex.c).
//
// Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
// SPDX-License-Identifier: BSD-2-Clause

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <irq.h>
#include <libbase/console.h>
#include <libbase/uart.h>
#include <generated/csr.h>

#include "mquickjs.h"
#include "mqjs_port.h"

/* ------------------------------------------------------------------ */
/* Log sink — route mquickjs output to the LiteX UART.                */
/* ------------------------------------------------------------------ */

void mqjs_log_func(void *opaque, const void *buf, size_t buf_len)
{
    (void)opaque;
    const char *p = (const char *)buf;
    for (size_t i = 0; i < buf_len; i++)
        putchar(p[i]);
}

/* ------------------------------------------------------------------ */
/* print / console.log                                                */
/* ------------------------------------------------------------------ */

JSValue js_print(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    (void)this_val;
    for (int i = 0; i < argc; i++) {
        if (i != 0)
            putchar(' ');
        JSValue v = argv[i];
        if (JS_IsString(ctx, v)) {
            JSCStringBuf sb;
            size_t len;
            const char *s = JS_ToCStringLen(ctx, &len, v, &sb);
            for (size_t j = 0; j < len; j++)
                putchar(s[j]);
        } else {
            JS_PrintValueF(ctx, argv[i], JS_DUMP_LONG);
        }
    }
    putchar('\n');
    return JS_UNDEFINED;
}

/* ------------------------------------------------------------------ */
/* gc                                                                 */
/* ------------------------------------------------------------------ */

JSValue js_gc(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    (void)this_val; (void)argc; (void)argv;
    JS_GC(ctx);
    return JS_UNDEFINED;
}

/* ------------------------------------------------------------------ */
/* Time                                                               */
/* ------------------------------------------------------------------ */

/* LiteX exposes a uptime counter when the SoC is built with
 * --timer-uptime. In litex_sim it is enabled by default. The register
 * ticks at the system clock (CONFIG_CLOCK_FREQUENCY). We return ms. */

static int64_t now_ms(void)
{
#if defined(CSR_TIMER0_UPTIME_CYCLES_ADDR)
    timer0_uptime_latch_write(1);
    uint64_t ticks = timer0_uptime_cycles_read();
    return (int64_t)(ticks / (CONFIG_CLOCK_FREQUENCY / 1000));
#else
    return 0;
#endif
}

JSValue js_date_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    (void)this_val; (void)argc; (void)argv;
    return JS_NewInt64(ctx, now_ms());
}

JSValue js_performance_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    (void)this_val; (void)argc; (void)argv;
    return JS_NewInt64(ctx, now_ms());
}

/* ------------------------------------------------------------------ */
/* litex.* bindings                                                   */
/* ------------------------------------------------------------------ */

JSValue js_litex_set_leds(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    (void)this_val;
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "setLeds requires one argument");
    int n;
    if (JS_ToInt32(ctx, &n, argv[0]))
        return JS_EXCEPTION;
#if defined(CSR_LEDS_BASE)
    leds_out_write((uint32_t)n);
#else
    (void)n;
#endif
    return JS_UNDEFINED;
}

JSValue js_litex_get_switches(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    (void)this_val; (void)argc; (void)argv;
#if defined(CSR_SWITCHES_BASE)
    return JS_NewUint32(ctx, (uint32_t)switches_in_read());
#else
    return JS_NewUint32(ctx, 0);
#endif
}

JSValue js_litex_millis(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    (void)this_val; (void)argc; (void)argv;
    return JS_NewInt64(ctx, now_ms());
}

JSValue js_litex_delay(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    (void)this_val;
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "delay requires one argument (ms)");
    int ms;
    if (JS_ToInt32(ctx, &ms, argv[0]))
        return JS_EXCEPTION;
    if (ms > 0)
        busy_wait((unsigned int)ms);
    return JS_UNDEFINED;
}

JSValue js_litex_csr_read32(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    (void)this_val;
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "csrRead32(addr)");
    uint32_t addr;
    if (JS_ToUint32(ctx, &addr, argv[0]))
        return JS_EXCEPTION;
    volatile uint32_t *p = (volatile uint32_t *)(uintptr_t)addr;
    return JS_NewUint32(ctx, *p);
}

JSValue js_litex_csr_write32(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    (void)this_val;
    if (argc < 2)
        return JS_ThrowTypeError(ctx, "csrWrite32(addr, value)");
    uint32_t addr, val;
    if (JS_ToUint32(ctx, &addr, argv[0]))
        return JS_EXCEPTION;
    if (JS_ToUint32(ctx, &val, argv[1]))
        return JS_EXCEPTION;
    volatile uint32_t *p = (volatile uint32_t *)(uintptr_t)addr;
    *p = val;
    return JS_UNDEFINED;
}

JSValue js_litex_reboot(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    (void)ctx; (void)this_val; (void)argc; (void)argv;
#if defined(CSR_CTRL_BASE)
    ctrl_reset_write(1);
#endif
    for (;;) { /* unreachable */ }
    return JS_UNDEFINED;
}
