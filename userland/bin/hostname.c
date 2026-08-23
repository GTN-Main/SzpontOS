/*
 * SzpontOS - POSIX hostname utility
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    if (argc > 1) {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            printf("Usage: hostname [NAME]\n");
            printf("Show or set system hostname (stored in /etc/hostname).\n");
            return 0;
        }

        const char *new_host = argv[1];
        if (sethostname(new_host, strlen(new_host)) != 0) {
            perror("hostname");
            return 1;
        }
        return 0;
    }

    char host[256];
    if (gethostname(host, sizeof(host)) != 0) {
        perror("hostname");
        return 1;
    }

    printf("%s\n", host);
    return 0;
}
