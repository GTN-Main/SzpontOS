/*
 * SzpontOS Libc - Network Database & DNS Resolver (getaddrinfo, gethostbyname, inet_*)
 * (C) Copyright by Szpont Industries. All rights reserved.
 * Inspired by FreeBSD libc/inet/inet_pton.c and inet_ntop.c
 */

#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

static struct hostent g_static_hostent;
static char *g_static_aliases[2] = { NULL, NULL };
static char *g_static_addr_list[2] = { NULL, NULL };
static uint32_t g_static_addr = 0;

static int inet_pton4_bsd(const char *src, unsigned char *dst) {
    static const char digits[] = "0123456789";
    int saw_digit = 0;
    int octets = 0;
    int ch;
    unsigned char tmp[4], *tp;

    tp = tmp;
    *tp = 0;

    while ((ch = *src++) != '\0') {
        const char *pch;
        if ((pch = strchr(digits, ch)) != NULL) {
            unsigned int new_val = (unsigned int)(*tp * 10 + (pch - digits));
            if (saw_digit && *tp == 0) return 0;
            if (new_val > 255) return 0;
            *tp = (unsigned char)new_val;
            if (!saw_digit) {
                if (++octets > 4) return 0;
                saw_digit = 1;
            }
        } else if (ch == '.' && saw_digit) {
            if (octets == 4) return 0;
            *++tp = 0;
            saw_digit = 0;
        } else {
            return 0;
        }
    }
    if (octets < 4) return 0;
    memcpy(dst, tmp, 4);
    return 1;
}

int inet_pton(int af, const char *src, void *dst) {
    if (!src || !dst) {
        errno = EFAULT;
        return 0;
    }
    if (af == AF_INET) {
        return inet_pton4_bsd(src, (unsigned char *)dst);
    }
    if (af == AF_INET6) {
        if (strcmp(src, "::1") == 0) {
            memset(dst, 0, 16);
            ((unsigned char *)dst)[15] = 1;
            return 1;
        }
        return 0;
    }
    errno = EAFNOSUPPORT;
    return -1;
}

const char *inet_ntop(int af, const void *src, char *dst, socklen_t size) {
    if (!src || !dst) {
        errno = EFAULT;
        return NULL;
    }
    if (af == AF_INET) {
        const unsigned char *p = (const unsigned char *)src;
        char tmp[32];
        int l = snprintf(tmp, sizeof(tmp), "%u.%u.%u.%u", p[0], p[1], p[2], p[3]);
        if (l <= 0 || (socklen_t)l >= size) {
            errno = ENOSPC;
            return NULL;
        }
        strcpy(dst, tmp);
        return dst;
    }
    if (af == AF_INET6) {
        if (size < INET6_ADDRSTRLEN) {
            errno = ENOSPC;
            return NULL;
        }
        strcpy(dst, "::1");
        return dst;
    }
    errno = EAFNOSUPPORT;
    return NULL;
}

char *inet_ntoa(struct in_addr in) {
    static char buf[INET_ADDRSTRLEN];
    const unsigned char *p = (const unsigned char *)&in.s_addr;
    snprintf(buf, sizeof(buf), "%u.%u.%u.%u", p[0], p[1], p[2], p[3]);
    return buf;
}

in_addr_t inet_addr(const char *cp) {
    struct in_addr in;
    if (inet_pton(AF_INET, cp, &in) == 1) {
        return in.s_addr;
    }
    return (in_addr_t)-1;
}

struct hostent *gethostbyname(const char *name) {
    if (!name) return NULL;

    if (strcmp(name, "localhost") == 0) {
        g_static_addr = 0x0100007F; /* 127.0.0.1 */
    } else if (strcmp(name, "szpontos-box") == 0) {
        g_static_addr = 0x0F02000A; /* 10.0.2.15 */
    } else if (strcmp(name, "gateway") == 0) {
        g_static_addr = 0x0202000A; /* 10.0.2.2 */
    } else {
        struct in_addr in;
        if (inet_pton(AF_INET, name, &in) == 1) {
            g_static_addr = in.s_addr;
        } else {
            return NULL;
        }
    }

    g_static_hostent.h_name = (char *)name;
    g_static_hostent.h_aliases = g_static_aliases;
    g_static_hostent.h_addrtype = AF_INET;
    g_static_hostent.h_length = 4;
    g_static_addr_list[0] = (char *)&g_static_addr;
    g_static_addr_list[1] = NULL;
    g_static_hostent.h_addr_list = g_static_addr_list;

    return &g_static_hostent;
}

int getaddrinfo(const char *node, const char *service,
                const struct addrinfo *hints,
                struct addrinfo **res) {
    if (!res) return EAI_NONAME;
    if (!node && !service) return EAI_NONAME;

    uint32_t ip = 0;
    uint16_t port = 0;

    if (service) {
        int p = atoi(service);
        if (p > 0 && p <= 65535) {
            port = (uint16_t)p;
        } else if (strcmp(service, "http") == 0) {
            port = 80;
        } else if (strcmp(service, "https") == 0) {
            port = 443;
        } else if (strcmp(service, "ssh") == 0) {
            port = 22;
        } else if (strcmp(service, "domain") == 0) {
            port = 53;
        } else if (strcmp(service, "echo") == 0) {
            port = 7;
        }
    }

    if (node) {
        if (strcmp(node, "localhost") == 0) {
            ip = 0x0100007F;
        } else if (strcmp(node, "0.0.0.0") == 0) {
            ip = 0;
        } else if (strcmp(node, "gateway") == 0) {
            ip = 0x0202000A;
        } else if (strcmp(node, "szpontos-box") == 0) {
            ip = 0x0F02000A;
        } else {
            struct in_addr in;
            if (inet_pton(AF_INET, node, &in) == 1) {
                ip = in.s_addr;
            } else {
                return EAI_NONAME;
            }
        }
    } else {
        if (hints && (hints->ai_flags & AI_PASSIVE)) {
            ip = 0;
        } else {
            ip = 0x0100007F;
        }
    }

    struct addrinfo *ai = (struct addrinfo *)malloc(sizeof(struct addrinfo));
    if (!ai) return EAI_MEMORY;
    memset(ai, 0, sizeof(struct addrinfo));

    ai->ai_family = AF_INET;
    ai->ai_socktype = hints ? hints->ai_socktype : SOCK_STREAM;
    ai->ai_protocol = hints ? hints->ai_protocol : 0;

    struct sockaddr_in *sa = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
    if (!sa) {
        free(ai);
        return EAI_MEMORY;
    }
    memset(sa, 0, sizeof(struct sockaddr_in));
    sa->sin_family = AF_INET;
    sa->sin_port = htons(port);
    sa->sin_addr.s_addr = ip;

    ai->ai_addr = (struct sockaddr *)sa;
    ai->ai_addrlen = sizeof(struct sockaddr_in);
    *res = ai;

    return 0;
}

void freeaddrinfo(struct addrinfo *res) {
    while (res) {
        struct addrinfo *next = res->ai_next;
        if (res->ai_addr) free(res->ai_addr);
        if (res->ai_canonname) free(res->ai_canonname);
        free(res);
        res = next;
    }
}

const char *gai_strerror(int errcode) {
    switch (errcode) {
        case 0: return "Success";
        case EAI_BADFLAGS: return "Invalid value for ai_flags";
        case EAI_NONAME: return "Name or service not known";
        case EAI_FAMILY: return "ai_family not supported";
        case EAI_SOCKTYPE: return "ai_socktype not supported";
        case EAI_MEMORY: return "Memory allocation failure";
        case EAI_SYSTEM: return "System error";
        default: return "Unknown resolver error";
    }
}
