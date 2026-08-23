/*
 * SzpontOS - /bin/nc (Netcat) TCP/UDP Networking Swiss Army Knife
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>

static void print_usage(void) {
    printf("Usage: nc [options] <host> <port>\n");
    printf("       nc -l -p <port>\n\n");
    printf("Options:\n");
    printf("  -l          Listen mode, for inbound connections\n");
    printf("  -p <port>   Local port number\n");
    printf("  -u          Use UDP instead of default TCP\n");
    printf("  -h          Display this help\n");
}

int main(int argc, char **argv) {
    int listen_mode = 0;
    int udp_mode = 0;
    int port = 0;
    const char *host = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-l") == 0) {
            listen_mode = 1;
        } else if (strcmp(argv[i], "-u") == 0) {
            udp_mode = 1;
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage();
            return 0;
        } else if (!host) {
            host = argv[i];
        } else if (port == 0) {
            port = atoi(argv[i]);
        }
    }

    if (listen_mode) {
        if (port == 0) {
            fprintf(stderr, "nc: must specify port with -p <port> in listen mode\n");
            return 1;
        }

        int sock_type = udp_mode ? SOCK_DGRAM : SOCK_STREAM;
        int sfd = socket(AF_INET, sock_type, 0);
        if (sfd < 0) {
            perror("nc: socket");
            return 1;
        }

        struct sockaddr_in sin;
        memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = INADDR_ANY;
        sin.sin_port = htons((uint16_t)port);

        if (bind(sfd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
            perror("nc: bind");
            close(sfd);
            return 1;
        }

        if (!udp_mode) {
            if (listen(sfd, 5) < 0) {
                perror("nc: listen");
                close(sfd);
                return 1;
            }
            printf("Listening on [0.0.0.0] (port %d)...\n", port);

            struct sockaddr_in client_addr;
            socklen_t clen = sizeof(client_addr);
            int cfd = accept(sfd, (struct sockaddr *)&client_addr, &clen);
            if (cfd < 0) {
                perror("nc: accept");
                close(sfd);
                return 1;
            }

            char c_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, c_ip, sizeof(c_ip));
            printf("Connection from [%s] port %d accepted\n", c_ip, ntohs(client_addr.sin_port));

            /* Bi-directional proxy between stdin/stdout and socket */
            char buf[512];
            while (1) {
                struct pollfd pfds[2];
                pfds[0].fd = 0;   /* stdin */
                pfds[0].events = POLLIN;
                pfds[1].fd = cfd; /* socket */
                pfds[1].events = POLLIN;

                int p = poll(pfds, 2, 100);
                if (p > 0) {
                    if (pfds[0].revents & POLLIN) {
                        ssize_t n = read(0, buf, sizeof(buf));
                        if (n <= 0) break;
                        send(cfd, buf, (size_t)n, 0);
                    }
                    if (pfds[1].revents & POLLIN) {
                        ssize_t n = recv(cfd, buf, sizeof(buf), 0);
                        if (n <= 0) break;
                        write(1, buf, (size_t)n);
                    }
                }
            }
            close(cfd);
            close(sfd);
            return 0;
        }
    } else {
        if (!host || port == 0) {
            print_usage();
            return 1;
        }

        struct hostent *he = gethostbyname(host);
        if (!he || !he->h_addr_list[0]) {
            fprintf(stderr, "nc: cannot resolve host %s\n", host);
            return 1;
        }

        int sock_type = udp_mode ? SOCK_DGRAM : SOCK_STREAM;
        int sfd = socket(AF_INET, sock_type, 0);
        if (sfd < 0) {
            perror("nc: socket");
            return 1;
        }

        struct sockaddr_in target_sin;
        memset(&target_sin, 0, sizeof(target_sin));
        target_sin.sin_family = AF_INET;
        memcpy(&target_sin.sin_addr, he->h_addr_list[0], sizeof(struct in_addr));
        target_sin.sin_port = htons((uint16_t)port);

        if (connect(sfd, (struct sockaddr *)&target_sin, sizeof(target_sin)) < 0) {
            perror("nc: connect");
            close(sfd);
            return 1;
        }

        char t_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &target_sin.sin_addr, t_ip, sizeof(t_ip));
        printf("Connected to %s [%s] port %d\n", host, t_ip, port);

        char buf[512];
        while (1) {
            struct pollfd pfds[2];
            pfds[0].fd = 0;   /* stdin */
            pfds[0].events = POLLIN;
            pfds[1].fd = sfd; /* socket */
            pfds[1].events = POLLIN;

            int p = poll(pfds, 2, 100);
            if (p > 0) {
                if (pfds[0].revents & POLLIN) {
                    ssize_t n = read(0, buf, sizeof(buf));
                    if (n <= 0) break;
                    send(sfd, buf, (size_t)n, 0);
                }
                if (pfds[1].revents & POLLIN) {
                    ssize_t n = recv(sfd, buf, sizeof(buf), 0);
                    if (n <= 0) break;
                    write(1, buf, (size_t)n);
                }
            }
        }
        close(sfd);
        return 0;
    }

    return 0;
}
