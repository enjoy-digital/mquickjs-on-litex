// Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef LITEX_MQJS_PORT_H
#define LITEX_MQJS_PORT_H

#include <stddef.h>
#include "mquickjs.h"

void mqjs_log_func(void *opaque, const void *buf, size_t buf_len);
void mqjs_set_print_func(JSWriteFunc *write_func);
int mqjs_fs_available(void);
int mqjs_fs_read_file(const char *path, const char **data, size_t *len,
                      char *err, size_t err_size);
int mqjs_fs_write_file(const char *path, const char *data, size_t len,
                       char *err, size_t err_size);

JSValue js_print(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_gc   (JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_date_now       (JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_performance_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);

JSValue js_litex_set_leds    (JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_litex_get_switches(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_litex_get_buttons (JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_litex_get_identifier(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_litex_get_scratch(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_litex_set_scratch(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_litex_clock_frequency(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_litex_millis      (JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_litex_delay       (JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_litex_csr_read32  (JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_litex_csr_write32 (JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_litex_read_file   (JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_litex_write_file  (JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_litex_load        (JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_litex_reboot      (JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);

JSValue js_framebuffer_get_width (JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_framebuffer_get_height(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_framebuffer_get_depth (JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_framebuffer_get_double_buffered(JSContext *ctx, JSValue *this_val,
                                           int argc, JSValue *argv);
JSValue js_framebuffer_begin     (JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_framebuffer_present   (JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_framebuffer_clear     (JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_framebuffer_fill_rect (JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_framebuffer_copy_rect (JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_framebuffer_line      (JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_framebuffer_circle    (JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_framebuffer_text      (JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_framebuffer_fade      (JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_framebuffer_scroll    (JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_framebuffer_blit      (JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_framebuffer_blit_scale(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_framebuffer_blit_indexed_scale(JSContext *ctx, JSValue *this_val,
                                          int argc, JSValue *argv);

#endif
