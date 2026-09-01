/*
 * SzpontOS Libc - Standard Resolver Interface (<resolv.h>)
 * (C) Copyright by Szpont Industries. All rights reserved.
 * Inspired by FreeBSD sys/net/resolv.h and BIND 8/9 API.
 */

#ifndef _RESOLV_H_
#define _RESOLV_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/nameser.h>

#define MAXNS           3   /* Max # name servers we'll track */
#define MAXDFLSRCH      3   /* # default domain levels to try */
#define MAXDNSRCH       6   /* Max # domains in search path */
#define LOCALDOMAINPARTS 2  /* Min levels in name that is "local" */
#define RES_TIMEOUT     2   /* Min. seconds between retries */
#define MAXRESOLVSORT   10  /* Number of sortbuf entries */
#define RES_MAXNDOTS    15  /* Max number of dots in name */
#define RES_MAXRETRANS  30  /* Max retransmissions */
#define RES_MAXRETRY    5   /* Max number of retries */
#define RES_DFLRETRY    2   /* Default # of retries */

/*
 * State of the resolver.
 */
struct __res_state {
    int retrans;                        /* Retransmission time interval */
    int retry;                          /* Number of times to retransmit */
    unsigned long options;              /* Option flags */
    int nscount;                        /* Number of name servers */
    struct sockaddr_in nsaddr_list[MAXNS]; /* Address of name server(s) */
    unsigned short id;                  /* Current message id */
    char defdname[256];                 /* Default domain name */
    char *dnsrch[MAXDNSRCH + 1];        /* Components of domain to search */
    int initialized;                    /* Nonzero if res_init has run */
};

typedef struct __res_state *res_state;

extern struct __res_state _res;

/* Resolver function prototypes */
int res_init(void);
int res_query(const char *dname, int class, int type, unsigned char *answer, int anslen);
int res_search(const char *dname, int class, int type, unsigned char *answer, int anslen);
int res_mkquery(int op, const char *dname, int class, int type,
                const unsigned char *data, int datalen,
                const unsigned char *newrr_in,
                unsigned char *buf, int buflen);
int res_send(const unsigned char *msg, int msglen, unsigned char *answer, int anslen);
void res_close(void);

#endif /* !_RESOLV_H_ */
