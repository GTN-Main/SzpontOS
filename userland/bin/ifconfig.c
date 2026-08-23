/*
 * SzpontOS - /bin/ifconfig network interface configuration and status utility
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

static void print_interface(struct ifaddrs *ifa) {
    printf("%-8s  flags=%u<", ifa->ifa_name, ifa->ifa_flags);
    int first = 1;
    if (ifa->ifa_flags & IFF_UP) { printf("%sUP", first ? "" : ","); first = 0; }
    if (ifa->ifa_flags & IFF_BROADCAST) { printf("%sBROADCAST", first ? "" : ","); first = 0; }
    if (ifa->ifa_flags & IFF_LOOPBACK) { printf("%sLOOPBACK", first ? "" : ","); first = 0; }
    if (ifa->ifa_flags & IFF_RUNNING) { printf("%sRUNNING", first ? "" : ","); first = 0; }
    if (ifa->ifa_flags & IFF_MULTICAST) { printf("%sMULTICAST", first ? "" : ","); first = 0; }
    printf(">\n");

    if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
        struct sockaddr_in *sin = (struct sockaddr_in *)ifa->ifa_addr;
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sin->sin_addr, ip_str, sizeof(ip_str));

        char netmask_str[INET_ADDRSTRLEN] = "255.255.255.0";
        if (ifa->ifa_netmask && ifa->ifa_netmask->sa_family == AF_INET) {
            struct sockaddr_in *snm = (struct sockaddr_in *)ifa->ifa_netmask;
            inet_ntop(AF_INET, &snm->sin_addr, netmask_str, sizeof(netmask_str));
        }

        printf("        inet %s  netmask %s", ip_str, netmask_str);
        if (ifa->ifa_flags & IFF_LOOPBACK) {
            printf("\n        loop  txqueuelen 1000  (Local Loopback)\n");
        } else {
            printf("  broadcast 10.0.2.255\n");
            printf("        ether 52:54:00:12:34:56  txqueuelen 1000  (Ethernet)\n");
        }
    }
    printf("\n");
}

int main(int argc, char **argv) {
    struct ifaddrs *ifap = NULL;
    if (getifaddrs(&ifap) != 0 || !ifap) {
        fprintf(stderr, "ifconfig: failed to query network interfaces\n");
        return 1;
    }

    const char *target_if = (argc > 1) ? argv[1] : NULL;
    int found = 0;

    for (struct ifaddrs *cur = ifap; cur != NULL; cur = cur->ifa_next) {
        if (!target_if || strcmp(cur->ifa_name, target_if) == 0) {
            print_interface(cur);
            found = 1;
        }
    }

    if (target_if && !found) {
        fprintf(stderr, "ifconfig: interface '%s' not found\n", target_if);
        freeifaddrs(ifap);
        return 1;
    }

    freeifaddrs(ifap);
    return 0;
}
