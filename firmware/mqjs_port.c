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

#if defined(CSR_SDCARD_BASE) || defined(CSR_SPISDCARD_BASE)
#include <libfatfs/ff.h>
#endif
#if defined(CSR_SDCARD_BASE)
#include <liblitesdcard/sdcard.h>
#endif
#if defined(CSR_SPISDCARD_BASE)
#include <liblitesdcard/spisdcard.h>
#endif

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

JSValue js_litex_get_buttons(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    (void)this_val; (void)argc; (void)argv;
#if defined(CSR_BUTTONS_BASE)
    return JS_NewUint32(ctx, (uint32_t)buttons_in_read());
#else
    return JS_NewUint32(ctx, 0);
#endif
}

JSValue js_litex_get_identifier(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    (void)this_val; (void)argc; (void)argv;
#if defined(CONFIG_IDENTIFIER)
    return JS_NewString(ctx, config_identifier_read());
#else
    return JS_NewString(ctx, "LiteX SoC");
#endif
}

JSValue js_litex_get_scratch(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    (void)this_val; (void)argc; (void)argv;
#if defined(CSR_CTRL_SCRATCH_ADDR)
    return JS_NewUint32(ctx, ctrl_scratch_read());
#else
    return JS_NewUint32(ctx, 0);
#endif
}

JSValue js_litex_set_scratch(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    (void)this_val;
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "setScratch(value)");
    uint32_t value;
    if (JS_ToUint32(ctx, &value, argv[0]))
        return JS_EXCEPTION;
#if defined(CSR_CTRL_SCRATCH_ADDR)
    ctrl_scratch_write(value);
#else
    (void)value;
#endif
    return JS_UNDEFINED;
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

/* ------------------------------------------------------------------ */
/* SDCard / FatFS                                                     */
/* ------------------------------------------------------------------ */

#if defined(CSR_SDCARD_BASE) || defined(CSR_SPISDCARD_BASE)
static int fatfs_ready;
static FATFS fatfs;
static char file_buf[64 * 1024 + 1];

static FRESULT ensure_fatfs(void)
{
    FRESULT fr;
    if (fatfs_ready)
        return FR_OK;
#if defined(CSR_SPISDCARD_BASE)
    fatfs_set_ops_spisdcard();
#elif defined(CSR_SDCARD_BASE)
    fatfs_set_ops_sdcard();
#endif
    fr = f_mount(&fatfs, "", 1);
    if (fr == FR_OK)
        fatfs_ready = 1;
    return fr;
}

static JSValue read_file_to_string(JSContext *ctx, const char *path)
{
    FRESULT fr;
    FIL file;
    UINT br;
    FSIZE_t size;

    fr = ensure_fatfs();
    if (fr != FR_OK)
        return JS_ThrowError(ctx, JS_CLASS_ERROR, "FatFS mount failed (%d)", fr);

    fr = f_open(&file, path, FA_READ);
    if (fr != FR_OK)
        return JS_ThrowError(ctx, JS_CLASS_ERROR, "cannot open %s (%d)", path, fr);

    size = f_size(&file);
    if (size > sizeof(file_buf) - 1) {
        f_close(&file);
        return JS_ThrowRangeError(ctx, "%s is too large (%lu bytes)",
                                  path, (unsigned long)size);
    }

    fr = f_read(&file, file_buf, (UINT)size, &br);
    f_close(&file);
    if (fr != FR_OK)
        return JS_ThrowError(ctx, JS_CLASS_ERROR, "read failed for %s (%d)", path, fr);
    if (br != (UINT)size)
        return JS_ThrowError(ctx, JS_CLASS_ERROR, "short read for %s", path);
    file_buf[size] = 0;

    return JS_NewStringLen(ctx, file_buf, (size_t)size);
}
#endif

JSValue js_litex_read_file(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    (void)this_val;
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "readFile(path)");

#if defined(CSR_SDCARD_BASE) || defined(CSR_SPISDCARD_BASE)
    JSCStringBuf sb;
    size_t len;
    const char *path = JS_ToCStringLen(ctx, &len, argv[0], &sb);
    if (!path)
        return JS_EXCEPTION;
    (void)len;
    return read_file_to_string(ctx, path);
#else
    (void)argv;
    return JS_ThrowError(ctx, JS_CLASS_ERROR, "SDCard support is not present in this SoC");
#endif
}

JSValue js_litex_load(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    (void)this_val;
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "load(path)");

#if defined(CSR_SDCARD_BASE) || defined(CSR_SPISDCARD_BASE)
    JSCStringBuf sb;
    size_t path_len;
    const char *path = JS_ToCStringLen(ctx, &path_len, argv[0], &sb);
    if (!path)
        return JS_EXCEPTION;
    (void)path_len;

    JSValue src_val = read_file_to_string(ctx, path);
    if (JS_IsException(src_val))
        return src_val;

    JSCStringBuf src_sb;
    size_t src_len;
    const char *src = JS_ToCStringLen(ctx, &src_len, src_val, &src_sb);
    if (!src)
        return JS_EXCEPTION;

    return JS_Eval(ctx, src, src_len, path, 0);
#else
    (void)argv;
    return JS_ThrowError(ctx, JS_CLASS_ERROR, "SDCard support is not present in this SoC");
#endif
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

/* ------------------------------------------------------------------ */
/* framebuffer.* bindings                                             */
/* ------------------------------------------------------------------ */

#if defined(VIDEO_FRAMEBUFFER_BASE) && defined(VIDEO_FRAMEBUFFER_HRES) && \
    defined(VIDEO_FRAMEBUFFER_VRES) && defined(VIDEO_FRAMEBUFFER_DEPTH) && \
    VIDEO_FRAMEBUFFER_DEPTH == 32
#define MQJS_HAS_FRAMEBUFFER 1
#else
#define MQJS_HAS_FRAMEBUFFER 0
#endif

#if MQJS_HAS_FRAMEBUFFER
static int js_arg_u32(JSContext *ctx, int argc, JSValue *argv, int idx,
                      uint32_t def, uint32_t *value)
{
    if (idx >= argc || JS_IsUndefined(argv[idx])) {
        *value = def;
        return 0;
    }
    return JS_ToUint32(ctx, value, argv[idx]);
}

static void framebuffer_start(void)
{
#if defined(CSR_VIDEO_FRAMEBUFFER_DMA_BASE_ADDR)
    video_framebuffer_dma_base_write(VIDEO_FRAMEBUFFER_BASE);
#endif
#if defined(CSR_VIDEO_FRAMEBUFFER_DMA_LENGTH_ADDR)
    video_framebuffer_dma_length_write(
        VIDEO_FRAMEBUFFER_HRES * VIDEO_FRAMEBUFFER_VRES * 4);
#endif
#if defined(CSR_VIDEO_FRAMEBUFFER_DMA_LOOP_ADDR)
    video_framebuffer_dma_loop_write(1);
#endif
#if defined(CSR_VIDEO_FRAMEBUFFER_DMA_ENABLE_ADDR)
    video_framebuffer_dma_enable_write(1);
#endif
#if defined(CSR_VIDEO_FRAMEBUFFER_VTG_ENABLE_ADDR)
    video_framebuffer_vtg_enable_write(1);
#endif
}

static volatile uint32_t *framebuffer_pixels(void)
{
    return (volatile uint32_t *)(uintptr_t)VIDEO_FRAMEBUFFER_BASE;
}
#else
static JSValue framebuffer_unavailable(JSContext *ctx)
{
    return JS_ThrowError(ctx, JS_CLASS_ERROR,
        "Video framebuffer support is not present in this SoC");
}
#endif

JSValue js_framebuffer_get_width(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    (void)this_val; (void)argc; (void)argv;
#if MQJS_HAS_FRAMEBUFFER
    return JS_NewUint32(ctx, VIDEO_FRAMEBUFFER_HRES);
#else
    return JS_NewUint32(ctx, 0);
#endif
}

JSValue js_framebuffer_get_height(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    (void)this_val; (void)argc; (void)argv;
#if MQJS_HAS_FRAMEBUFFER
    return JS_NewUint32(ctx, VIDEO_FRAMEBUFFER_VRES);
#else
    return JS_NewUint32(ctx, 0);
#endif
}

JSValue js_framebuffer_get_depth(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    (void)this_val; (void)argc; (void)argv;
#if MQJS_HAS_FRAMEBUFFER
    return JS_NewUint32(ctx, VIDEO_FRAMEBUFFER_DEPTH);
#else
    return JS_NewUint32(ctx, 0);
#endif
}

JSValue js_framebuffer_clear(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    (void)this_val;
#if MQJS_HAS_FRAMEBUFFER
    uint32_t color = 0;
    volatile uint32_t *fb = framebuffer_pixels();
    uint32_t count = VIDEO_FRAMEBUFFER_HRES * VIDEO_FRAMEBUFFER_VRES;

    if (argc > 0 && JS_ToUint32(ctx, &color, argv[0]))
        return JS_EXCEPTION;

    framebuffer_start();
    for (uint32_t i = 0; i < count; i++)
        fb[i] = color;
    return JS_UNDEFINED;
#else
    (void)argc; (void)argv;
    return framebuffer_unavailable(ctx);
#endif
}

JSValue js_framebuffer_blit(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    (void)this_val;
#if MQJS_HAS_FRAMEBUFFER
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "blit(buffer[, width, height, x, y])");

    uint32_t width, height, x, y;
    if (js_arg_u32(ctx, argc, argv, 1, VIDEO_FRAMEBUFFER_HRES, &width))
        return JS_EXCEPTION;
    if (js_arg_u32(ctx, argc, argv, 2, VIDEO_FRAMEBUFFER_VRES, &height))
        return JS_EXCEPTION;
    if (js_arg_u32(ctx, argc, argv, 3, 0, &x))
        return JS_EXCEPTION;
    if (js_arg_u32(ctx, argc, argv, 4, 0, &y))
        return JS_EXCEPTION;

    if (width == 0 || height == 0)
        return JS_UNDEFINED;
    if (x >= VIDEO_FRAMEBUFFER_HRES || y >= VIDEO_FRAMEBUFFER_VRES ||
        width > VIDEO_FRAMEBUFFER_HRES - x ||
        height > VIDEO_FRAMEBUFFER_VRES - y) {
        return JS_ThrowRangeError(ctx, "blit rectangle outside framebuffer");
    }

    JSValue length_val = JS_GetPropertyStr(ctx, argv[0], "length");
    if (JS_IsException(length_val))
        return length_val;
    uint32_t length;
    if (JS_ToUint32(ctx, &length, length_val))
        return JS_EXCEPTION;
    if (length < width * height)
        return JS_ThrowRangeError(ctx, "blit buffer is too small");

    framebuffer_start();
    volatile uint32_t *fb = framebuffer_pixels();
    for (uint32_t row = 0; row < height; row++) {
        uint32_t dst = (y + row) * VIDEO_FRAMEBUFFER_HRES + x;
        uint32_t src = row * width;
        for (uint32_t col = 0; col < width; col++) {
            JSValue pixel_val = JS_GetPropertyUint32(ctx, argv[0], src + col);
            uint32_t pixel;
            if (JS_IsException(pixel_val))
                return pixel_val;
            if (JS_ToUint32(ctx, &pixel, pixel_val))
                return JS_EXCEPTION;
            fb[dst + col] = pixel;
        }
    }
    return JS_UNDEFINED;
#else
    (void)argc; (void)argv;
    return framebuffer_unavailable(ctx);
#endif
}
