/*
 * SzpontOS - Loopback Network Interface (127.0.0.1)
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <net/net.h>
#include <kernel/string.h>
#include <kernel/kprint.h>

static netif_t g_loopback_netif;

static int loopback_send(netif_t *netif, net_buf_t *buf) {
    if (!netif || !buf) return -1;

    net_buf_t *rx_copy = net_buf_alloc();
    if (!rx_copy) return -1;

    memcpy(rx_copy->data, buf->data + buf->offset, buf->len);
    rx_copy->len = buf->len;
    rx_copy->offset = 0;

    netif->tx_packets++;
    netif->tx_bytes += buf->len;
    netif->rx_packets++;
    netif->rx_bytes += buf->len;

    /* Loopback directly injects packet into input handler */
    netif_input(netif, rx_copy);
    return 0;
}

void loopback_init(void) {
    memset(&g_loopback_netif, 0, sizeof(netif_t));
    ksnprintf(g_loopback_netif.name, sizeof(g_loopback_netif.name), "lo");

    /* 127.0.0.1 */
    g_loopback_netif.ip = (127) | (0 << 8) | (0 << 16) | (1 << 24);
    /* 255.0.0.0 */
    g_loopback_netif.netmask = (255) | (0 << 8) | (0 << 16) | (0 << 24);
    g_loopback_netif.gateway = 0;
    g_loopback_netif.flags = NETIF_FLAG_UP | NETIF_FLAG_LOOPBACK | NETIF_FLAG_RUNNING;
    g_loopback_netif.send = loopback_send;

    netif_register(&g_loopback_netif);
    klog_info("NET: Loopback interface 'lo' initialized (127.0.0.1/8)");
}
