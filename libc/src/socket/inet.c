/*
 * SzpontOS Libc - Internet Address Manipulation Routines (inet.c)
 * (C) Copyright by Szpont Industries. All rights reserved.
 * Inspired by FreeBSD libc/inet/inet_pton.c and inet_ntop.c
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int inet_aton(const char *cp, struct in_addr *pin) {
    if (!cp || !pin)
        return 0;

    uint32_t parts[4] = {0};
    int part_idx = 0;
    const char *p = cp;

    while (*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
        p++;

    for (part_idx = 0; part_idx < 4; part_idx++) {
        if (*p < '0' || *p > '9')
            return 0;

        uint32_t val = 0;
        while (*p >= '0' && *p <= '9') {
            val = val * 10 + (*p - '0');
            if (val > 255)
                return 0;
            p++;
        }
        parts[part_idx] = val;

        if (part_idx < 3) {
            if (*p != '.')
                return 0;
            p++;
        }
    }

    while (*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
        p++;

    if (*p != '\0')
        return 0;

    pin->s_addr = (uint32_t)((parts[0] & 0xFF) |
                            ((parts[1] & 0xFF) << 8) |
                            ((parts[2] & 0xFF) << 16) |
                            ((parts[3] & 0xFF) << 24));
    return 1;
}

in_addr_t inet_addr(const char *cp) {
    struct in_addr val;
    if (inet_aton(cp, &val))
        return val.s_addr;
    return INADDR_NONE;
}

char *inet_ntoa(struct in_addr in) {
    static char buf[18];
    uint8_t *bytes = (uint8_t *)&in.s_addr;
    snprintf(buf, sizeof(buf), "%u.%u.%u.%u", bytes[0], bytes[1], bytes[2], bytes[3]);
    return buf;
}

int inet_pton(int af, const char *src, void *dst) {
    if (!src || !dst) {
        errno = EAFNOSUPPORT;
        return -1;
    }

    if (af == AF_INET) {
        struct in_addr in;
        if (inet_aton(src, &in)) {
            memcpy(dst, &in.s_addr, sizeof(struct in_addr));
            return 1;
        }
        return 0;
    }

    errno = EAFNOSUPPORT;
    return -1;
}

const char *inet_ntop(int af, const void *src, char *dst, socklen_t size) {
    if (!src || !dst) {
        errno = EINVAL;
        return NULL;
    }

    if (af == AF_INET) {
        if (size < INET_ADDRSTRLEN) {
            errno = ENOSPC;
            return NULL;
        }
        const uint8_t *bytes = (const uint8_t *)src;
        snprintf(dst, size, "%u.%u.%u.%u", bytes[0], bytes[1], bytes[2], bytes[3]);
        return dst;
    }

    errno = EAFNOSUPPORT;
    return NULL;
}
