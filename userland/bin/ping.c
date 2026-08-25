/*
 * SzpontOS - /bin/ping (ICMP Echo Utility)
 * (C) Copyright by Szpont Industries. All rights reserved.
 * Inspired by FreeBSD sbin/ping/ping.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <sys/time.h>

static uint16_t icmp_checksum(const void *data, size_t len) {
    const uint16_t *buf = (const uint16_t *)data;
    uint32_t sum = 0;
    while (len > 1) {
        sum += *buf++;
        len -= 2;
    }
    if (len == 1) {
        sum += *(const uint8_t *)buf;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t)(~sum);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: ping <host_or_ip> [-c count]\n");
        return 1;
    }

    const char *host = argv[1];
    int count = 4;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            count = atoi(argv[i + 1]);
            i++;
        }
    }

    struct hostent *he = gethostbyname(host);
    if (!he || !he->h_addr_list[0]) {
        fprintf(stderr, "ping: unknown host %s\n", host);
        return 1;
    }

    struct in_addr target_addr;
    memcpy(&target_addr, he->h_addr_list[0], sizeof(struct in_addr));
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &target_addr, ip_str, sizeof(ip_str));

    int sock = socket(AF_INET, SOCK_RAW, 1 /* IPPROTO_ICMP */);
    if (sock < 0) {
        /* Fallback to DGRAM */
        sock = socket(AF_INET, SOCK_DGRAM, 1);
    }
    if (sock < 0) {
        fprintf(stderr, "ping: cannot create ICMP socket\n");
        return 1;
    }

    printf("PING %s (%s) 56(84) bytes of data.\n", host, ip_str);

    int transmitted = 0;
    int received = 0;
    uint16_t ping_id = (uint16_t)(getpid() & 0xFFFF);

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr = target_addr;

    double min_rtt = 999999.0;
    double max_rtt = 0.0;
    double sum_rtt = 0.0;

    for (int seq = 1; seq <= count; seq++) {
        transmitted++;

        /* Build ICMP Echo Request Packet (64 bytes total: 8 bytes header + 56 bytes payload) */
        uint8_t packet[64];
        memset(packet, 0, sizeof(packet));

        struct icmphdr *icmp = (struct icmphdr *)packet;
        icmp->icmp_type = ICMP_ECHO;
        icmp->icmp_code = 0;
        icmp->icmp_cksum = 0;
        icmp->un.echo.id = htons(ping_id);
        icmp->un.echo.sequence = htons((uint16_t)seq);

        /* Fill payload with test pattern */
        for (size_t i = sizeof(struct icmphdr); i < sizeof(packet); i++) {
            packet[i] = (uint8_t)(i & 0xFF);
        }

        icmp->icmp_cksum = icmp_checksum(packet, sizeof(packet));

        struct timespec ts_start, ts_end;
        clock_gettime(CLOCK_MONOTONIC, &ts_start);

        ssize_t sent = sendto(sock, packet, sizeof(packet), 0, (struct sockaddr *)&sin, sizeof(sin));
        if (sent < 0) {
            printf("ping: sendto failed for icmp_seq=%d\n", seq);
            continue;
        }

        /* Wait up to 1000 ms for reply */
        uint8_t recv_buf[128];
        struct sockaddr_in from_addr;
        uint32_t from_len = sizeof(from_addr);
        int got_reply = 0;

        for (int t = 0; t < 100; t++) {
            struct pollfd pfd;
            pfd.fd = sock;
            pfd.events = POLLIN;
            pfd.revents = 0;

            int ret = poll(&pfd, 1, 10);
            if (ret > 0 || (pfd.revents & POLLIN)) {
                ssize_t n = recvfrom(sock, recv_buf, sizeof(recv_buf), 0, (struct sockaddr *)&from_addr, &from_len);
                if (n >= (ssize_t)sizeof(struct icmphdr)) {
                    struct icmphdr *reply_icmp = (struct icmphdr *)recv_buf;
                    if (reply_icmp->icmp_type == ICMP_ECHOREPLY) {
                        clock_gettime(CLOCK_MONOTONIC, &ts_end);
                        double rtt_ms = (double)(ts_end.tv_sec - ts_start.tv_sec) * 1000.0 +
                                        (double)(ts_end.tv_nsec - ts_start.tv_nsec) / 1000000.0;

                        char reply_ip[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &from_addr.sin_addr, reply_ip, sizeof(reply_ip));
                        received++;
                        printf("64 bytes from %s: icmp_seq=%d ttl=64 time=%.3f ms\n", reply_ip, seq, rtt_ms);

                        if (rtt_ms < min_rtt)
                            min_rtt = rtt_ms;
                        if (rtt_ms > max_rtt)
                            max_rtt = rtt_ms;
                        sum_rtt += rtt_ms;
                        got_reply = 1;
                        break;
                    }
                }
            } else {
                ssize_t n = recvfrom(sock, recv_buf, sizeof(recv_buf), 0x40 /* MSG_DONTWAIT */,
                                     (struct sockaddr *)&from_addr, &from_len);
                if (n >= (ssize_t)sizeof(struct icmphdr)) {
                    struct icmphdr *reply_icmp = (struct icmphdr *)recv_buf;
                    if (reply_icmp->icmp_type == ICMP_ECHOREPLY) {
                        clock_gettime(CLOCK_MONOTONIC, &ts_end);
                        double rtt_ms = (double)(ts_end.tv_sec - ts_start.tv_sec) * 1000.0 +
                                        (double)(ts_end.tv_nsec - ts_start.tv_nsec) / 1000000.0;

                        char reply_ip[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &from_addr.sin_addr, reply_ip, sizeof(reply_ip));
                        received++;
                        printf("64 bytes from %s: icmp_seq=%d ttl=64 time=%.3f ms\n", reply_ip, seq, rtt_ms);

                        if (rtt_ms < min_rtt)
                            min_rtt = rtt_ms;
                        if (rtt_ms > max_rtt)
                            max_rtt = rtt_ms;
                        sum_rtt += rtt_ms;
                        got_reply = 1;
                        break;
                    }
                }
            }
            for (volatile int d = 0; d < 50000; d++) {
            }
        }

        if (!got_reply) {
            printf("Request timeout for icmp_seq %d\n", seq);
        }

        if (seq < count) {
            /* Small delay between pings */
            for (volatile int d = 0; d < 3000000; d++) {
            }
        }
    }

    close(sock);

    int loss = (transmitted > 0) ? ((transmitted - received) * 100) / transmitted : 0;
    printf("\n--- %s ping statistics ---\n", host);
    printf("%d packets transmitted, %d received, %d%% packet loss\n", transmitted, received, loss);
    if (received > 0) {
        double avg_rtt = sum_rtt / received;
        printf("rtt min/avg/max = %.3f/%.3f/%.3f ms\n", min_rtt, avg_rtt, max_rtt);
    }

    return (received > 0) ? 0 : 1;
}
