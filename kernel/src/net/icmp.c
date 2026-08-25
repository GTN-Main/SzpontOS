/*
 * SzpontOS - Internet Control Message Protocol (ICMP) Subsystem
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <net/net.h>
#include <net/socket.h>
#include <kernel/string.h>
#include <kernel/kprint.h>

void icmp_input(netif_t *netif, net_buf_t *buf) {
    if (!netif || !buf || buf->len < sizeof(icmp_hdr_t)) {
        if (buf)
            net_buf_free(buf);
        return;
    }

    ipv4_hdr_t *ip = (ipv4_hdr_t *)(buf->data + buf->offset - sizeof(ipv4_hdr_t));
    icmp_hdr_t *icmp = (icmp_hdr_t *)(buf->data + buf->offset);

    if (icmp->type == ICMP_TYPE_ECHOREQ) {
        uint32_t src_ip = ip->src_ip;

        /* Build Echo Reply */
        icmp->type = ICMP_TYPE_ECHOREPLY;
        icmp->code = 0;
        icmp->checksum = 0;
        icmp->checksum = ipv4_checksum(icmp, buf->len);

        net_buf_t *reply_buf = net_buf_alloc();
        if (reply_buf) {
            memcpy(reply_buf->data, icmp, buf->len);
            reply_buf->len = buf->len;
            reply_buf->offset = 0;
            ipv4_output(netif, src_ip, IP_PROTO_ICMP, reply_buf);
        }
    } else if (icmp->type == ICMP_TYPE_ECHOREPLY) {
        /* Deliver to RAW ICMP sockets (FreeBSD rip_input pattern) */
        socket_t *sock = socket_find_icmp(netif->ip);
        if (sock) {
            socket_enqueue_data(sock, icmp, buf->len, ip->src_ip, 0);
        }
    }

    net_buf_free(buf);
}

int icmp_send_echo_request(uint32_t dest_ip, uint16_t id, uint16_t seq, const void *payload, size_t len) {
    net_buf_t *buf = net_buf_alloc();
    if (!buf)
        return -1;

    icmp_hdr_t *icmp = (icmp_hdr_t *)buf->data;
    icmp->type = ICMP_TYPE_ECHOREQ;
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->id = htons(id);
    icmp->sequence = htons(seq);

    if (payload && len > 0) {
        memcpy(buf->data + sizeof(icmp_hdr_t), payload, len);
    }

    buf->len = sizeof(icmp_hdr_t) + len;
    buf->offset = 0;
    icmp->checksum = ipv4_checksum(icmp, buf->len);

    return ipv4_output(NULL, dest_ip, IP_PROTO_ICMP, buf);
}
