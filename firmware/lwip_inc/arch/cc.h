// Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef LITEX_MQJS_LWIP_ARCH_CC_H
#define LITEX_MQJS_LWIP_ARCH_CC_H

#include <stdio.h>

#define LWIP_PLATFORM_DIAG(x)      do { printf x; } while (0)
#define LWIP_PLATFORM_ASSERT(x)    do { printf("lwIP assert: %s\n", x); } while (0)
#define LWIP_NO_CTYPE_H            1

#endif /* LITEX_MQJS_LWIP_ARCH_CC_H */
