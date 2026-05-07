// Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef LITEX_MQJS_LITEETH_LWIP_H
#define LITEX_MQJS_LITEETH_LWIP_H

#include <stdint.h>

#include "lwip/netif.h"

int liteeth_lwip_init(struct netif *netif, const uint8_t *mac_addr,
                      uint32_t ip, uint32_t netmask, uint32_t gateway);
void liteeth_lwip_poll(struct netif *netif);

#endif /* LITEX_MQJS_LITEETH_LWIP_H */
