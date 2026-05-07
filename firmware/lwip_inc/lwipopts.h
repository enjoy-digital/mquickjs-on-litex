// Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
// SPDX-License-Identifier: BSD-2-Clause

#ifndef LITEX_MQJS_LWIPOPTS_H
#define LITEX_MQJS_LWIPOPTS_H

#include <generated/csr.h>

#ifdef CSR_TIMER0_UPTIME_CYCLES_ADDR
static inline unsigned int litex_lwip_rand(void)
{
    timer0_uptime_latch_write(1);
    return (unsigned int)timer0_uptime_cycles_read();
}
#define LWIP_RAND() litex_lwip_rand()
#else
#define LWIP_RAND() 0x12345678u
#endif

#define NO_SYS                          1
#define SYS_LIGHTWEIGHT_PROT            0
#define LWIP_SOCKET                     0
#define LWIP_NETCONN                    0

#define LWIP_IPV4                       1
#define LWIP_IPV6                       0
#define LWIP_TCP                        1
#define LWIP_UDP                        0
#define LWIP_RAW                        1
#define LWIP_ICMP                       1
#define LWIP_DHCP                       0
#define LWIP_DNS                        0
#define LWIP_AUTOIP                     0

#define LWIP_NETIF_HOSTNAME             0
#define LWIP_NETIF_STATUS_CALLBACK      0
#define LWIP_NETIF_LINK_CALLBACK        0
#define LWIP_NETIF_TX_SINGLE_PBUF       0

#define MEM_ALIGNMENT                   4
#define MEM_SIZE                        (48 * 1024)
#define MEMP_NUM_PBUF                   12
#define MEMP_NUM_RAW_PCB                2
#define MEMP_NUM_TCP_PCB                4
#define MEMP_NUM_TCP_PCB_LISTEN         1
#define MEMP_NUM_TCP_SEG                24

#define PBUF_POOL_SIZE                  16
#define PBUF_POOL_BUFSIZE               1536

#define TCP_MSS                         1460
#define TCP_WND                         (4 * TCP_MSS)
#define TCP_SND_BUF                     (4 * TCP_MSS)
#define TCP_SND_QUEUELEN                16
#define TCP_LISTEN_BACKLOG              1

#define LWIP_STATS                      0
#define LWIP_DEBUG                      0
#define LWIP_DBG_MIN_LEVEL              LWIP_DBG_LEVEL_OFF

#define CHECKSUM_GEN_IP                 1
#define CHECKSUM_GEN_TCP                1
#define CHECKSUM_CHECK_IP               1
#define CHECKSUM_CHECK_TCP              1

#endif /* LITEX_MQJS_LWIPOPTS_H */
