// Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef LITEX_MQJS_LIVE_RUNTIME_H
#define LITEX_MQJS_LIVE_RUNTIME_H

#include <stddef.h>

#include "mquickjs.h"

JSContext *new_mqjs_context(void);
int run_source(JSContext *ctx, const char *src, size_t src_len,
               const char *filename, int eval_flags);

#endif /* LITEX_MQJS_LIVE_RUNTIME_H */
