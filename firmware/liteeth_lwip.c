// Copyright (c) 2026 EnjoyDigital <florent@enjoy-digital.fr>
// SPDX-License-Identifier: BSD-2-Clause

#include <generated/csr.h>
#include <generated/mem.h>
#include <generated/soc.h>

#ifdef CSR_ETHMAC_BASE

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <system.h>
#include <libbase/crc.h>

#include "lwip/etharp.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/snmp.h"
#include "lwip/sys.h"
#include "netif/ethernet.h"

#include "liteeth_lwip.h"

#ifdef CSR_ETHMAC_PREAMBLE_CRC_ADDR
#define LITEETH_LWIP_HW_PREAMBLE_CRC
#endif

#ifndef ETHMAC_EV_SRAM_READER
#define ETHMAC_EV_SRAM_READER 0x1
#endif
#ifndef ETHMAC_EV_SRAM_WRITER
#define ETHMAC_EV_SRAM_WRITER 0x1
#endif

static uint8_t tx_slot;

u32_t sys_now(void)
{
#ifdef CSR_TIMER0_UPTIME_CYCLES_ADDR
    timer0_uptime_latch_write(1);
    return (u32_t)(timer0_uptime_cycles_read() / (CONFIG_CLOCK_FREQUENCY / 1000));
#else
    static u32_t fallback_ms;

    return fallback_ms++;
#endif
}

static void liteeth_lwip_eth_init(void)
{
    puts("Ethernet init...");
#ifdef CSR_ETHPHY_CRG_RESET_ADDR
#ifndef ETH_PHY_NO_RESET
    ethphy_crg_reset_write(1);
    busy_wait(200);
    ethphy_crg_reset_write(0);
    busy_wait(200);
#endif
#endif
}

static int liteeth_lwip_rx_payload(const uint8_t *raw, uint16_t raw_len,
                                   const uint8_t **payload,
                                   uint16_t *payload_len)
{
#ifdef LITEETH_LWIP_HW_PREAMBLE_CRC
    *payload     = raw;
    *payload_len = raw_len;
    return 0;
#else
    uint32_t received_crc;
    uint32_t computed_crc;

    if (raw_len < 12)
        return -1;
    for (int i = 0; i < 7; i++) {
        if (raw[i] != 0x55)
            return -1;
    }
    if (raw[7] != 0xd5)
        return -1;

    received_crc = ((uint32_t)raw[raw_len - 1] << 24) |
                   ((uint32_t)raw[raw_len - 2] << 16) |
                   ((uint32_t)raw[raw_len - 3] <<  8) |
                   ((uint32_t)raw[raw_len - 4]);
    computed_crc = crc32(&raw[8], raw_len - 12);
    if (received_crc != computed_crc)
        return -1;

    *payload     = &raw[8];
    *payload_len = raw_len - 12;
    return 0;
#endif
}

static void liteeth_lwip_poll_rx(struct netif *netif)
{
    for (int i = 0; i < ETHMAC_RX_SLOTS; i++) {
        const uint8_t *payload;
        struct pbuf *p;
        uint8_t slot;
        const uint8_t *raw;
        uint16_t raw_len;
        uint16_t payload_len;

        if (!(ethmac_sram_writer_ev_pending_read() & ETHMAC_EV_SRAM_WRITER))
            break;

        flush_cpu_dcache();

        slot    = ethmac_sram_writer_slot_read();
        raw_len = ethmac_sram_writer_length_read();
        raw     = (const uint8_t *)(ETHMAC_BASE + ETHMAC_SLOT_SIZE * slot);

        if (raw_len <= ETHMAC_SLOT_SIZE &&
            liteeth_lwip_rx_payload(raw, raw_len, &payload, &payload_len) == 0) {
            p = pbuf_alloc(PBUF_RAW, payload_len, PBUF_POOL);
            if (p != NULL) {
                pbuf_take(p, payload, payload_len);
                if (netif->input(p, netif) != ERR_OK)
                    pbuf_free(p);
            }
        }

        ethmac_sram_writer_ev_pending_write(ETHMAC_EV_SRAM_WRITER);
    }
}

void liteeth_lwip_poll(struct netif *netif)
{
    liteeth_lwip_poll_rx(netif);
}

static err_t liteeth_lwip_linkoutput(struct netif *netif, struct pbuf *p)
{
    uint8_t *raw;
    uint16_t raw_len;

    (void)netif;

    while (!ethmac_sram_reader_ready_read()) {
    }

    raw = (uint8_t *)(ETHMAC_BASE + ETHMAC_SLOT_SIZE *
                      (ETHMAC_RX_SLOTS + tx_slot));

#ifdef LITEETH_LWIP_HW_PREAMBLE_CRC
    if (p->tot_len > ETHMAC_SLOT_SIZE)
        return ERR_BUF;
    pbuf_copy_partial(p, raw, p->tot_len, 0);
    raw_len = p->tot_len;
#else
    if ((p->tot_len + 12) > ETHMAC_SLOT_SIZE)
        return ERR_BUF;
    for (int i = 0; i < 7; i++)
        raw[i] = 0x55;
    raw[7] = 0xd5;
    pbuf_copy_partial(p, &raw[8], p->tot_len, 0);
    raw_len = p->tot_len + 8;
    uint32_t crc = crc32(&raw[8], raw_len - 8);
    raw[raw_len + 0] = (crc >>  0) & 0xff;
    raw[raw_len + 1] = (crc >>  8) & 0xff;
    raw[raw_len + 2] = (crc >> 16) & 0xff;
    raw[raw_len + 3] = (crc >> 24) & 0xff;
    raw_len += 4;
#endif

    flush_cpu_dcache();

    ethmac_sram_reader_slot_write(tx_slot);
    ethmac_sram_reader_length_write(raw_len);
    ethmac_sram_reader_start_write(1);
    tx_slot = (tx_slot + 1) % ETHMAC_TX_SLOTS;

    LINK_STATS_INC(link.xmit);

    return ERR_OK;
}

static err_t liteeth_lwip_netif_init(struct netif *netif)
{
    netif->name[0]    = 'e';
    netif->name[1]    = 'n';
    netif->mtu        = 1500;
    netif->hwaddr_len = 6;
    netif->output     = etharp_output;
    netif->linkoutput = liteeth_lwip_linkoutput;
    netif->flags      = NETIF_FLAG_BROADCAST |
                        NETIF_FLAG_ETHARP |
                        NETIF_FLAG_ETHERNET |
                        NETIF_FLAG_LINK_UP;
    MIB2_INIT_NETIF(netif, snmp_ifType_ethernet_csmacd, 100000000);

    return ERR_OK;
}

static void ip4_from_uint32(ip4_addr_t *addr, uint32_t ip)
{
    IP4_ADDR(addr,
             (ip >> 24) & 0xff,
             (ip >> 16) & 0xff,
             (ip >>  8) & 0xff,
             (ip >>  0) & 0xff);
}

int liteeth_lwip_init(struct netif *netif, const uint8_t *mac_addr,
                      uint32_t ip, uint32_t netmask, uint32_t gateway)
{
    static int lwip_initialized;
    ip4_addr_t ip_addr;
    ip4_addr_t netmask_addr;
    ip4_addr_t gateway_addr;

    if (!lwip_initialized) {
        lwip_init();
        lwip_initialized = 1;
    }

    liteeth_lwip_eth_init();

    memcpy(netif->hwaddr, mac_addr, 6);
    ethmac_sram_reader_ev_pending_write(ETHMAC_EV_SRAM_READER);
    ethmac_sram_writer_ev_pending_write(ETHMAC_EV_SRAM_WRITER);
    tx_slot = 0;

    ip4_from_uint32(&ip_addr, ip);
    ip4_from_uint32(&netmask_addr, netmask);
    ip4_from_uint32(&gateway_addr, gateway);

    if (netif_add(netif, &ip_addr, &netmask_addr, &gateway_addr, NULL,
                  liteeth_lwip_netif_init, ethernet_input) == NULL)
        return -1;

    netif_set_default(netif);
    netif_set_up(netif);
    netif_set_link_up(netif);

    return 0;
}

#endif /* CSR_ETHMAC_BASE */
