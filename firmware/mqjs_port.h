// Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef LITEX_MQJS_PORT_H
#define LITEX_MQJS_PORT_H

#include <stddef.h>
#include "mquickjs.h"

void mqjs_log_func(void *opaque, const void *buf, size_t buf_len);

JSValue js_print(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_gc   (JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_date_now       (JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_performance_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);

JSValue js_litex_set_leds    (JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_litex_get_switches(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_litex_get_buttons (JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_litex_millis      (JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_litex_delay       (JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_litex_csr_read32  (JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_litex_csr_write32 (JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_litex_read_file   (JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_litex_load        (JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
JSValue js_litex_reboot      (JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);

#endif
