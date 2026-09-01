/*
 * SzpontOS - User Datagram Protocol (UDP) Subsystem
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <net/net.h>
#include <net/socket.h>
#include <kernel/string.h>
#include <kernel/kprint.h>

extern socket_t *socket_find_udp(uint32_t local_ip, uint16_t local_port);
extern int socket_enqueue_data(socket_t *sock, const void *data, size_t len, uint32_t from_ip, uint16_t from_port);

void udp_init(void) {}

void udp_input(netif_t *netif, net_buf_t *buf) {
    if (!netif || !buf || buf->len < sizeof(udp_hdr_t)) {
        if (buf)
            net_buf_free(buf);
        return;
    }

    ipv4_hdr_t *ip = (ipv4_hdr_t *)(buf->data + buf->offset - sizeof(ipv4_hdr_t));
    udp_hdr_t *udp = (udp_hdr_t *)(buf->data + buf->offset);

    uint16_t src_port = ntohs(udp->src_port);
    uint16_t dest_port = ntohs(udp->dest_port);
    uint16_t len = ntohs(udp->length);

    if (len < sizeof(udp_hdr_t) || len > buf->len) {
        net_buf_free(buf);
        return;
    }

    const uint8_t *payload = buf->data + buf->offset + sizeof(udp_hdr_t);
    size_t payload_len = len - sizeof(udp_hdr_t);

    klog_info("UDP: input %zu bytes from %d.%d.%d.%d:%u to port %u",
              payload_len,
              (ip->src_ip) & 0xFF, (ip->src_ip >> 8) & 0xFF, (ip->src_ip >> 16) & 0xFF, (ip->src_ip >> 24) & 0xFF,
              src_port, dest_port);

    socket_t *sock = socket_find_udp(ip->dest_ip, dest_port);
    if (sock) {
        klog_info("UDP: found destination socket %p for port %u, enqueuing", sock, dest_port);
        socket_enqueue_data(sock, payload, payload_len, ip->src_ip, src_port);
    } else {
        klog_info("UDP: no socket listening on port %u", dest_port);
    }

    net_buf_free(buf);
}

typedef struct __attribute__((packed)) {
    uint32_t src_ip;
    uint32_t dest_ip;
    uint8_t zero;
    uint8_t protocol;
    uint16_t udp_len;
} udp_pseudo_header_t;

static uint16_t udp_checksum(uint32_t src_ip, uint32_t dest_ip, const udp_hdr_t *udp, size_t len) {
    udp_pseudo_header_t ph;
    ph.src_ip = src_ip;
    ph.dest_ip = dest_ip;
    ph.zero = 0;
    ph.protocol = IP_PROTO_UDP;
    ph.udp_len = htons((uint16_t)len);

    uint32_t sum = 0;
    const uint16_t *p = (const uint16_t *)&ph;
    for (size_t i = 0; i < sizeof(ph) / 2; i++) {
        sum += *p++;
    }

    p = (const uint16_t *)udp;
    size_t l = len;
    while (l > 1) {
        sum += *p++;
        l -= 2;
    }
    if (l == 1) {
        sum += (uint8_t)(*(const uint8_t *)p);
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    uint16_t res = (uint16_t)(~sum);
    return (res == 0) ? 0xFFFF : res;
}

int udp_output(uint32_t src_ip, uint16_t src_port, uint32_t dest_ip, uint16_t dest_port, net_buf_t *payload) {
    if (!payload)
        return -1;

    net_buf_t *tx_buf = net_buf_alloc();
    if (!tx_buf) {
        net_buf_free(payload);
        return -1;
    }

    udp_hdr_t *udp = (udp_hdr_t *)tx_buf->data;
    udp->src_port = htons(src_port);
    udp->dest_port = htons(dest_port);
    udp->length = htons((uint16_t)(sizeof(udp_hdr_t) + payload->len));
    udp->checksum = 0;

    memcpy(tx_buf->data + sizeof(udp_hdr_t), payload->data + payload->offset, payload->len);
    tx_buf->len = sizeof(udp_hdr_t) + payload->len;
    tx_buf->offset = 0;

    netif_t *netif = ((dest_ip & 0xFF) == 127 || netif_find_by_ip(dest_ip) != NULL) ? netif_get_loopback() : netif_get_default();
    uint32_t real_src_ip = (src_ip != 0) ? src_ip : (netif ? netif->ip : 0);
    udp->checksum = 0; /* Optional in IPv4 (RFC 768) - ensures 100% SLIRP acceptance */

    klog_info("UDP: sending %u bytes from %u.%u.%u.%u:%u to %u.%u.%u.%u:%u (cksum=0x%04x)",
              (unsigned int)payload->len,
              (unsigned int)((real_src_ip) & 0xFF), (unsigned int)((real_src_ip >> 8) & 0xFF),
              (unsigned int)((real_src_ip >> 16) & 0xFF), (unsigned int)((real_src_ip >> 24) & 0xFF),
              (unsigned int)src_port,
              (unsigned int)((dest_ip) & 0xFF), (unsigned int)((dest_ip >> 8) & 0xFF),
              (unsigned int)((dest_ip >> 16) & 0xFF), (unsigned int)((dest_ip >> 24) & 0xFF),
              (unsigned int)dest_port, (unsigned int)udp->checksum);

    net_buf_free(payload);
    return ipv4_output(netif, dest_ip, IP_PROTO_UDP, tx_buf);
}
