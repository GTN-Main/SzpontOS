/*
 * SzpontOS - Address Resolution Protocol (ARP) Subsystem
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <net/net.h>
#include <kernel/string.h>
#include <kernel/kprint.h>
#include <kernel/spinlock.h>

#define ARP_CACHE_SIZE 64

typedef struct {
    uint32_t ip;
    uint8_t  mac[ETH_ALEN];
    bool     valid;
} arp_entry_t;

static arp_entry_t g_arp_cache[ARP_CACHE_SIZE];
static spinlock_t g_arp_lock = SPINLOCK_INIT;

void arp_init(void) {
    memset(g_arp_cache, 0, sizeof(g_arp_cache));
}

void arp_cache_insert(uint32_t ip, const uint8_t *mac) {
    spinlock_acquire(&g_arp_lock);

    /* Check if already in cache */
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (g_arp_cache[i].valid && g_arp_cache[i].ip == ip) {
            memcpy(g_arp_cache[i].mac, mac, ETH_ALEN);
            spinlock_release(&g_arp_lock);
            return;
        }
    }

    /* Find free slot */
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!g_arp_cache[i].valid) {
            g_arp_cache[i].ip = ip;
            memcpy(g_arp_cache[i].mac, mac, ETH_ALEN);
            g_arp_cache[i].valid = true;
            spinlock_release(&g_arp_lock);
            return;
        }
    }

    /* Overwrite first slot if full */
    g_arp_cache[0].ip = ip;
    memcpy(g_arp_cache[0].mac, mac, ETH_ALEN);
    g_arp_cache[0].valid = true;
    spinlock_release(&g_arp_lock);
}

bool arp_lookup(uint32_t ip, uint8_t mac_out[ETH_ALEN]) {
    spinlock_acquire(&g_arp_lock);
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (g_arp_cache[i].valid && g_arp_cache[i].ip == ip) {
            memcpy(mac_out, g_arp_cache[i].mac, ETH_ALEN);
            spinlock_release(&g_arp_lock);
            return true;
        }
    }
    spinlock_release(&g_arp_lock);
    return false;
}

void arp_send_request(netif_t *netif, uint32_t target_ip) {
    if (!netif || !netif->send) return;

    net_buf_t *buf = net_buf_alloc();
    if (!buf) return;

    eth_hdr_t *eth = (eth_hdr_t *)buf->data;
    memset(eth->dest, 0xFF, ETH_ALEN); /* Broadcast */
    memcpy(eth->src, netif->mac, ETH_ALEN);
    eth->type = htons(ETH_P_ARP);

    arp_hdr_t *arp = (arp_hdr_t *)(buf->data + sizeof(eth_hdr_t));
    arp->hw_type = htons(1);       /* Ethernet */
    arp->proto_type = htons(ETH_P_IP);
    arp->hw_len = ETH_ALEN;
    arp->proto_len = 4;
    arp->opcode = htons(ARP_OP_REQUEST);
    memcpy(arp->sender_mac, netif->mac, ETH_ALEN);
    arp->sender_ip = netif->ip;
    memset(arp->target_mac, 0, ETH_ALEN);
    arp->target_ip = target_ip;

    buf->len = sizeof(eth_hdr_t) + sizeof(arp_hdr_t);
    buf->offset = 0;

    netif_output(netif, buf);
}

static void arp_send_reply(netif_t *netif, uint32_t target_ip, const uint8_t *target_mac) {
    if (!netif || !netif->send) return;

    net_buf_t *buf = net_buf_alloc();
    if (!buf) return;

    eth_hdr_t *eth = (eth_hdr_t *)buf->data;
    memcpy(eth->dest, target_mac, ETH_ALEN);
    memcpy(eth->src, netif->mac, ETH_ALEN);
    eth->type = htons(ETH_P_ARP);

    arp_hdr_t *arp = (arp_hdr_t *)(buf->data + sizeof(eth_hdr_t));
    arp->hw_type = htons(1);
    arp->proto_type = htons(ETH_P_IP);
    arp->hw_len = ETH_ALEN;
    arp->proto_len = 4;
    arp->opcode = htons(ARP_OP_REPLY);
    memcpy(arp->sender_mac, netif->mac, ETH_ALEN);
    arp->sender_ip = netif->ip;
    memcpy(arp->target_mac, target_mac, ETH_ALEN);
    arp->target_ip = target_ip;

    buf->len = sizeof(eth_hdr_t) + sizeof(arp_hdr_t);
    buf->offset = 0;

    netif_output(netif, buf);
}

void arp_input(netif_t *netif, net_buf_t *buf) {
    if (!netif || !buf || buf->len < sizeof(arp_hdr_t)) {
        if (buf) net_buf_free(buf);
        return;
    }

    arp_hdr_t *arp = (arp_hdr_t *)(buf->data + buf->offset);
    uint16_t opcode = ntohs(arp->opcode);

    /* Learn sender MAC */
    arp_cache_insert(arp->sender_ip, arp->sender_mac);

    if (opcode == ARP_OP_REQUEST && arp->target_ip == netif->ip) {
        arp_send_reply(netif, arp->sender_ip, arp->sender_mac);
    }

    net_buf_free(buf);
}
