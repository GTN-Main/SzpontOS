/*
 * SzpontOS - Network Interface Management & Packet Buffer Subsystem
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <net/net.h>
#include <mm/heap.h>
#include <kernel/kprint.h>
#include <kernel/string.h>

static netif_t *g_netif_list = NULL;
static spinlock_t g_netif_lock = SPINLOCK_INIT;

net_buf_t *net_buf_alloc(void) {
    net_buf_t *buf = (net_buf_t *)kmalloc(sizeof(net_buf_t));
    if (buf) {
        memset(buf, 0, sizeof(net_buf_t));
    }
    return buf;
}

void net_buf_free(net_buf_t *buf) {
    if (buf) {
        kfree(buf);
    }
}

void netif_register(netif_t *nif) {
    if (!nif) return;

    spinlock_acquire(&g_netif_lock);
    nif->next = g_netif_list;
    g_netif_list = nif;
    spinlock_release(&g_netif_lock);

    klog_info("NET: Registered interface '%s'", nif->name);
}

netif_t *netif_get_default(void) {
    spinlock_acquire(&g_netif_lock);
    for (netif_t *cur = g_netif_list; cur != NULL; cur = cur->next) {
        if (!(cur->flags & NETIF_FLAG_LOOPBACK) && (cur->flags & NETIF_FLAG_UP)) {
            spinlock_release(&g_netif_lock);
            return cur;
        }
    }
    netif_t *lo = netif_get_loopback();
    spinlock_release(&g_netif_lock);
    return lo;
}

netif_t *netif_get_loopback(void) {
    for (netif_t *cur = g_netif_list; cur != NULL; cur = cur->next) {
        if (cur->flags & NETIF_FLAG_LOOPBACK) {
            return cur;
        }
    }
    return NULL;
}

netif_t *netif_find_by_name(const char *name) {
    if (!name) return NULL;
    spinlock_acquire(&g_netif_lock);
    for (netif_t *cur = g_netif_list; cur != NULL; cur = cur->next) {
        if (strcmp(cur->name, name) == 0) {
            spinlock_release(&g_netif_lock);
            return cur;
        }
    }
    spinlock_release(&g_netif_lock);
    return NULL;
}

netif_t *netif_find_by_ip(uint32_t ip) {
    spinlock_acquire(&g_netif_lock);
    for (netif_t *cur = g_netif_list; cur != NULL; cur = cur->next) {
        if (cur->ip == ip) {
            spinlock_release(&g_netif_lock);
            return cur;
        }
    }
    spinlock_release(&g_netif_lock);
    return NULL;
}

netif_t *netif_get_list(void) {
    return g_netif_list;
}

void netif_input(netif_t *netif, net_buf_t *buf) {
    if (!netif || !buf || buf->len < sizeof(eth_hdr_t)) {
        if (buf) net_buf_free(buf);
        return;
    }

    eth_hdr_t *eth = (eth_hdr_t *)(buf->data + buf->offset);
    uint16_t eth_type = ntohs(eth->type);

    /* Advance buffer offset past Ethernet header */
    buf->offset += sizeof(eth_hdr_t);
    buf->len -= sizeof(eth_hdr_t);

    if (eth_type == ETH_P_ARP) {
        arp_input(netif, buf);
    } else if (eth_type == ETH_P_IP) {
        ipv4_input(netif, buf);
    } else {
        net_buf_free(buf);
    }
}

int netif_output(netif_t *netif, net_buf_t *buf) {
    if (!netif || !buf || !netif->send) {
        if (buf) net_buf_free(buf);
        return -1;
    }
    int res = netif->send(netif, buf);
    net_buf_free(buf);
    return res;
}
