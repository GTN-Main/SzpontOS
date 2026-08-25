/*
 * SzpontOS Libc - Network Interface Discovery (getifaddrs, if_nameindex)
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <net/if.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>

static struct if_nameindex g_interfaces[] = {{1, "lo"}, {2, "eth0"}, {0, NULL}};

struct if_nameindex *if_nameindex(void) {
    return g_interfaces;
}

void if_freenameindex(struct if_nameindex *ptr) {
    (void)ptr;
}

unsigned int if_nametoindex(const char *ifname) {
    if (!ifname)
        return 0;
    if (strcmp(ifname, "lo") == 0)
        return 1;
    if (strcmp(ifname, "eth0") == 0)
        return 2;
    return 0;
}

char *if_indextoname(unsigned int ifindex, char *ifname) {
    if (!ifname)
        return NULL;
    if (ifindex == 1) {
        strcpy(ifname, "lo");
        return ifname;
    }
    if (ifindex == 2) {
        strcpy(ifname, "eth0");
        return ifname;
    }
    return NULL;
}

int getifaddrs(struct ifaddrs **ifap) {
    if (!ifap)
        return -1;

    /* 1. Loopback (lo) */
    struct ifaddrs *lo = (struct ifaddrs *)malloc(sizeof(struct ifaddrs));
    if (!lo)
        return -1;
    memset(lo, 0, sizeof(struct ifaddrs));
    lo->ifa_name = "lo";
    lo->ifa_flags = IFF_UP | IFF_LOOPBACK | IFF_RUNNING;

    struct sockaddr_in *lo_addr = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
    if (lo_addr) {
        memset(lo_addr, 0, sizeof(struct sockaddr_in));
        lo_addr->sin_family = AF_INET;
        lo_addr->sin_addr.s_addr = 0x0100007F; /* 127.0.0.1 in network byte order */
        lo->ifa_addr = (struct sockaddr *)lo_addr;
    }
    struct sockaddr_in *lo_netmask = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
    if (lo_netmask) {
        memset(lo_netmask, 0, sizeof(struct sockaddr_in));
        lo_netmask->sin_family = AF_INET;
        lo_netmask->sin_addr.s_addr = 0x000000FF; /* 255.0.0.0 */
        lo->ifa_netmask = (struct sockaddr *)lo_netmask;
    }

    /* 2. Ethernet (eth0) */
    struct ifaddrs *eth0 = (struct ifaddrs *)malloc(sizeof(struct ifaddrs));
    if (!eth0) {
        *ifap = lo;
        return 0;
    }
    memset(eth0, 0, sizeof(struct ifaddrs));
    eth0->ifa_name = "eth0";
    eth0->ifa_flags = IFF_UP | IFF_RUNNING | IFF_BROADCAST | IFF_MULTICAST;

    struct sockaddr_in *eth_addr = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
    if (eth_addr) {
        memset(eth_addr, 0, sizeof(struct sockaddr_in));
        eth_addr->sin_family = AF_INET;
        eth_addr->sin_addr.s_addr = 0x0F02000A; /* 10.0.2.15 in network byte order */
        eth0->ifa_addr = (struct sockaddr *)eth_addr;
    }
    struct sockaddr_in *eth_netmask = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
    if (eth_netmask) {
        memset(eth_netmask, 0, sizeof(struct sockaddr_in));
        eth_netmask->sin_family = AF_INET;
        eth_netmask->sin_addr.s_addr = 0x00FFFFFF; /* 255.255.255.0 */
        eth0->ifa_netmask = (struct sockaddr *)eth_netmask;
    }

    lo->ifa_next = eth0;
    eth0->ifa_next = NULL;

    *ifap = lo;
    return 0;
}

void freeifaddrs(struct ifaddrs *ifa) {
    while (ifa) {
        struct ifaddrs *next = ifa->ifa_next;
        if (ifa->ifa_addr)
            free(ifa->ifa_addr);
        if (ifa->ifa_netmask)
            free(ifa->ifa_netmask);
        free(ifa);
        ifa = next;
    }
}
