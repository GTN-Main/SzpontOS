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

void udp_init(void) {
}

void udp_input(netif_t *netif, net_buf_t *buf) {
    if (!netif || !buf || buf->len < sizeof(udp_hdr_t)) {
        if (buf) net_buf_free(buf);
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

    socket_t *sock = socket_find_udp(ip->dest_ip, dest_port);
    if (sock) {
        socket_enqueue_data(sock, payload, payload_len, ip->src_ip, src_port);
    }

    net_buf_free(buf);
}

int udp_output(uint32_t src_ip, uint16_t src_port, uint32_t dest_ip, uint16_t dest_port, net_buf_t *payload) {
    if (!payload) return -1;

    net_buf_t *tx_buf = net_buf_alloc();
    if (!tx_buf) {
        net_buf_free(payload);
        return -1;
    }

    udp_hdr_t *udp = (udp_hdr_t *)tx_buf->data;
    udp->src_port = htons(src_port);
    udp->dest_port = htons(dest_port);
    udp->length = htons((uint16_t)(sizeof(udp_hdr_t) + payload->len));
    udp->checksum = 0; /* Optional in IPv4 */

    memcpy(tx_buf->data + sizeof(udp_hdr_t), payload->data + payload->offset, payload->len);
    tx_buf->len = sizeof(udp_hdr_t) + payload->len;
    tx_buf->offset = 0;

    (void)src_ip;
    net_buf_free(payload);
    return ipv4_output(NULL, dest_ip, IP_PROTO_UDP, tx_buf);
}
