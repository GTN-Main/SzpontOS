/*
 * SzpontOS Libc - <netinet/ip_icmp.h>
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef _NETINET_IP_ICMP_H_
#define _NETINET_IP_ICMP_H_

#include <stdint.h>
#include <sys/types.h>
#include <netinet/in.h>

struct icmphdr {
    uint8_t  icmp_type;
    uint8_t  icmp_code;
    uint16_t icmp_cksum;
    union {
        struct {
            uint16_t id;
            uint16_t sequence;
        } echo;
        uint32_t gateway;
        struct {
            uint16_t unused;
            uint16_t mtu;
        } frag;
    } un;
};

#define ICMP_ECHOREPLY      0
#define ICMP_UNREACH        3
#define ICMP_ECHO           8
#define ICMP_TIMXCEED       11

#endif /* _NETINET_IP_ICMP_H_ */
