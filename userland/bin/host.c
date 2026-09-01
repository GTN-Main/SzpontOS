/*
 * SzpontOS - /bin/host (DNS Lookup Diagnostic Tool)
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <resolv.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: host <domain_name>\n");
        return 1;
    }

    const char *domain = argv[1];
    printf("[host] Resolving '%s'...\n", domain);

    res_init();
    printf("[host] Nameserver count: %d\n", _res.nscount);
    for (int i = 0; i < _res.nscount; i++) {
        char ns_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &_res.nsaddr_list[i].sin_addr, ns_ip, sizeof(ns_ip));
        printf("[host] Nameserver #%d: %s:%d\n", i, ns_ip, ntohs(_res.nsaddr_list[i].sin_port));
    }

    unsigned char ans[PACKETSZ];
    memset(ans, 0, sizeof(ans));

    printf("[host] Calling res_query('%s', C_IN, T_A)...\n", domain);
    int anslen = res_query(domain, C_IN, T_A, ans, sizeof(ans));
    printf("[host] res_query returned: %d bytes\n", anslen);

    if (anslen > 0) {
        HEADER *hp = (HEADER *)ans;
        printf("[host] DNS Response Header: ID=0x%04x Flags=0x%04x QD=%d AN=%d NS=%d AR=%d\n",
               ntohs(hp->id), ntohs(hp->flags), ntohs(hp->qdcount), ntohs(hp->ancount),
               ntohs(hp->nscount), ntohs(hp->arcount));
    }

    struct hostent *he = gethostbyname(domain);
    if (!he) {
        printf("[host] gethostbyname('%s') failed (h_errno=%d)\n", domain, h_errno);
        return 1;
    }

    printf("[host] gethostbyname SUCCESS: Official name: %s\n", he->h_name);
    for (int i = 0; he->h_addr_list[i] != NULL; i++) {
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, he->h_addr_list[i], ip, sizeof(ip));
        printf("[host] %s has address %s\n", domain, ip);
    }

    return 0;
}
