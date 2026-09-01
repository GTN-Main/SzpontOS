/*
 * SzpontOS Libc - Standard DNS Name Server Protocol (<arpa/nameser.h>)
 * (C) Copyright by Szpont Industries. All rights reserved.
 * Inspired by FreeBSD and RFC 1035.
 */

#ifndef _ARPA_NAMESER_H_
#define _ARPA_NAMESER_H_

#include <stdint.h>

#define PACKETSZ        512     /* Maximum packet size */
#define HFIXEDSZ        12      /* #/bytes of fixed data in header */
#define QFIXEDSZ        4       /* #/bytes of fixed data in query */
#define RRFIXEDSZ       10      /* #/bytes of fixed data in rrec */
#define MAXDNAME        1025    /* Maximum domain name */
#define MAXCDNAME       255     /* Maximum compressed domain name */
#define MAXLABEL        63      /* Maximum length of domain label */

#define QUERY           0x0     /* Standard query */
#define IQUERY          0x1     /* Inverse query */
#define STATUS          0x2     /* Server status request */
#define NOTIFY          0x4     /* Zone change notification */
#define UPDATE          0x5     /* Dynamic update */

#define NOERROR         0       /* No error */
#define FORMERR         1       /* Format error */
#define SERVFAIL        2       /* Server failure */
#define NXDOMAIN        3       /* Non existent domain */
#define NOTIMP          4       /* Not implemented */
#define REFUSED         5       /* Query refused */

/* Type values for Resources and Queries */
#define T_A             1       /* Host address */
#define T_NS            2       /* Authoritative server */
#define T_MD            3       /* Mail destination */
#define T_MF            4       /* Mail forwarder */
#define T_CNAME         5       /* Canonical name */
#define T_SOA           6       /* Start of authority zone */
#define T_MB            7       /* Mailbox domain name */
#define T_MG            8       /* Mail group member */
#define T_MR            9       /* Mail rename name */
#define T_NULL          10      /* Null resource record */
#define T_WKS           11      /* Well known service */
#define T_PTR           12      /* Domain name pointer */
#define T_HINFO         13      /* Host information */
#define T_MINFO         14      /* Mailbox information */
#define T_MX            15      /* Mail routing information */
#define T_TXT           16      /* Text strings */
#define T_AAAA          28      /* IPv6 address */
#define T_SRV           33      /* Server selection */
#define T_ANY           255     /* Wildcard match */

/* Values for class field */
#define C_IN            1       /* The ARPA Internet */
#define C_CHAOS         3       /* Chaos net */
#define C_HS            4       /* Hesiod server */
#define C_ANY           255     /* Wildcard match */

/* Header structure for DNS protocol */
typedef struct {
    uint16_t id;                /* Query identification number */
    uint16_t flags;             /* DNS message flags */
    uint16_t qdcount;           /* Number of question entries */
    uint16_t ancount;           /* Number of answer entries */
    uint16_t nscount;           /* Number of authority entries */
    uint16_t arcount;           /* Number of resource entries */
} HEADER;

#endif /* !_ARPA_NAMESER_H_ */
