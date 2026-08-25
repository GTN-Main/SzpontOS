#ifndef SZPONTOS_NET_NET_H
#define SZPONTOS_NET_NET_H

#include <kernel/types.h>
#include <kernel/spinlock.h>

#define ETH_ALEN 6
#define ETH_HLEN 14
#define ETH_P_IP 0x0800
#define ETH_P_ARP 0x0806

#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP 6
#define IP_PROTO_UDP 17

#define NET_MAX_PACKET_SIZE 1536

/* Network Buffer (sk_buff equivalent) */
typedef struct net_buf {
    uint8_t data[NET_MAX_PACKET_SIZE];
    size_t len;
    size_t offset;
    struct net_buf *next;
} net_buf_t;

/* Ethernet Frame Header */
typedef struct __attribute__((packed)) {
    uint8_t dest[ETH_ALEN];
    uint8_t src[ETH_ALEN];
    uint16_t type;
} eth_hdr_t;

/* ARP Header */
typedef struct __attribute__((packed)) {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t hw_len;
    uint8_t proto_len;
    uint16_t opcode;
    uint8_t sender_mac[ETH_ALEN];
    uint32_t sender_ip;
    uint8_t target_mac[ETH_ALEN];
    uint32_t target_ip;
} arp_hdr_t;

#define ARP_OP_REQUEST 1
#define ARP_OP_REPLY 2

/* IPv4 Header */
typedef struct __attribute__((packed)) {
    uint8_t version_ihl;
    uint8_t tos;
    uint16_t total_len;
    uint16_t id;
    uint16_t flags_offset;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dest_ip;
} ipv4_hdr_t;

/* ICMP Header */
typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t id;
    uint16_t sequence;
} icmp_hdr_t;

#define ICMP_TYPE_ECHOREPLY 0
#define ICMP_TYPE_ECHOREQ 8

/* UDP Header */
typedef struct __attribute__((packed)) {
    uint16_t src_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
} udp_hdr_t;

/* TCP Header */
typedef struct __attribute__((packed)) {
    uint16_t src_port;
    uint16_t dest_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t data_offset; /* 4 bits offset, 4 bits reserved */
    uint8_t flags;
    uint16_t window_size;
    uint16_t checksum;
    uint16_t urgent_ptr;
} tcp_hdr_t;

#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10
#define TCP_FLAG_URG 0x20

/* Network Interface */
typedef struct netif {
    char name[16];
    uint8_t mac[ETH_ALEN];
    uint32_t ip;
    uint32_t netmask;
    uint32_t gateway;
    uint32_t flags;
    size_t rx_packets;
    size_t tx_packets;
    size_t rx_bytes;
    size_t tx_bytes;
    int (*send)(struct netif *netif, net_buf_t *buf);
    struct netif *next;
} netif_t;

#define NETIF_FLAG_UP 0x01
#define NETIF_FLAG_LOOPBACK 0x02
#define NETIF_FLAG_RUNNING 0x04

/* Byte-order helpers */
static inline uint16_t htons(uint16_t hostshort) {
    return (uint16_t)(((hostshort & 0xFF) << 8) | ((hostshort >> 8) & 0xFF));
}

static inline uint16_t ntohs(uint16_t netshort) {
    return htons(netshort);
}

static inline uint32_t htonl(uint32_t hostlong) {
    return ((hostlong & 0xFF) << 24) | ((hostlong & 0xFF00) << 8) | ((hostlong & 0xFF0000) >> 8) |
           ((hostlong >> 24) & 0xFF);
}

static inline uint32_t ntohl(uint32_t netlong) {
    return htonl(netlong);
}

/* Core Stack Subsystem Prototypes */
void net_init(void);
net_buf_t *net_buf_alloc(void);
void net_buf_free(net_buf_t *buf);

void netif_register(netif_t *nif);
netif_t *netif_get_default(void);
netif_t *netif_get_loopback(void);
netif_t *netif_find_by_name(const char *name);
netif_t *netif_find_by_ip(uint32_t ip);
netif_t *netif_get_list(void);

void netif_input(netif_t *netif, net_buf_t *buf);
int netif_output(netif_t *netif, net_buf_t *buf);

/* ARP Prototypes */
void arp_init(void);
void arp_input(netif_t *netif, net_buf_t *buf);
bool arp_lookup(uint32_t ip, uint8_t mac_out[ETH_ALEN]);
void arp_send_request(netif_t *netif, uint32_t target_ip);
void arp_cache_insert(uint32_t ip, const uint8_t *mac);

/* IPv4 Prototypes */
void ipv4_init(void);
void ipv4_input(netif_t *netif, net_buf_t *buf);
int ipv4_output(netif_t *netif, uint32_t dest_ip, uint8_t proto, net_buf_t *payload);
uint16_t ipv4_checksum(const void *data, size_t len);

/* ICMP Prototypes */
void icmp_input(netif_t *netif, net_buf_t *buf);
int icmp_send_echo_request(uint32_t dest_ip, uint16_t id, uint16_t seq, const void *payload, size_t len);

/* UDP Prototypes */
void udp_init(void);
void udp_input(netif_t *netif, net_buf_t *buf);
int udp_output(uint32_t src_ip, uint16_t src_port, uint32_t dest_ip, uint16_t dest_port, net_buf_t *buf);

/* TCP Prototypes */
void tcp_init(void);
void tcp_input(netif_t *netif, net_buf_t *buf);

#endif /* SZPONTOS_NET_NET_H */
