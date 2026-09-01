/*
 * SzpontOS Libc - Network Database & POSIX Resolver API (getaddrinfo, gethostbyname, etc.)
 * (C) Copyright by Szpont Industries. All rights reserved.
 * Inspired by FreeBSD libc/net/getaddrinfo.c and gethostnamadr.c
 */

#include <netdb.h>
#include <resolv.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

extern int hosts_lookup(const char *name, uint32_t *out_ip);
extern int nss_get_hosts_order(int *order, int max_order);

static struct hostent g_static_hostent;
static char *g_static_aliases[2] = {NULL, NULL};
static char *g_static_addr_list[2] = {NULL, NULL};
static uint32_t g_static_addr = 0;
static char g_static_hostname[256];

int h_errno = 0;

/*
 * DNS Response Parser (extracts IPv4 address from Answer section)
 */
static int parse_dns_response_ipv4(const unsigned char *resp, int len, uint32_t *out_ip) {
    if (!resp || len <= HFIXEDSZ || !out_ip)
        return -1;

    HEADER *hp = (HEADER *)resp;
    uint16_t flags = ntohs(hp->flags);
    uint16_t rcode = flags & 0x000F;
    uint16_t ancount = ntohs(hp->ancount);
    if (ancount == 0 || rcode != NOERROR)
        return -1;

    size_t rpos = HFIXEDSZ;
    uint16_t qdcount = ntohs(hp->qdcount);

    /* Skip Question Section */
    for (uint16_t q = 0; q < qdcount && rpos < (size_t)len; q++) {
        while (rpos < (size_t)len) {
            if ((resp[rpos] & 0xC0) == 0xC0) {
                rpos += 2;
                break;
            }
            if (resp[rpos] == 0) {
                rpos++;
                break;
            }
            rpos += 1 + resp[rpos];
        }
        rpos += QFIXEDSZ; /* QTYPE + QCLASS */
    }

    /* Parse Answers */
    for (uint16_t a = 0; a < ancount && rpos < (size_t)len; a++) {
        /* Skip Name */
        while (rpos < (size_t)len) {
            if ((resp[rpos] & 0xC0) == 0xC0) {
                rpos += 2;
                break;
            }
            if (resp[rpos] == 0) {
                rpos++;
                break;
            }
            rpos += 1 + resp[rpos];
        }

        if (rpos + 10 > (size_t)len)
            break;

        uint16_t type = (uint16_t)((resp[rpos] << 8) | resp[rpos + 1]);
        uint16_t rdlen = (uint16_t)((resp[rpos + 8] << 8) | resp[rpos + 9]);
        rpos += 10;

        if (type == T_A && rdlen == 4 && rpos + 4 <= (size_t)len) {
            memcpy(out_ip, &resp[rpos], 4);
            return 0;
        }

        rpos += rdlen;
    }

    return -1;
}

/*
 * Unified Host Resolution (follows /etc/nsswitch.conf: files -> dns)
 */
static int resolve_hostname_ipv4(const char *hostname, uint32_t *out_ip) {
    if (!hostname || !*hostname || !out_ip)
        return -1;

    char clean_host[256];
    strncpy(clean_host, hostname, sizeof(clean_host) - 1);
    clean_host[sizeof(clean_host) - 1] = '\0';
    char *colon = strchr(clean_host, ':');
    if (colon) {
        *colon = '\0';
    }

    /* 1. Check if numeric IPv4 address directly */
    struct in_addr in;
    if (inet_aton(clean_host, &in)) {
        *out_ip = in.s_addr;
        return 0;
    }

    /* 2. Execute NSS order */
    int order[4];
    int nsources = nss_get_hosts_order(order, 4);

    for (int i = 0; i < nsources; i++) {
        if (order[i] == 1 /* NSS_SRC_FILES */) {
            if (hosts_lookup(clean_host, out_ip) == 0) {
                return 0;
            }
        } else if (order[i] == 2 /* NSS_SRC_DNS */) {
            unsigned char ans[PACKETSZ];
            int anslen = res_query(clean_host, C_IN, T_A, ans, sizeof(ans));
            if (anslen > 0) {
                if (parse_dns_response_ipv4(ans, anslen, out_ip) == 0) {
                    return 0;
                }
            }
        }
    }

    return -1;
}

/*
 * POSIX getaddrinfo
 */
int getaddrinfo(const char *node, const char *service,
                const struct addrinfo *hints, struct addrinfo **res) {
    if (!res)
        return EAI_FAIL;
    *res = NULL;

    if (!node && !service)
        return EAI_NONAME;

    int socktype = SOCK_STREAM;
    int protocol = IPPROTO_TCP;
    int family = AF_INET;

    if (hints) {
        if (hints->ai_family != AF_UNSPEC && hints->ai_family != AF_INET)
            return EAI_FAMILY;
        if (hints->ai_socktype)
            socktype = hints->ai_socktype;
        if (hints->ai_protocol)
            protocol = hints->ai_protocol;
    }

    uint16_t port = 0;
    if (service) {
        char *endptr = NULL;
        long p = strtol(service, &endptr, 10);
        if (*service && *endptr == '\0') {
            port = (uint16_t)p;
        } else {
            /* Standard well-known service mapping */
            if (strcasecmp(service, "http") == 0) port = 80;
            else if (strcasecmp(service, "https") == 0) port = 443;
            else if (strcasecmp(service, "ssh") == 0) port = 22;
            else if (strcasecmp(service, "ftp") == 0) port = 21;
            else if (strcasecmp(service, "telnet") == 0) port = 23;
            else if (strcasecmp(service, "domain") == 0 || strcasecmp(service, "dns") == 0) port = 53;
            else if (strcasecmp(service, "smtp") == 0) port = 25;
            else if (strcasecmp(service, "ntp") == 0) port = 123;
            else return EAI_SERVICE;
        }
    } else if (node) {
        const char *col = strchr(node, ':');
        if (col) {
            port = (uint16_t)atoi(col + 1);
        }
    }

    uint32_t ip = 0;
    if (node) {
        if (hints && (hints->ai_flags & AI_NUMERICHOST)) {
            struct in_addr in;
            if (!inet_aton(node, &in))
                return EAI_NONAME;
            ip = in.s_addr;
        } else {
            if (resolve_hostname_ipv4(node, &ip) != 0)
                return EAI_NONAME;
        }
    } else {
        if (hints && (hints->ai_flags & AI_PASSIVE)) {
            ip = INADDR_ANY;
        } else {
            ip = htonl(INADDR_LOOPBACK);
        }
    }

    struct addrinfo *ai = (struct addrinfo *)calloc(1, sizeof(struct addrinfo));
    struct sockaddr_in *sin = (struct sockaddr_in *)calloc(1, sizeof(struct sockaddr_in));
    if (!ai || !sin) {
        free(ai);
        free(sin);
        return EAI_MEMORY;
    }

    sin->sin_family = AF_INET;
    sin->sin_port = htons(port);
    sin->sin_addr.s_addr = ip;

    ai->ai_family = family;
    ai->ai_socktype = socktype;
    ai->ai_protocol = protocol;
    ai->ai_addrlen = sizeof(struct sockaddr_in);
    ai->ai_addr = (struct sockaddr *)sin;

    if (node && (hints && (hints->ai_flags & AI_CANONNAME))) {
        ai->ai_canonname = strdup(node);
    }

    *res = ai;
    return 0;
}

void freeaddrinfo(struct addrinfo *ai) {
    while (ai) {
        struct addrinfo *next = ai->ai_next;
        if (ai->ai_addr)
            free(ai->ai_addr);
        if (ai->ai_canonname)
            free(ai->ai_canonname);
        free(ai);
        ai = next;
    }
}

/*
 * Legacy BSD gethostbyname API
 */
struct hostent *gethostbyname(const char *name) {
    if (!name || !*name) {
        h_errno = HOST_NOT_FOUND;
        return NULL;
    }

    if (resolve_hostname_ipv4(name, &g_static_addr) != 0) {
        h_errno = HOST_NOT_FOUND;
        return NULL;
    }

    strncpy(g_static_hostname, name, sizeof(g_static_hostname) - 1);
    g_static_addr_list[0] = (char *)&g_static_addr;
    g_static_addr_list[1] = NULL;
    g_static_aliases[0] = NULL;

    g_static_hostent.h_name = g_static_hostname;
    g_static_hostent.h_aliases = g_static_aliases;
    g_static_hostent.h_addrtype = AF_INET;
    g_static_hostent.h_length = sizeof(uint32_t);
    g_static_hostent.h_addr_list = g_static_addr_list;

    h_errno = 0;
    return &g_static_hostent;
}

struct hostent *gethostbyname2(const char *name, int af) {
    if (af != AF_INET) {
        h_errno = NO_RECOVERY;
        return NULL;
    }
    return gethostbyname(name);
}

struct hostent *gethostbyaddr(const void *addr, socklen_t len, int type) {
    if (!addr || len < sizeof(struct in_addr) || type != AF_INET) {
        h_errno = HOST_NOT_FOUND;
        return NULL;
    }

    memcpy(&g_static_addr, addr, sizeof(uint32_t));
    struct in_addr in;
    in.s_addr = g_static_addr;
    const char *s = inet_ntoa(in);
    strncpy(g_static_hostname, s ? s : "unknown", sizeof(g_static_hostname) - 1);

    g_static_addr_list[0] = (char *)&g_static_addr;
    g_static_addr_list[1] = NULL;
    g_static_aliases[0] = NULL;

    g_static_hostent.h_name = g_static_hostname;
    g_static_hostent.h_aliases = g_static_aliases;
    g_static_hostent.h_addrtype = AF_INET;
    g_static_hostent.h_length = sizeof(uint32_t);
    g_static_hostent.h_addr_list = g_static_addr_list;

    h_errno = 0;
    return &g_static_hostent;
}

int getnameinfo(const struct sockaddr *sa, socklen_t salen,
                char *host, socklen_t hostlen,
                char *serv, socklen_t servlen, int flags) {
    (void)flags;
    if (!sa || salen < sizeof(struct sockaddr_in))
        return EAI_FAMILY;

    const struct sockaddr_in *sin = (const struct sockaddr_in *)sa;

    if (host && hostlen > 0) {
        const char *s = inet_ntop(AF_INET, &sin->sin_addr, host, hostlen);
        if (!s) return EAI_SYSTEM;
    }

    if (serv && servlen > 0) {
        snprintf(serv, servlen, "%u", ntohs(sin->sin_port));
    }

    return 0;
}

const char *gai_strerror(int ecode) {
    switch (ecode) {
    case 0: return "Success";
    case EAI_BADFLAGS: return "Bad ai_flags";
    case EAI_NONAME: return "Name or service not known";
    case EAI_AGAIN: return "Temporary failure in name resolution";
    case EAI_FAIL: return "Non-recoverable failure in name resolution";
    case EAI_FAMILY: return "ai_family not supported";
    case EAI_SOCKTYPE: return "ai_socktype not supported";
    case EAI_SERVICE: return "Servname not supported for ai_socktype";
    case EAI_MEMORY: return "Memory allocation failure";
    case EAI_SYSTEM: return "System error";
    case EAI_OVERFLOW: return "Argument buffer overflow";
    default: return "Unknown error";
    }
}

const char *hstrerror(int err) {
    switch (err) {
    case 0: return "Resolver Error 0 (no error)";
    case HOST_NOT_FOUND: return "Unknown host";
    case TRY_AGAIN: return "Host name lookup failure";
    case NO_RECOVERY: return "Unknown server error";
    case NO_DATA: return "No address associated with name";
    default: return "Unknown resolver error";
    }
}

void herror(const char *s) {
    if (s && *s) {
        fprintf(stderr, "%s: %s\n", s, hstrerror(h_errno));
    } else {
        fprintf(stderr, "%s\n", hstrerror(h_errno));
    }
}

static struct servent g_static_servent;
static char g_static_servname[64];
static char g_static_servproto[16];
static char *g_static_servaliases[2] = {NULL, NULL};

struct servent *getservbyname(const char *name, const char *proto) {
    if (!name || !*name)
        return NULL;

    int port = 0;
    const char *p = proto ? proto : "tcp";

    if (strcasecmp(name, "http") == 0) port = 80;
    else if (strcasecmp(name, "https") == 0) port = 443;
    else if (strcasecmp(name, "ssh") == 0) port = 22;
    else if (strcasecmp(name, "ftp") == 0) port = 21;
    else if (strcasecmp(name, "telnet") == 0) port = 23;
    else if (strcasecmp(name, "domain") == 0 || strcasecmp(name, "dns") == 0) port = 53;
    else if (strcasecmp(name, "smtp") == 0) port = 25;
    else if (strcasecmp(name, "ntp") == 0) port = 123;
    else if (strcasecmp(name, "x11") == 0) port = 6000;
    else return NULL;

    strncpy(g_static_servname, name, sizeof(g_static_servname) - 1);
    strncpy(g_static_servproto, p, sizeof(g_static_servproto) - 1);
    g_static_servent.s_name = g_static_servname;
    g_static_servent.s_aliases = g_static_servaliases;
    g_static_servent.s_port = htons((uint16_t)port);
    g_static_servent.s_proto = g_static_servproto;

    return &g_static_servent;
}

struct servent *getservbyport(int port, const char *proto) {
    uint16_t hport = ntohs((uint16_t)port);
    const char *name = NULL;
    const char *p = proto ? proto : "tcp";

    switch (hport) {
    case 80: name = "http"; break;
    case 443: name = "https"; break;
    case 22: name = "ssh"; break;
    case 21: name = "ftp"; break;
    case 23: name = "telnet"; break;
    case 53: name = "domain"; break;
    case 25: name = "smtp"; break;
    case 123: name = "ntp"; break;
    case 6000: name = "x11"; break;
    default: return NULL;
    }

    strncpy(g_static_servname, name, sizeof(g_static_servname) - 1);
    strncpy(g_static_servproto, p, sizeof(g_static_servproto) - 1);
    g_static_servent.s_name = g_static_servname;
    g_static_servent.s_aliases = g_static_servaliases;
    g_static_servent.s_port = (int)(uint16_t)port;
    g_static_servent.s_proto = g_static_servproto;

    return &g_static_servent;
}

void setservent(int stayopen) {
    (void)stayopen;
}

void endservent(void) {
}

