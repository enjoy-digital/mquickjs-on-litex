// Port glue between mquickjs and LiteX: implementations of the C
// functions referenced by the embedded stdlib (see mqjs_stdlib_litex.c).
//
// Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
// SPDX-License-Identifier: BSD-2-Clause

#include <stdint.h>
#include <stdarg.h>
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

static JSWriteFunc *mqjs_print_func;

void mqjs_set_print_func(JSWriteFunc *write_func)
{
    mqjs_print_func = write_func;
}

static void mqjs_print_write(const void *buf, size_t len)
{
    const char *p = (const char *)buf;

    if (mqjs_print_func != NULL) {
        mqjs_print_func(NULL, buf, len);
        return;
    }

    for (size_t i = 0; i < len; i++)
        putchar(p[i]);
}

static void mqjs_print_char(char c)
{
    mqjs_print_write(&c, 1);
}

/* ------------------------------------------------------------------ */
/* print / console.log                                                */
/* ------------------------------------------------------------------ */

JSValue js_print(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    (void)this_val;
    for (int i = 0; i < argc; i++) {
        if (i != 0)
            mqjs_print_char(' ');
        JSValue v = argv[i];
        if (JS_IsString(ctx, v)) {
            JSCStringBuf sb;
            size_t len;
            const char *s = JS_ToCStringLen(ctx, &len, v, &sb);
            mqjs_print_write(s, len);
        } else {
            JS_PrintValueF(ctx, argv[i], JS_DUMP_LONG);
        }
    }
    mqjs_print_char('\n');
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

JSValue js_litex_clock_frequency(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    (void)this_val; (void)argc; (void)argv;
    return JS_NewUint32(ctx, CONFIG_CLOCK_FREQUENCY);
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

static void fs_error(char *err, size_t err_size, const char *fmt, ...)
{
    va_list ap;

    if (err == NULL || err_size == 0)
        return;

    va_start(ap, fmt);
    vsnprintf(err, err_size, fmt, ap);
    va_end(ap);
}

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

int mqjs_fs_available(void)
{
    return 1;
}

int mqjs_fs_read_file(const char *path, const char **data, size_t *len,
                      char *err, size_t err_size)
{
    FRESULT fr;
    FIL file;
    UINT br;
    FSIZE_t size;

    fr = ensure_fatfs();
    if (fr != FR_OK) {
        fs_error(err, err_size, "FatFS mount failed (%d)", fr);
        return -1;
    }

    fr = f_open(&file, path, FA_READ);
    if (fr != FR_OK) {
        fs_error(err, err_size, "cannot open %s (%d)", path, fr);
        return -1;
    }

    size = f_size(&file);
    if (size > sizeof(file_buf) - 1) {
        f_close(&file);
        fs_error(err, err_size, "%s is too large (%lu bytes)",
                 path, (unsigned long)size);
        return -1;
    }

    fr = f_read(&file, file_buf, (UINT)size, &br);
    f_close(&file);
    if (fr != FR_OK) {
        fs_error(err, err_size, "read failed for %s (%d)", path, fr);
        return -1;
    }
    if (br != (UINT)size) {
        fs_error(err, err_size, "short read for %s", path);
        return -1;
    }
    file_buf[size] = 0;
    *data = file_buf;
    *len  = (size_t)size;

    return 0;
}

int mqjs_fs_write_file(const char *path, const char *data, size_t len,
                       char *err, size_t err_size)
{
    FRESULT fr;
    FIL file;
    UINT bw;

    fr = ensure_fatfs();
    if (fr != FR_OK) {
        fs_error(err, err_size, "FatFS mount failed (%d)", fr);
        return -1;
    }

    fr = f_open(&file, path, FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK) {
        fs_error(err, err_size, "cannot open %s for write (%d)", path, fr);
        return -1;
    }

    fr = f_write(&file, data, (UINT)len, &bw);
    f_close(&file);
    if (fr != FR_OK) {
        fs_error(err, err_size, "write failed for %s (%d)", path, fr);
        return -1;
    }
    if (bw != (UINT)len) {
        fs_error(err, err_size, "short write for %s", path);
        return -1;
    }

    return 0;
}
#else
int mqjs_fs_available(void)
{
    return 0;
}

int mqjs_fs_read_file(const char *path, const char **data, size_t *len,
                      char *err, size_t err_size)
{
    (void)path;
    (void)data;
    (void)len;
    if (err != NULL && err_size != 0)
        snprintf(err, err_size, "SDCard support is not present in this SoC");
    return -1;
}

int mqjs_fs_write_file(const char *path, const char *data, size_t len,
                       char *err, size_t err_size)
{
    (void)path;
    (void)data;
    (void)len;
    if (err != NULL && err_size != 0)
        snprintf(err, err_size, "SDCard support is not present in this SoC");
    return -1;
}
#endif

JSValue js_litex_read_file(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    (void)this_val;
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "readFile(path)");

    JSCStringBuf sb;
    size_t len;
    const char *path = JS_ToCStringLen(ctx, &len, argv[0], &sb);
    const char *data;
    size_t data_len;
    char err[96];

    if (!path)
        return JS_EXCEPTION;
    (void)len;
    if (mqjs_fs_read_file(path, &data, &data_len, err, sizeof(err)) != 0)
        return JS_ThrowError(ctx, JS_CLASS_ERROR, "%s", err);

    return JS_NewStringLen(ctx, data, data_len);
}

JSValue js_litex_write_file(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    (void)this_val;
    if (argc < 2)
        return JS_ThrowTypeError(ctx, "writeFile(path, data)");

    JSCStringBuf path_sb;
    JSCStringBuf data_sb;
    size_t path_len;
    size_t data_len;
    const char *path = JS_ToCStringLen(ctx, &path_len, argv[0], &path_sb);
    const char *data = JS_ToCStringLen(ctx, &data_len, argv[1], &data_sb);
    char err[96];

    if (!path || !data)
        return JS_EXCEPTION;
    (void)path_len;
    if (mqjs_fs_write_file(path, data, data_len, err, sizeof(err)) != 0)
        return JS_ThrowError(ctx, JS_CLASS_ERROR, "%s", err);

    return JS_UNDEFINED;
}

JSValue js_litex_load(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    (void)this_val;
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "load(path)");

    JSCStringBuf sb;
    size_t path_len;
    const char *path = JS_ToCStringLen(ctx, &path_len, argv[0], &sb);
    const char *src;
    size_t src_len;
    char err[96];

    if (!path)
        return JS_EXCEPTION;
    (void)path_len;

    if (mqjs_fs_read_file(path, &src, &src_len, err, sizeof(err)) != 0)
        return JS_ThrowError(ctx, JS_CLASS_ERROR, "%s", err);

    return JS_Eval(ctx, src, src_len, path, 0);
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
    ((VIDEO_FRAMEBUFFER_DEPTH == 16) || (VIDEO_FRAMEBUFFER_DEPTH == 32))
#define MQJS_HAS_FRAMEBUFFER 1
#else
#define MQJS_HAS_FRAMEBUFFER 0
#endif

#if MQJS_HAS_FRAMEBUFFER
#if VIDEO_FRAMEBUFFER_DEPTH == 16
typedef uint16_t framebuffer_pixel_t;
#else
typedef uint32_t framebuffer_pixel_t;
#endif

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
        VIDEO_FRAMEBUFFER_HRES * VIDEO_FRAMEBUFFER_VRES *
        (VIDEO_FRAMEBUFFER_DEPTH / 8));
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

static framebuffer_pixel_t framebuffer_color(uint32_t rgb)
{
#if VIDEO_FRAMEBUFFER_DEPTH == 16
    uint32_t r = (rgb >> 19) & 0x1f;
    uint32_t g = (rgb >> 10) & 0x3f;
    uint32_t b = (rgb >>  3) & 0x1f;

    return (framebuffer_pixel_t)((r << 11) | (g << 5) | b);
#else
    uint32_t r = (rgb >> 16) & 0xff;
    uint32_t g = (rgb >>  8) & 0xff;
    uint32_t b = (rgb >>  0) & 0xff;

    return (framebuffer_pixel_t)((b << 16) | (g << 8) | r);
#endif
}

static framebuffer_pixel_t *framebuffer_pixels(void)
{
    return (framebuffer_pixel_t *)(uintptr_t)VIDEO_FRAMEBUFFER_BASE;
}

static void framebuffer_copy_pixels(framebuffer_pixel_t *dst,
                                    const framebuffer_pixel_t *src,
                                    uint32_t count)
{
    for (uint32_t i = 0; i < count; i++)
        dst[i] = src[i];
}

static int framebuffer_get_typed_array(JSContext *ctx, JSValue obj,
                                       int expected_class_id,
                                       uint32_t byte_length_min,
                                       const void **data)
{
    uint8_t *buf;
    uint32_t byte_length;
    int class_id;

    if (JS_GetClassID(ctx, obj) != expected_class_id)
        return 0;
    if (JS_GetTypedArrayBuffer(ctx, &buf, &byte_length, &class_id, obj))
        return -1;
    if (class_id != expected_class_id)
        return 0;
    if (byte_length < byte_length_min) {
        JS_ThrowRangeError(ctx, "framebuffer buffer is too small");
        return -1;
    }

    *data = buf;
    return 1;
}

static int framebuffer_get_uint8_pixels(JSContext *ctx, JSValue obj,
                                        uint32_t width, uint32_t height,
                                        const uint8_t **pixels)
{
    const void *data;
    int ret;

    ret = framebuffer_get_typed_array(ctx, obj, JS_CLASS_UINT8_ARRAY,
        width * height, &data);
    if (ret > 0)
        *pixels = (const uint8_t *)data;
    return ret;
}

static int framebuffer_get_uint32_pixels(JSContext *ctx, JSValue obj,
                                         uint32_t width, uint32_t height,
                                         const uint32_t **pixels)
{
    const void *data;
    int ret;

    ret = framebuffer_get_typed_array(ctx, obj, JS_CLASS_UINT32_ARRAY,
        width * height * sizeof(uint32_t), &data);
    if (ret > 0)
        *pixels = (const uint32_t *)data;
    return ret;
}

static int framebuffer_check_rect(uint32_t x, uint32_t y,
                                  uint32_t width, uint32_t height)
{
    return x <= VIDEO_FRAMEBUFFER_HRES &&
           y <= VIDEO_FRAMEBUFFER_VRES &&
           width  <= VIDEO_FRAMEBUFFER_HRES - x &&
           height <= VIDEO_FRAMEBUFFER_VRES - y;
}

static int framebuffer_get_region(JSContext *ctx, int argc, JSValue *argv,
                                  int first_arg,
                                  uint32_t width, uint32_t height,
                                  uint32_t *region_x, uint32_t *region_y,
                                  uint32_t *region_width,
                                  uint32_t *region_height)
{
    *region_x      = 0;
    *region_y      = 0;
    *region_width  = width;
    *region_height = height;

    if (argc <= first_arg)
        return 0;
    if (argc < first_arg + 4) {
        JS_ThrowTypeError(ctx, "dirty region requires x, y, width, height");
        return -1;
    }
    if (JS_ToUint32(ctx, region_x, argv[first_arg + 0]))
        return -1;
    if (JS_ToUint32(ctx, region_y, argv[first_arg + 1]))
        return -1;
    if (JS_ToUint32(ctx, region_width, argv[first_arg + 2]))
        return -1;
    if (JS_ToUint32(ctx, region_height, argv[first_arg + 3]))
        return -1;
    if (*region_x > width || *region_y > height ||
        *region_width  > width  - *region_x ||
        *region_height > height - *region_y) {
        JS_ThrowRangeError(ctx, "dirty region outside source buffer");
        return -1;
    }
    return 0;
}

static void framebuffer_expand_pixel(framebuffer_pixel_t *dst,
                                     framebuffer_pixel_t pixel,
                                     uint32_t scale)
{
    switch (scale) {
    case 1:
        dst[0] = pixel;
        break;
    case 2:
        dst[0] = pixel;
        dst[1] = pixel;
        break;
    case 4:
        dst[0] = pixel;
        dst[1] = pixel;
        dst[2] = pixel;
        dst[3] = pixel;
        break;
    case 8:
        dst[0] = pixel;
        dst[1] = pixel;
        dst[2] = pixel;
        dst[3] = pixel;
        dst[4] = pixel;
        dst[5] = pixel;
        dst[6] = pixel;
        dst[7] = pixel;
        break;
    default:
        for (uint32_t i = 0; i < scale; i++)
            dst[i] = pixel;
        break;
    }
}

static void framebuffer_repeat_scaled_row(framebuffer_pixel_t *dst,
                                          uint32_t dst_stride,
                                          uint32_t width, uint32_t scale)
{
    uint32_t count = width * scale;

    for (uint32_t y = 1; y < scale; y++)
        framebuffer_copy_pixels(dst + y * dst_stride, dst, count);
}

static void framebuffer_blit_scale_u32(const uint32_t *pixels,
                                       uint32_t width,
                                       uint32_t dst_x, uint32_t dst_y,
                                       uint32_t scale,
                                       uint32_t region_x, uint32_t region_y,
                                       uint32_t region_width,
                                       uint32_t region_height)
{
    framebuffer_pixel_t *fb = framebuffer_pixels();

    for (uint32_t row = 0; row < region_height; row++) {
        uint32_t src = (region_y + row) * width + region_x;
        framebuffer_pixel_t *dst = fb +
            (dst_y + (region_y + row) * scale) * VIDEO_FRAMEBUFFER_HRES +
            dst_x + region_x * scale;

        for (uint32_t col = 0; col < region_width; col++)
            framebuffer_expand_pixel(dst + col * scale,
                framebuffer_color(pixels[src + col]), scale);

        framebuffer_repeat_scaled_row(dst, VIDEO_FRAMEBUFFER_HRES,
            region_width, scale);
    }
}

static void framebuffer_blit_indexed_scale_u8(const uint8_t *indexes,
                                              const uint32_t *palette,
                                              uint32_t palette_offset,
                                              uint32_t width,
                                              uint32_t dst_x,
                                              uint32_t dst_y,
                                              uint32_t scale,
                                              uint32_t region_x,
                                              uint32_t region_y,
                                              uint32_t region_width,
                                              uint32_t region_height)
{
    framebuffer_pixel_t native_palette[256];
    framebuffer_pixel_t *fb = framebuffer_pixels();

    for (uint32_t i = 0; i < 256; i++)
        native_palette[i] = framebuffer_color(palette[i]);

    for (uint32_t row = 0; row < region_height; row++) {
        uint32_t src = (region_y + row) * width + region_x;
        framebuffer_pixel_t *dst = fb +
            (dst_y + (region_y + row) * scale) * VIDEO_FRAMEBUFFER_HRES +
            dst_x + region_x * scale;

        for (uint32_t col = 0; col < region_width; col++) {
            uint32_t index = (indexes[src + col] + palette_offset) & 0xff;
            framebuffer_expand_pixel(dst + col * scale, native_palette[index],
                scale);
        }

        framebuffer_repeat_scaled_row(dst, VIDEO_FRAMEBUFFER_HRES,
            region_width, scale);
    }
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
    framebuffer_pixel_t pixel;
    framebuffer_pixel_t *fb = framebuffer_pixels();
    uint32_t count = VIDEO_FRAMEBUFFER_HRES * VIDEO_FRAMEBUFFER_VRES;

    if (argc > 0 && JS_ToUint32(ctx, &color, argv[0]))
        return JS_EXCEPTION;
    pixel = framebuffer_color(color);

    framebuffer_start();
    for (uint32_t i = 0; i < count; i++)
        fb[i] = pixel;
    return JS_UNDEFINED;
#else
    (void)argc; (void)argv;
    return framebuffer_unavailable(ctx);
#endif
}

JSValue js_framebuffer_fill_rect(JSContext *ctx, JSValue *this_val,
                                 int argc, JSValue *argv)
{
    (void)this_val;
#if MQJS_HAS_FRAMEBUFFER
    if (argc < 5)
        return JS_ThrowTypeError(ctx, "fillRect(x, y, width, height, color)");

    uint32_t x, y, width, height, color;
    if (JS_ToUint32(ctx, &x, argv[0]))
        return JS_EXCEPTION;
    if (JS_ToUint32(ctx, &y, argv[1]))
        return JS_EXCEPTION;
    if (JS_ToUint32(ctx, &width, argv[2]))
        return JS_EXCEPTION;
    if (JS_ToUint32(ctx, &height, argv[3]))
        return JS_EXCEPTION;
    if (JS_ToUint32(ctx, &color, argv[4]))
        return JS_EXCEPTION;

    if (width == 0 || height == 0)
        return JS_UNDEFINED;
    if (!framebuffer_check_rect(x, y, width, height))
        return JS_ThrowRangeError(ctx, "fillRect rectangle outside framebuffer");

    framebuffer_start();
    framebuffer_pixel_t pixel = framebuffer_color(color);
    framebuffer_pixel_t *fb = framebuffer_pixels();
    framebuffer_pixel_t *first = fb + y * VIDEO_FRAMEBUFFER_HRES + x;

    for (uint32_t col = 0; col < width; col++)
        first[col] = pixel;
    for (uint32_t row = 1; row < height; row++)
        framebuffer_copy_pixels(first + row * VIDEO_FRAMEBUFFER_HRES,
            first, width);
    return JS_UNDEFINED;
#else
    (void)argc; (void)argv;
    return framebuffer_unavailable(ctx);
#endif
}

JSValue js_framebuffer_copy_rect(JSContext *ctx, JSValue *this_val,
                                 int argc, JSValue *argv)
{
    (void)this_val;
#if MQJS_HAS_FRAMEBUFFER
    if (argc < 6) {
        return JS_ThrowTypeError(ctx,
            "copyRect(srcX, srcY, width, height, dstX, dstY)");
    }

    uint32_t src_x, src_y, width, height, dst_x, dst_y;
    if (JS_ToUint32(ctx, &src_x, argv[0]))
        return JS_EXCEPTION;
    if (JS_ToUint32(ctx, &src_y, argv[1]))
        return JS_EXCEPTION;
    if (JS_ToUint32(ctx, &width, argv[2]))
        return JS_EXCEPTION;
    if (JS_ToUint32(ctx, &height, argv[3]))
        return JS_EXCEPTION;
    if (JS_ToUint32(ctx, &dst_x, argv[4]))
        return JS_EXCEPTION;
    if (JS_ToUint32(ctx, &dst_y, argv[5]))
        return JS_EXCEPTION;

    if (width == 0 || height == 0)
        return JS_UNDEFINED;
    if (!framebuffer_check_rect(src_x, src_y, width, height) ||
        !framebuffer_check_rect(dst_x, dst_y, width, height)) {
        return JS_ThrowRangeError(ctx, "copyRect rectangle outside framebuffer");
    }

    framebuffer_start();
    framebuffer_pixel_t *fb = framebuffer_pixels();
    size_t size = width * sizeof(framebuffer_pixel_t);

    if (dst_y > src_y) {
        for (uint32_t row = height; row != 0; row--) {
            uint32_t r = row - 1;
            memmove(
                fb + (dst_y + r) * VIDEO_FRAMEBUFFER_HRES + dst_x,
                fb + (src_y + r) * VIDEO_FRAMEBUFFER_HRES + src_x,
                size);
        }
    } else {
        for (uint32_t row = 0; row < height; row++) {
            memmove(
                fb + (dst_y + row) * VIDEO_FRAMEBUFFER_HRES + dst_x,
                fb + (src_y + row) * VIDEO_FRAMEBUFFER_HRES + src_x,
                size);
        }
    }
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

    framebuffer_start();
    framebuffer_pixel_t *fb = framebuffer_pixels();
    const uint32_t *pixels;
    int fast_path = framebuffer_get_uint32_pixels(ctx, argv[0], width, height, &pixels);
    if (fast_path < 0)
        return JS_EXCEPTION;
    if (fast_path) {
        for (uint32_t row = 0; row < height; row++) {
            uint32_t dst = (y + row) * VIDEO_FRAMEBUFFER_HRES + x;
            uint32_t src = row * width;
            for (uint32_t col = 0; col < width; col++)
                fb[dst + col] = framebuffer_color(pixels[src + col]);
        }
        return JS_UNDEFINED;
    }

    JSValue length_val = JS_GetPropertyStr(ctx, argv[0], "length");
    if (JS_IsException(length_val))
        return length_val;
    uint32_t length;
    if (JS_ToUint32(ctx, &length, length_val))
        return JS_EXCEPTION;
    if (length < width * height)
        return JS_ThrowRangeError(ctx, "blit buffer is too small");

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
            fb[dst + col] = framebuffer_color(pixel);
        }
    }
    return JS_UNDEFINED;
#else
    (void)argc; (void)argv;
    return framebuffer_unavailable(ctx);
#endif
}

JSValue js_framebuffer_blit_scale(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    (void)this_val;
#if MQJS_HAS_FRAMEBUFFER
    if (argc < 6)
        return JS_ThrowTypeError(ctx, "blitScale(buffer, width, height, x, y, scale)");

    uint32_t width, height, x, y, scale;
    if (JS_ToUint32(ctx, &width, argv[1]))
        return JS_EXCEPTION;
    if (JS_ToUint32(ctx, &height, argv[2]))
        return JS_EXCEPTION;
    if (JS_ToUint32(ctx, &x, argv[3]))
        return JS_EXCEPTION;
    if (JS_ToUint32(ctx, &y, argv[4]))
        return JS_EXCEPTION;
    if (JS_ToUint32(ctx, &scale, argv[5]))
        return JS_EXCEPTION;

    if (width == 0 || height == 0 || scale == 0)
        return JS_UNDEFINED;
    if (x >= VIDEO_FRAMEBUFFER_HRES || y >= VIDEO_FRAMEBUFFER_VRES ||
        width > (VIDEO_FRAMEBUFFER_HRES - x) / scale ||
        height > (VIDEO_FRAMEBUFFER_VRES - y) / scale) {
        return JS_ThrowRangeError(ctx, "scaled blit rectangle outside framebuffer");
    }

    uint32_t region_x, region_y, region_width, region_height;
    if (framebuffer_get_region(ctx, argc, argv, 6, width, height,
        &region_x, &region_y, &region_width, &region_height)) {
        return JS_EXCEPTION;
    }

    framebuffer_start();
    const uint32_t *pixels;
    int fast_path = framebuffer_get_uint32_pixels(ctx, argv[0], width, height, &pixels);
    if (fast_path < 0)
        return JS_EXCEPTION;
    if (fast_path) {
        framebuffer_blit_scale_u32(pixels, width, x, y, scale, region_x,
            region_y, region_width, region_height);
        return JS_UNDEFINED;
    }

    JSValue length_val = JS_GetPropertyStr(ctx, argv[0], "length");
    if (JS_IsException(length_val))
        return length_val;
    uint32_t length;
    if (JS_ToUint32(ctx, &length, length_val))
        return JS_EXCEPTION;
    if (length < width * height)
        return JS_ThrowRangeError(ctx, "blitScale buffer is too small");

    framebuffer_pixel_t *fb = framebuffer_pixels();
    for (uint32_t row = 0; row < region_height; row++) {
        for (uint32_t col = 0; col < region_width; col++) {
            uint32_t src = (region_y + row) * width + region_x + col;
            JSValue pixel_val = JS_GetPropertyUint32(ctx, argv[0], src);
            uint32_t pixel;
            if (JS_IsException(pixel_val))
                return pixel_val;
            if (JS_ToUint32(ctx, &pixel, pixel_val))
                return JS_EXCEPTION;

            uint32_t dst_x = x + (region_x + col) * scale;
            uint32_t dst_y = y + (region_y + row) * scale;
            for (uint32_t sy = 0; sy < scale; sy++) {
                uint32_t dst = (dst_y + sy) * VIDEO_FRAMEBUFFER_HRES + dst_x;
                for (uint32_t sx = 0; sx < scale; sx++)
                    fb[dst + sx] = framebuffer_color(pixel);
            }
        }
    }
    return JS_UNDEFINED;
#else
    (void)argc; (void)argv;
    return framebuffer_unavailable(ctx);
#endif
}

JSValue js_framebuffer_blit_indexed_scale(JSContext *ctx, JSValue *this_val,
                                          int argc, JSValue *argv)
{
    (void)this_val;
#if MQJS_HAS_FRAMEBUFFER
    if (argc < 7) {
        return JS_ThrowTypeError(ctx,
            "blitIndexedScale(indexes, palette, width, height, x, y, scale)");
    }

    uint32_t width, height, x, y, scale, palette_offset = 0;
    if (JS_ToUint32(ctx, &width, argv[2]))
        return JS_EXCEPTION;
    if (JS_ToUint32(ctx, &height, argv[3]))
        return JS_EXCEPTION;
    if (JS_ToUint32(ctx, &x, argv[4]))
        return JS_EXCEPTION;
    if (JS_ToUint32(ctx, &y, argv[5]))
        return JS_EXCEPTION;
    if (JS_ToUint32(ctx, &scale, argv[6]))
        return JS_EXCEPTION;
    if (argc > 7 && JS_ToUint32(ctx, &palette_offset, argv[7]))
        return JS_EXCEPTION;

    if (width == 0 || height == 0 || scale == 0)
        return JS_UNDEFINED;
    if (x >= VIDEO_FRAMEBUFFER_HRES || y >= VIDEO_FRAMEBUFFER_VRES ||
        width > (VIDEO_FRAMEBUFFER_HRES - x) / scale ||
        height > (VIDEO_FRAMEBUFFER_VRES - y) / scale) {
        return JS_ThrowRangeError(ctx, "indexed blit rectangle outside framebuffer");
    }

    uint32_t region_x, region_y, region_width, region_height;
    if (framebuffer_get_region(ctx, argc, argv, 8, width, height,
        &region_x, &region_y, &region_width, &region_height)) {
        return JS_EXCEPTION;
    }

    const uint8_t *indexes;
    int indexes_ok = framebuffer_get_uint8_pixels(ctx, argv[0], width, height, &indexes);
    if (indexes_ok < 0)
        return JS_EXCEPTION;
    if (!indexes_ok)
        return JS_ThrowTypeError(ctx, "indexes must be a Uint8Array");

    const uint32_t *palette;
    int palette_ok = framebuffer_get_uint32_pixels(ctx, argv[1], 256, 1, &palette);
    if (palette_ok < 0)
        return JS_EXCEPTION;
    if (!palette_ok)
        return JS_ThrowTypeError(ctx, "palette must be a Uint32Array");

    framebuffer_start();
    framebuffer_blit_indexed_scale_u8(indexes, palette, palette_offset,
        width, x, y, scale, region_x, region_y, region_width,
        region_height);
    return JS_UNDEFINED;
#else
    (void)argc; (void)argv;
    return framebuffer_unavailable(ctx);
#endif
}
