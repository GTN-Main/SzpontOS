/*
 * SzpontOS - /bin/httpget (Direct Socket HTTP Client)
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: httpget <domain_or_ip> [port] [path]\n");
        return 1;
    }

    const char *host = argv[1];
    int port = (argc > 2) ? atoi(argv[2]) : 80;
    const char *path = (argc > 3) ? argv[3] : "/";

    printf("[httpget] Resolving '%s'...\n", host);
    struct hostent *he = gethostbyname(host);
    if (!he) {
        printf("[httpget] Failed to resolve hostname '%s'\n", host);
        return 1;
    }

    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, he->h_addr_list[0], ip_str, sizeof(ip_str));
    printf("[httpget] Connecting to %s:%d...\n", ip_str, port);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons((uint16_t)port);
    memcpy(&serv_addr.sin_addr, he->h_addr_list[0], sizeof(struct in_addr));

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        close(sockfd);
        return 1;
    }
    printf("[httpget] Connected successfully!\n");

    char request[512];
    int req_len = snprintf(request, sizeof(request),
                           "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: SzpontOS-httpget/1.0\r\nConnection: close\r\n\r\n",
                           path, host);

    printf("[httpget] Sending %d bytes HTTP request...\n", req_len);
    ssize_t sent = send(sockfd, request, (size_t)req_len, 0);
    if (sent != req_len) {
        perror("send");
        close(sockfd);
        return 1;
    }
    printf("[httpget] Request sent, reading response...\n\n");

    char buffer[1024];
    ssize_t n;
    while ((n = recv(sockfd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[n] = '\0';
        printf("%s", buffer);
    }

    printf("\n[httpget] Transfer complete.\n");
    close(sockfd);
    return 0;
}
