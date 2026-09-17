#ifndef SZPONTOS_NET_SOCKET_H
#define SZPONTOS_NET_SOCKET_H

#include <kernel/types.h>
#include <kernel/spinlock.h>
#include <net/net.h>
#include <fs/vfs.h>

/* Address Families */
#define AF_UNSPEC 0
#define AF_UNIX 1
#define AF_LOCAL 1
#define AF_INET 2
#define AF_INET6 10

/* Socket Types */
#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define SOCK_RAW 3

/* Protocol constants */
#define IPPROTO_IP 0
#define IPPROTO_ICMP 1
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17

/* Socket States */
typedef enum {
    SS_UNCONNECTED = 0,
    SS_BIND,
    SS_LISTENING,
    SS_CONNECTING,
    SS_CONNECTED,
    SS_DISCONNECTING,
    SS_CLOSED
} sock_state_t;

/* TCP States */
typedef enum {
    TCP_STATE_CLOSED = 0,
    TCP_STATE_LISTEN,
    TCP_STATE_SYN_SENT,
    TCP_STATE_SYN_RECEIVED,
    TCP_STATE_ESTABLISHED,
    TCP_STATE_FIN_WAIT_1,
    TCP_STATE_FIN_WAIT_2,
    TCP_STATE_CLOSE_WAIT,
    TCP_STATE_CLOSING,
    TCP_STATE_LAST_ACK,
    TCP_STATE_TIME_WAIT
} tcp_state_t;

/* Standard POSIX sockaddr structures for Kernel */
struct sockaddr {
    uint16_t sa_family;
    char sa_data[14];
};

struct in_addr {
    uint32_t s_addr;
};

struct sockaddr_in {
    uint16_t sin_family;
    uint16_t sin_port;
    struct in_addr sin_addr;
    char sin_zero[8];
};

struct sockaddr_un {
    uint16_t sun_family;
    char sun_path[108];
};

/* Socket Options (SOL_SOCKET = 1) */
#define SOL_SOCKET 1
#define SO_DEBUG 1
#define SO_REUSEADDR 2
#define SO_TYPE 3
#define SO_ERROR 4
#define SO_DONTROUTE 5
#define SO_BROADCAST 6
#define SO_SNDBUF 7
#define SO_RCVBUF 8
#define SO_KEEPALIVE 9
#define SO_OOBINLINE 10
#define SO_NO_CHECK 11
#define SO_PRIORITY 12
#define SO_LINGER 13
#define SO_BSDCOMPAT 14
#define SO_REUSEPORT 15
#define SO_PASSCRED 16
#define SO_PEERCRED 17
#define SO_RCVLOWAT 18
#define SO_SNDLOWAT 19
#define SO_RCVTIMEO 20
#define SO_SNDTIMEO 21
#define SO_BINDTODEVICE 25
#define SO_ATTACH_FILTER 26
#define SO_DETACH_FILTER 27
#define SO_ACCEPTCONN 30
#define SO_PEERSEC 31
#define SO_SNDBUFFORCE 32
#define SO_RCVBUFFORCE 33
#define SO_PASSSEC 34

/* TCP Options (IPPROTO_TCP = 6) */
#define TCP_NODELAY 1
#define TCP_MAXSEG 2
#define TCP_CORK 3
#define TCP_KEEPIDLE 4
#define TCP_KEEPINTVL 5
#define TCP_KEEPCNT 6
#define TCP_SYNCNT 7
#define TCP_LINGER2 8
#define TCP_DEFER_ACCEPT 9
#define TCP_WINDOW_CLAMP 10
#define TCP_INFO 11
#define TCP_QUICKACK 12
#define TCP_CONGESTION 13

/* IP Options (IPPROTO_IP = 0) */
#define IP_TOS 1
#define IP_TTL 2
#define IP_HDRINCL 3
#define IP_OPTIONS 4
#define IP_ROUTER_ALERT 5
#define IP_RECVOPTS 6
#define IP_RETOPTS 7
#define IP_PKTINFO 8
#define IP_PKTOPTIONS 9
#define IP_MTU_DISCOVER 10
#define IP_RECVERR 11
#define IP_RECVTTL 12
#define IP_RECVTOS 13
#define IP_MTU 14
#define IP_MULTICAST_IF 32
#define IP_MULTICAST_TTL 33
#define IP_MULTICAST_LOOP 34
#define IP_ADD_MEMBERSHIP 35
#define IP_DROP_MEMBERSHIP 36

/* IPv6 Options (IPPROTO_IPV6 = 41) */
#define IPPROTO_IPV6 41
#define IPV6_V6ONLY 26

/* Shutdown options */
#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2

/* Socket timeout / linger / peercred structs for kernel */
struct linger_k {
    int l_onoff;
    int l_linger;
};

struct ucred_k {
    pid_t pid;
    uid_t uid;
    gid_t gid;
};

/* SCM_RIGHTS */
#define SCM_RIGHTS 1
#define UNIX_MAX_PASSED_FDS 8

#define CMSG_ALIGN(len) (((len) + sizeof(size_t) - 1) & ~(sizeof(size_t) - 1))
#define CMSG_DATA(cmsg) ((unsigned char *)((struct cmsghdr *)(cmsg) + 1))
#define CMSG_SPACE(len) (CMSG_ALIGN(sizeof(struct cmsghdr)) + CMSG_ALIGN(len))
#define CMSG_LEN(len)   (CMSG_ALIGN(sizeof(struct cmsghdr)) + (len))

struct iovec {
    void *iov_base;
    size_t iov_len;
};

struct msghdr {
    void *msg_name;
    uint32_t msg_namelen;
    struct iovec *msg_iov;
    size_t msg_iovlen;
    void *msg_control;
    size_t msg_controllen;
    int msg_flags;
};

struct cmsghdr {
    size_t cmsg_len;
    int cmsg_level;
    int cmsg_type;
};

#define SOCK_RX_BUF_SIZE 65536
#define SOCK_TX_BUF_SIZE 65536
#define SOCK_DGRAM_QUEUE_LEN 32

typedef struct {
    size_t len;
    uint32_t from_ip;
    uint16_t from_port;
} dgram_meta_t;

typedef struct socket {
    int domain;
    int type;
    int protocol;
    sock_state_t state;
    tcp_state_t tcp_state;

    /* Addressing */
    uint32_t local_ip;
    uint16_t local_port;
    uint32_t remote_ip;
    uint16_t remote_port;

    /* Unix Domain socket path */
    char unix_path[108];
    struct socket *peer;

    /* TCP sequence and acknowledgment */
    uint32_t snd_una;
    uint32_t snd_nxt;
    uint32_t snd_wnd;
    uint32_t rcv_nxt;
    uint32_t rcv_wnd;

    /* Buffers */
    uint8_t rx_buf[SOCK_RX_BUF_SIZE];
    size_t rx_head;
    size_t rx_tail;
    size_t rx_len;

    /* Datagram queue metadata for SOCK_DGRAM / SOCK_RAW */
    dgram_meta_t dgram_queue[SOCK_DGRAM_QUEUE_LEN];
    size_t dgram_head;
    size_t dgram_tail;
    size_t dgram_count;

    /* Listen backlog */
    int backlog;
    struct socket *accept_queue[16];
    size_t accept_count;

    /* SCM_RIGHTS file descriptor passing for AF_UNIX */
    file_descriptor_t *passed_fds[8];
    size_t passed_fd_count;

    /* Socket configuration options */
    int so_reuseaddr;
    int so_reuseport;
    int so_broadcast;
    int so_keepalive;
    int so_passcred;
    int so_rcvbuf;
    int so_sndbuf;
    int so_error;
    int so_dontroute;
    int so_oobinline;
    int so_priority;
    int64_t so_rcvtimeo_ms;
    int64_t so_sndtimeo_ms;
    struct linger_k so_linger;
    char bind_device[16];

    /* TCP options */
    int tcp_nodelay;
    int tcp_cork;
    int tcp_mss;
    int tcp_keepidle;
    int tcp_keepintvl;
    int tcp_keepcnt;
    int tcp_quickack;

    /* IP options */
    uint8_t ip_ttl;
    uint8_t ip_tos;
    int ip_multicast_loop;
    uint8_t ip_multicast_ttl;
    int ip_pktinfo;
    int ipv6_v6only;

    /* Shutdown state (bit 0 = SHUT_RD, bit 1 = SHUT_WR) */
    int shutdown_flags;

    spinlock_t lock;
    vfs_node_t *vfs_node;
    struct socket *next;
} socket_t;

void socket_subsystem_init(void);
socket_t *socket_create(int domain, int type, int protocol);
void socket_destroy(socket_t *sock);
socket_t *socket_find_udp(uint32_t local_ip, uint16_t local_port);
socket_t *socket_find_tcp(uint32_t local_ip, uint16_t local_port, uint32_t remote_ip, uint16_t remote_port);
socket_t *socket_find_icmp(uint32_t local_ip);
int socket_enqueue_data(socket_t *sock, const void *data, size_t len, uint32_t from_ip, uint16_t from_port);

/* Syscall implementations */
int sys_socket(int domain, int type, int protocol);
int sys_bind(int fd, const struct sockaddr *addr, uint32_t addrlen);
int sys_connect(int fd, const struct sockaddr *addr, uint32_t addrlen);
int sys_listen(int fd, int backlog);
int sys_accept(int fd, struct sockaddr *addr, uint32_t *addrlen);
ssize_t sys_sendto(int fd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, uint32_t addrlen);
ssize_t sys_recvfrom(int fd, void *buf, size_t len, int flags, struct sockaddr *src_addr, uint32_t *addrlen);
ssize_t sys_sendmsg(int fd, const struct msghdr *msg, int flags);
ssize_t sys_recvmsg(int fd, struct msghdr *msg, int flags);
int sys_shutdown(int fd, int how);
int sys_getsockname(int fd, struct sockaddr *addr, uint32_t *addrlen);
int sys_getpeername(int fd, struct sockaddr *addr, uint32_t *addrlen);
int sys_setsockopt(int fd, int level, int optname, const void *optval, uint32_t optlen);
int sys_getsockopt(int fd, int level, int optname, void *optval, uint32_t *optlen);
int sys_socketpair(int domain, int type, int protocol, int sv[2]);

#endif /* SZPONTOS_NET_SOCKET_H */
