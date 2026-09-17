/*
 * SzpontOS - Berkeley Sockets Implementation & System Calls
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <net/socket.h>
#include <net/net.h>
#include <fs/vfs.h>
#include <sched/process.h>
#include <sched/sched.h>
#include <mm/heap.h>
#include <kernel/string.h>
#include <kernel/kprint.h>
#include <kernel/spinlock.h>
#include <drivers/drm.h>
#include <mm/usercopy.h>

extern int tcp_send_segment(uint32_t src_ip, uint16_t src_port, uint32_t dest_ip, uint16_t dest_port, uint32_t seq,
                            uint32_t ack, uint8_t flags, const void *data, size_t len);

static socket_t *g_socket_list = NULL;
static spinlock_t g_socket_list_lock = SPINLOCK_INIT;
static uint16_t g_ephemeral_port = 49152;

static vfs_ops_t g_socket_vfs_ops;

static ssize_t socket_vfs_read(vfs_node_t *node, off_t offset, size_t size, void *buffer) {
    (void)offset;
    if (!node || !node->device_data || !buffer || size == 0)
        return 0;
    socket_t *sock = (socket_t *)node->device_data;

    if (sock->shutdown_flags & 1) {
        return 0; /* EOF: Read shut down */
    }

    netif_poll_all();

    process_t *proc = sched_get_current_process();
    bool is_nonblock = false;
    if (proc) {
        for (int i = 0; i < MAX_FD; i++) {
            if (proc->fds[i] && proc->fds[i]->node == node) {
                if (proc->fds[i]->flags & 0x800) {
                    is_nonblock = true;
                }
                break;
            }
        }
    }
    if (sock->domain == AF_UNIX) {
        if (is_nonblock && sock->rx_len == 0) {
            if (sock->state == SS_CLOSED || !sock->peer || sock->peer->state == SS_CLOSED) {
                return 0; /* EOF */
            }
            return -11; /* -EAGAIN */
        }
        while (sock->rx_len == 0) {
            if (sock->state == SS_CLOSED || !sock->peer || sock->peer->state == SS_CLOSED) {
                return 0; /* EOF */
            }
            thread_sleep(1);
        }
    } else if (sock->type == SOCK_DGRAM || sock->type == SOCK_RAW) {
        if (is_nonblock && sock->dgram_count == 0) {
            if (sock->state == SS_CLOSED) {
                return 0; /* EOF */
            }
            return -11; /* -EAGAIN */
        }
        while (sock->dgram_count == 0) {
            if (sock->state == SS_CLOSED) {
                return 0; /* EOF */
            }
            netif_poll_all();
            thread_sleep(1);
        }
    } else {
        if (is_nonblock && sock->rx_len == 0) {
            if (sock->state == SS_CLOSED || (sock->type == SOCK_STREAM && (sock->tcp_state == TCP_STATE_CLOSE_WAIT || sock->tcp_state == TCP_STATE_CLOSED))) {
                return 0; /* EOF */
            }
            return -11; /* -EAGAIN */
        }
        while (sock->rx_len == 0) {
            if (sock->state == SS_CLOSED || (sock->type == SOCK_STREAM && (sock->tcp_state == TCP_STATE_CLOSE_WAIT || sock->tcp_state == TCP_STATE_CLOSED))) {
                return 0; /* EOF */
            }
            netif_poll_all();
            thread_sleep(1);
        }
    }

    spinlock_acquire(&sock->lock);
    if (sock->type == SOCK_DGRAM || sock->type == SOCK_RAW) {
        if (sock->dgram_count == 0) {
            spinlock_release(&sock->lock);
            return is_nonblock ? -11 : 0;
        }

        dgram_meta_t meta = sock->dgram_queue[sock->dgram_head];
        sock->dgram_head = (sock->dgram_head + 1) % SOCK_DGRAM_QUEUE_LEN;
        sock->dgram_count--;

        size_t pkt_len = meta.len;
        size_t to_read = size < pkt_len ? size : pkt_len;
        uint8_t *dst = (uint8_t *)buffer;

        for (size_t i = 0; i < to_read; i++) {
            dst[i] = sock->rx_buf[sock->rx_head];
            sock->rx_head = (sock->rx_head + 1) % SOCK_RX_BUF_SIZE;
        }
        if (pkt_len > to_read) {
            sock->rx_head = (sock->rx_head + (pkt_len - to_read)) % SOCK_RX_BUF_SIZE;
        }
        if (sock->rx_len >= pkt_len)
            sock->rx_len -= pkt_len;
        else
            sock->rx_len = 0;

        spinlock_release(&sock->lock);
        return (ssize_t)to_read;
    }

    size_t to_read = size < sock->rx_len ? size : sock->rx_len;
    uint8_t *dst = (uint8_t *)buffer;
    for (size_t i = 0; i < to_read; i++) {
        dst[i] = sock->rx_buf[sock->rx_head];
        sock->rx_head = (sock->rx_head + 1) % SOCK_RX_BUF_SIZE;
    }
    sock->rx_len -= to_read;
    spinlock_release(&sock->lock);
    return (ssize_t)to_read;
}

static ssize_t socket_vfs_write(vfs_node_t *node, off_t offset, size_t size, const void *buffer) {
    (void)offset;
    if (!node || !node->device_data || !buffer || size == 0)
        return 0;
    socket_t *sock = (socket_t *)node->device_data;

    if (sock->shutdown_flags & 2) {
        return -32; /* -EPIPE: Write shut down */
    }

    if (sock->domain == AF_UNIX) {
        if (!sock->peer || sock->peer->state == SS_CLOSED)
            return -1;
        socket_t *peer = sock->peer;

        process_t *proc = sched_get_current_process();
        bool is_nonblock = false;
        if (proc) {
            for (int i = 0; i < MAX_FD; i++) {
                if (proc->fds[i] && proc->fds[i]->node == node) {
                    if (proc->fds[i]->flags & 0x800) {
                        is_nonblock = true;
                    }
                    break;
                }
            }
        }

        if (is_nonblock && peer->rx_len >= SOCK_RX_BUF_SIZE) {
            return -11; /* -EAGAIN */
        }

        while (peer->rx_len >= SOCK_RX_BUF_SIZE) {
            if (peer->state == SS_CLOSED)
                return -1;
            thread_sleep(1);
        }

        spinlock_acquire(&peer->lock);
        size_t written = 0;
        const uint8_t *src = (const uint8_t *)buffer;
        while (written < size && peer->rx_len < SOCK_RX_BUF_SIZE) {
            peer->rx_buf[peer->rx_tail] = src[written++];
            peer->rx_tail = (peer->rx_tail + 1) % SOCK_RX_BUF_SIZE;
            peer->rx_len++;
        }
        spinlock_release(&peer->lock);
        if (written == 0 && is_nonblock) {
            return -11; /* -EAGAIN */
        }
        return (ssize_t)written;
    }

    if (sock->type == SOCK_STREAM) {
        if (sock->state != SS_CONNECTED && sock->tcp_state != TCP_STATE_ESTABLISHED &&
            sock->tcp_state != TCP_STATE_SYN_RECEIVED) {
            return -107; /* -ENOTCONN */
        }
        if (sock->local_ip == 0) {
            netif_t *def = ((sock->remote_ip & 0xFF) == 127) ? netif_get_loopback() : netif_get_default();
            if (def)
                sock->local_ip = def->ip;
        }
        tcp_send_segment(sock->local_ip, sock->local_port, sock->remote_ip, sock->remote_port, sock->snd_nxt,
                         sock->rcv_nxt, TCP_FLAG_ACK | TCP_FLAG_PSH, buffer, size);
        sock->snd_nxt += (uint32_t)size;
        return (ssize_t)size;
    } else if (sock->type == SOCK_DGRAM) {
        net_buf_t *buf = net_buf_alloc();
        if (!buf)
            return -1;
        memcpy(buf->data, buffer, size);
        buf->len = size;
        buf->offset = 0;
        udp_output(sock->local_ip, sock->local_port, sock->remote_ip, sock->remote_port, buf);
        return (ssize_t)size;
    }

    return -1;
}

static int socket_vfs_close(vfs_node_t *node) {
    if (!node)
        return 0;
    socket_t *sock = (socket_t *)node->device_data;
    if (sock) {
        if (sock->domain == AF_INET && sock->type == SOCK_STREAM &&
            (sock->tcp_state == TCP_STATE_ESTABLISHED || sock->tcp_state == TCP_STATE_SYN_RECEIVED)) {
            tcp_send_segment(sock->local_ip, sock->local_port, sock->remote_ip, sock->remote_port,
                             sock->snd_nxt, sock->rcv_nxt, TCP_FLAG_FIN | TCP_FLAG_ACK, NULL, 0);
            sock->snd_nxt++;
            sock->tcp_state = TCP_STATE_FIN_WAIT_1;
            sock->state = SS_CLOSED;
        }
        socket_destroy(sock);
    }
    kfree(node);
    return 0;
}

void socket_subsystem_init(void) {
    memset(&g_socket_vfs_ops, 0, sizeof(vfs_ops_t));
    g_socket_vfs_ops.read = socket_vfs_read;
    g_socket_vfs_ops.write = socket_vfs_write;
    g_socket_vfs_ops.close = socket_vfs_close;
}

static socket_t *socket_create_unlocked(int domain, int type, int protocol) {
    socket_t *sock = (socket_t *)kmalloc(sizeof(socket_t));
    if (!sock)
        return NULL;
    memset(sock, 0, sizeof(socket_t));

    sock->domain = domain;
    sock->type = type;
    sock->protocol = protocol;
    sock->state = SS_UNCONNECTED;
    sock->tcp_state = TCP_STATE_CLOSED;
    sock->lock = SPINLOCK_INIT;

    /* Initialize default socket options */
    sock->so_rcvbuf = SOCK_RX_BUF_SIZE;
    sock->so_sndbuf = SOCK_TX_BUF_SIZE;
    sock->ip_ttl = 64;
    sock->ip_multicast_ttl = 1;
    sock->ip_multicast_loop = 1;
    sock->tcp_keepidle = 7200;
    sock->tcp_keepintvl = 75;
    sock->tcp_keepcnt = 9;
    sock->tcp_mss = 1460;

    sock->next = g_socket_list;
    g_socket_list = sock;

    return sock;
}

socket_t *socket_create(int domain, int type, int protocol) {
    spinlock_acquire(&g_socket_list_lock);
    socket_t *sock = socket_create_unlocked(domain, type, protocol);
    spinlock_release(&g_socket_list_lock);
    return sock;
}

void socket_destroy(socket_t *sock) {
    if (!sock)
        return;

    if (sock->type == SOCK_STREAM && sock->tcp_state == TCP_STATE_ESTABLISHED) {
        /* Send FIN */
        tcp_send_segment(sock->local_ip, sock->local_port, sock->remote_ip, sock->remote_port, sock->snd_nxt,
                         sock->rcv_nxt, TCP_FLAG_FIN | TCP_FLAG_ACK, NULL, 0);
        sock->snd_nxt++;
        sock->tcp_state = TCP_STATE_FIN_WAIT_1;
    }

    if (sock->domain == AF_UNIX && sock->peer) {
        sock->peer->state = SS_CLOSED;
        sock->peer->peer = NULL;
        sock->peer = NULL;
    }

    spinlock_acquire(&g_socket_list_lock);
    if (g_socket_list == sock) {
        g_socket_list = sock->next;
    } else {
        for (socket_t *cur = g_socket_list; cur != NULL; cur = cur->next) {
            if (cur->next == sock) {
                cur->next = sock->next;
                break;
            }
        }
    }
    spinlock_release(&g_socket_list_lock);

    for (size_t i = 0; i < sock->passed_fd_count; i++) {
        if (sock->passed_fds[i]) {
            fd_release(sock->passed_fds[i]);
            sock->passed_fds[i] = NULL;
        }
    }

    kfree(sock);
}

socket_t *socket_find_udp(uint32_t local_ip, uint16_t local_port) {
    spinlock_acquire(&g_socket_list_lock);
    for (socket_t *cur = g_socket_list; cur != NULL; cur = cur->next) {
        if (cur->type == SOCK_DGRAM && cur->local_port == local_port) {
            if (cur->local_ip == 0 || cur->local_ip == local_ip) {
                spinlock_release(&g_socket_list_lock);
                return cur;
            }
        }
    }
    spinlock_release(&g_socket_list_lock);
    return NULL;
}

socket_t *socket_find_tcp(uint32_t local_ip, uint16_t local_port, uint32_t remote_ip, uint16_t remote_port) {
    spinlock_acquire(&g_socket_list_lock);
    for (socket_t *cur = g_socket_list; cur != NULL; cur = cur->next) {
        if (cur->type == SOCK_STREAM && cur->local_port == local_port &&
            (cur->local_ip == 0 || cur->local_ip == local_ip) && cur->remote_port == remote_port &&
            cur->remote_ip == remote_ip) {
            spinlock_release(&g_socket_list_lock);
            return cur;
        }
    }
    spinlock_release(&g_socket_list_lock);
    return NULL;
}

socket_t *socket_find_tcp_listener(uint32_t local_ip, uint16_t local_port) {
    spinlock_acquire(&g_socket_list_lock);
    for (socket_t *cur = g_socket_list; cur != NULL; cur = cur->next) {
        if (cur->type == SOCK_STREAM && cur->tcp_state == TCP_STATE_LISTEN && cur->local_port == local_port) {
            if (cur->local_ip == 0 || cur->local_ip == local_ip) {
                spinlock_release(&g_socket_list_lock);
                return cur;
            }
        }
    }
    spinlock_release(&g_socket_list_lock);
    return NULL;
}

socket_t *socket_create_child(socket_t *listener, uint32_t remote_ip, uint16_t remote_port) {
    if (!listener)
        return NULL;
    socket_t *child = socket_create(listener->domain, listener->type, listener->protocol);
    if (!child)
        return NULL;

    child->local_ip = listener->local_ip;
    if (child->local_ip == 0) {
        netif_t *def = ((remote_ip & 0xFF) == 127) ? netif_get_loopback() : netif_get_default();
        if (def)
            child->local_ip = def->ip;
    }
    child->local_port = listener->local_port;
    child->remote_ip = remote_ip;
    child->remote_port = remote_port;

    spinlock_acquire(&listener->lock);
    if (listener->accept_count < 16) {
        listener->accept_queue[listener->accept_count++] = child;
    }
    spinlock_release(&listener->lock);

    return child;
}

socket_t *socket_find_icmp(uint32_t local_ip) {
    (void)local_ip;
    spinlock_acquire(&g_socket_list_lock);
    for (socket_t *cur = g_socket_list; cur != NULL; cur = cur->next) {
        if (cur->domain == AF_INET && (cur->type == SOCK_RAW || cur->protocol == IP_PROTO_ICMP || cur->protocol == 1)) {
            spinlock_release(&g_socket_list_lock);
            return cur;
        }
    }
    spinlock_release(&g_socket_list_lock);
    return NULL;
}

int socket_enqueue_data(socket_t *sock, const void *data, size_t len, uint32_t from_ip, uint16_t from_port) {
    if (!sock || !data || len == 0)
        return 0;

    spinlock_acquire(&sock->lock);
    if (from_ip != 0) {
        sock->remote_ip = from_ip;
        sock->remote_port = from_port;
    }

    if (sock->type == SOCK_DGRAM || sock->type == SOCK_RAW) {
        if (sock->dgram_count >= SOCK_DGRAM_QUEUE_LEN || (sock->rx_len + len) > SOCK_RX_BUF_SIZE) {
            spinlock_release(&sock->lock);
            return 0; /* Drop datagram if queue or buffer full */
        }
        sock->dgram_queue[sock->dgram_tail].len = len;
        sock->dgram_queue[sock->dgram_tail].from_ip = from_ip;
        sock->dgram_queue[sock->dgram_tail].from_port = from_port;
        sock->dgram_tail = (sock->dgram_tail + 1) % SOCK_DGRAM_QUEUE_LEN;
        sock->dgram_count++;
    }

    const uint8_t *src = (const uint8_t *)data;
    size_t written = 0;

    while (written < len && sock->rx_len < SOCK_RX_BUF_SIZE) {
        sock->rx_buf[sock->rx_tail] = src[written++];
        sock->rx_tail = (sock->rx_tail + 1) % SOCK_RX_BUF_SIZE;
        sock->rx_len++;
    }
    spinlock_release(&sock->lock);

    return (int)written;
}

static socket_t *get_socket_from_fd(int fd) {
    process_t *proc = sched_get_current_process();
    if (!proc || fd < 0 || fd >= MAX_FD)
        return NULL;
    file_descriptor_t *fdesc = proc->fds[fd];
    if (!fdesc || !fdesc->node || fdesc->node->flags != VFS_TYPE_SOCKET)
        return NULL;
    return (socket_t *)fdesc->node->device_data;
}

/* Syscall Wrappers */
int sys_socket(int domain, int type, int protocol) {
    process_t *proc = sched_get_current_process();
    if (!proc)
        return -1;

    /* Find free fd */
    int fd = -1;
    for (int i = 3; i < MAX_FD; i++) {
        if (!proc->fds[i]) {
            fd = i;
            break;
        }
    }
    int base_type = type & 0xFF;
    bool is_nonblock = (type & (0x800 | 0x4000)) != 0;
    bool is_cloexec = (type & (0x80000 | 0x200000)) != 0;

    socket_t *sock = socket_create(domain, base_type, protocol);
    if (!sock)
        return -1;

    vfs_node_t *node = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
    if (!node) {
        socket_destroy(sock);
        return -1;
    }
    memset(node, 0, sizeof(vfs_node_t));
    ksnprintf(node->name, sizeof(node->name), "socket:[%d]", fd);
    node->flags = VFS_TYPE_SOCKET;
    node->ops = &g_socket_vfs_ops;
    node->device_data = sock;
    sock->vfs_node = node;

    file_descriptor_t *fdesc = (file_descriptor_t *)kmalloc(sizeof(file_descriptor_t));
    if (!fdesc) {
        kfree(node);
        socket_destroy(sock);
        return -1;
    }
    fdesc->node = node;
    fdesc->flags = O_RDWR | (is_nonblock ? 0x800 : 0);
    fdesc->offset = 0;
    fdesc->refcount = 1;

    proc->fds[fd] = fdesc;
    proc->fd_cloexec[fd] = is_cloexec;
    return fd;
}

int sys_bind(int fd, const struct sockaddr *addr, uint32_t addrlen) {
    socket_t *sock = get_socket_from_fd(fd);
    if (!sock || !addr || addrlen < sizeof(struct sockaddr))
        return -1;

    if (sock->domain == AF_INET) {
        const struct sockaddr_in *in = (const struct sockaddr_in *)addr;
        sock->local_ip = in->sin_addr.s_addr;
        sock->local_port = ntohs(in->sin_port);
        sock->state = SS_BIND;
        return 0;
    } else if (sock->domain == AF_UNIX) {
        const struct sockaddr_un *un = (const struct sockaddr_un *)addr;
        strncpy(sock->unix_path, un->sun_path, sizeof(sock->unix_path) - 1);
        sock->state = SS_BIND;
        return 0;
    }

    return -1;
}

int sys_listen(int fd, int backlog) {
    socket_t *sock = get_socket_from_fd(fd);
    if (!sock || sock->type != SOCK_STREAM)
        return -1;

    sock->backlog = backlog > 0 ? backlog : 5;
    sock->state = SS_LISTENING;
    sock->tcp_state = TCP_STATE_LISTEN;
    return 0;
}

int sys_accept(int fd, struct sockaddr *addr, uint32_t *addrlen) {
    socket_t *sock = get_socket_from_fd(fd);
    if (!sock || sock->state != SS_LISTENING)
        return -1;

    process_t *proc = sched_get_current_process();
    if (!proc)
        return -1;

    bool is_nonblock = false;
    if (fd >= 0 && fd < MAX_FD && proc->fds[fd]) {
        if (proc->fds[fd]->flags & 0x0800 /* O_NONBLOCK */) {
            is_nonblock = true;
        }
    }

    socket_t *child = NULL;

    /* Wait for child socket in accept_queue */
    while (1) {
        if (sock->state != SS_LISTENING)
            return -1;

        spinlock_acquire(&sock->lock);
        if (sock->accept_count > 0) {
            child = sock->accept_queue[0];
            for (size_t i = 1; i < sock->accept_count; i++) {
                sock->accept_queue[i - 1] = sock->accept_queue[i];
            }
            sock->accept_count--;
            spinlock_release(&sock->lock);
            break;
        }
        spinlock_release(&sock->lock);

        if (is_nonblock) {
            return -11; /* -EAGAIN / -EWOULDBLOCK */
        }

        if (sock->domain != AF_UNIX)
            netif_poll_all();
        thread_sleep(5);
    }

    if (!child) {
        return -1;
    }

    /* Allocate new FD for child socket */
    int new_fd = -1;
    for (int i = 3; i < MAX_FD; i++) {
        if (!proc->fds[i]) {
            new_fd = i;
            break;
        }
    }
    if (new_fd == -1) {
        socket_destroy(child);
        return -1;
    }

    vfs_node_t *node = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
    if (!node) {
        socket_destroy(child);
        return -1;
    }
    memset(node, 0, sizeof(vfs_node_t));
    ksnprintf(node->name, sizeof(node->name), "socket:[%d]", new_fd);
    node->flags = VFS_TYPE_SOCKET;
    node->ops = &g_socket_vfs_ops;
    node->device_data = child;
    child->vfs_node = node;

    file_descriptor_t *fdesc = (file_descriptor_t *)kmalloc(sizeof(file_descriptor_t));
    fdesc->node = node;
    fdesc->flags = O_RDWR;
    fdesc->offset = 0;
    fdesc->refcount = 1;

    proc->fds[new_fd] = fdesc;

    if (addr && addrlen) {
        if (child->domain == AF_UNIX && *addrlen >= sizeof(struct sockaddr_un)) {
            struct sockaddr_un *un = (struct sockaddr_un *)addr;
            un->sun_family = AF_UNIX;
            strncpy(un->sun_path, child->unix_path, sizeof(un->sun_path) - 1);
            *addrlen = sizeof(struct sockaddr_un);
        } else if (child->domain == AF_INET && *addrlen >= sizeof(struct sockaddr_in)) {
            struct sockaddr_in *in = (struct sockaddr_in *)addr;
            in->sin_family = AF_INET;
            in->sin_addr.s_addr = child->remote_ip;
            in->sin_port = htons(child->remote_port);
            *addrlen = sizeof(struct sockaddr_in);
        }
    }

    return new_fd;
}

int sys_connect(int fd, const struct sockaddr *addr, uint32_t addrlen) {
    socket_t *sock = get_socket_from_fd(fd);
    if (!sock || !addr || addrlen < sizeof(struct sockaddr))
        return -1;

    if (sock->domain == AF_INET) {
        const struct sockaddr_in *in = (const struct sockaddr_in *)addr;
        sock->remote_ip = in->sin_addr.s_addr;
        sock->remote_port = ntohs(in->sin_port);

        if (sock->local_port == 0) {
            sock->local_port = g_ephemeral_port++;
            if (g_ephemeral_port > 65000)
                g_ephemeral_port = 49152;
        }

        if (sock->local_ip == 0) {
            netif_t *def = ((sock->remote_ip & 0xFF) == 127) ? netif_get_loopback() : netif_get_default();
            if (def)
                sock->local_ip = def->ip;
        }

        if (sock->type == SOCK_STREAM) {
            sock->snd_nxt = 500;
            sock->tcp_state = TCP_STATE_SYN_SENT;
            sock->state = SS_CONNECTING;

            /* Send SYN */
            tcp_send_segment(sock->local_ip, sock->local_port, sock->remote_ip, sock->remote_port, sock->snd_nxt, 0,
                             TCP_FLAG_SYN, NULL, 0);
            sock->snd_nxt++;

            if (sock->tcp_state == TCP_STATE_ESTABLISHED) {
                sock->state = SS_CONNECTED;
                return 0;
            }

            /* Check if non-blocking */
            file_descriptor_t *fdesc = NULL;
            process_t *proc = sched_get_current_process();
            if (proc && fd >= 0 && fd < MAX_FD) {
                fdesc = proc->fds[fd];
            }
            if (fdesc && (fdesc->flags & 0x0800 /* O_NONBLOCK */)) {
                return -115; /* -EINPROGRESS */
            }

            /* Wait for ESTABLISHED or timeout */
            for (int t = 0; t < 300; t++) {
                netif_poll_all();
                if (sock->tcp_state == TCP_STATE_ESTABLISHED) {
                    sock->state = SS_CONNECTED;
                    return 0;
                }
                if (sock->tcp_state == TCP_STATE_CLOSED) {
                    return -1;
                }
                thread_sleep(10);
            }
            return (sock->tcp_state == TCP_STATE_ESTABLISHED) ? 0 : -1;
        }
        sock->state = SS_CONNECTED;
        return 0;
    } else if (sock->domain == AF_UNIX) {
        const struct sockaddr_un *un = (const struct sockaddr_un *)addr;
        klog_info("NET: sys_connect AF_UNIX target path '%s'", un->sun_path);
        socket_t *listener = NULL;
        spinlock_acquire(&g_socket_list_lock);
        for (socket_t *cur = g_socket_list; cur != NULL; cur = cur->next) {
            if (cur->domain == AF_UNIX && cur->state == SS_LISTENING && strcmp(cur->unix_path, un->sun_path) == 0) {
                listener = cur;
                break;
            }
        }

        if (listener) {
            socket_t *child = socket_create_unlocked(AF_UNIX, sock->type, sock->protocol);
            if (child) {
                child->peer = sock;
                sock->peer = child;
                child->state = SS_CONNECTED;
                sock->state = SS_CONNECTED;
                child->unix_path[0] = '\0'; /* Connected client socket does not listen */

                spinlock_acquire(&listener->lock);
                if (listener->accept_count < 16) {
                    listener->accept_queue[listener->accept_count++] = child;
                }
                spinlock_release(&listener->lock);

                spinlock_release(&g_socket_list_lock);
                klog_info("NET: AF_UNIX connect successful! Enqueued child into listening socket");
                return 0;
            }
        }
        spinlock_release(&g_socket_list_lock);
        klog_info("NET: AF_UNIX connect target '%s' not found or not listening in g_socket_list!", un->sun_path);
        return -1;
    }

    return -1;
}

ssize_t sys_sendto(int fd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, uint32_t addrlen) {
    (void)flags;
    socket_t *sock = get_socket_from_fd(fd);
    if (!sock)
        return -9; /* -EBADF */
    if (len == 0)
        return 0;
    if (!buf)
        return -14; /* -EFAULT */

    if (sock->domain == AF_UNIX) {
        if (!sock->peer)
            return -1;
        socket_t *peer = sock->peer;

        process_t *proc = sched_get_current_process();
        bool is_nonblock = (flags & 0x40);
        if (proc && fd >= 0 && fd < MAX_FD && proc->fds[fd]) {
            if (proc->fds[fd]->flags & 0x800) {
                is_nonblock = true;
            }
        }

        if (is_nonblock && peer->rx_len >= SOCK_RX_BUF_SIZE) {
            return -11; /* -EAGAIN */
        }

        while (peer->rx_len >= SOCK_RX_BUF_SIZE) {
            if (peer->state == SS_CLOSED)
                return -1;
            thread_sleep(1);
        }

        spinlock_acquire(&peer->lock);
        size_t written = 0;
        const uint8_t *src = (const uint8_t *)buf;
        while (written < len && peer->rx_len < SOCK_RX_BUF_SIZE) {
            peer->rx_buf[peer->rx_tail] = src[written++];
            peer->rx_tail = (peer->rx_tail + 1) % SOCK_RX_BUF_SIZE;
            peer->rx_len++;
        }
        spinlock_release(&peer->lock);
        if (written == 0 && is_nonblock) {
            return -11; /* -EAGAIN */
        }
        return (ssize_t)written;
    }

    uint32_t dest_ip = sock->remote_ip;
    uint16_t dest_port = sock->remote_port;

    if (dest_addr && addrlen >= sizeof(struct sockaddr_in)) {
        const struct sockaddr_in *in = (const struct sockaddr_in *)dest_addr;
        dest_ip = in->sin_addr.s_addr;
        dest_port = ntohs(in->sin_port);
    }

    if (sock->local_port == 0) {
        sock->local_port = g_ephemeral_port++;
        if (g_ephemeral_port > 65000)
            g_ephemeral_port = 49152;
    }
    if (sock->local_ip == 0) {
        netif_t *def = ((dest_ip & 0xFF) == 127) ? netif_get_loopback() : netif_get_default();
        if (def)
            sock->local_ip = def->ip;
    }

    if (sock->type == SOCK_RAW || sock->protocol == IP_PROTO_ICMP || sock->protocol == 1) {
        net_buf_t *pbuf = net_buf_alloc();
        if (!pbuf)
            return -1;
        memcpy(pbuf->data, buf, len);
        pbuf->len = len;
        pbuf->offset = 0;

        icmp_hdr_t *icmp = (icmp_hdr_t *)pbuf->data;
        if (icmp->checksum == 0) {
            icmp->checksum = ipv4_checksum(icmp, len);
        }

        ipv4_output(NULL, dest_ip, IP_PROTO_ICMP, pbuf);
        return (ssize_t)len;
    } else if (sock->type == SOCK_STREAM) {
        size_t sent_bytes = 0;
        const uint8_t *src = (const uint8_t *)buf;
        if (len == 0) {
            tcp_send_segment(sock->local_ip, sock->local_port, dest_ip, dest_port, sock->snd_nxt, sock->rcv_nxt,
                             TCP_FLAG_ACK, NULL, 0);
            return 0;
        }
        while (sent_bytes < len) {
            size_t chunk = len - sent_bytes;
            if (chunk > 1460) {
                chunk = 1460;
            }
            uint8_t flags = TCP_FLAG_ACK;
            if (sent_bytes + chunk >= len) {
                flags |= TCP_FLAG_PSH;
            }
            int res = tcp_send_segment(sock->local_ip, sock->local_port, dest_ip, dest_port, sock->snd_nxt, sock->rcv_nxt,
                                       flags, src + sent_bytes, chunk);
            if (res < 0) {
                return (sent_bytes > 0) ? (ssize_t)sent_bytes : -1;
            }
            sock->snd_nxt += (uint32_t)chunk;
            sent_bytes += chunk;
        }
        return (ssize_t)sent_bytes;
    } else if (sock->type == SOCK_DGRAM) {
        net_buf_t *pbuf = net_buf_alloc();
        if (!pbuf)
            return -1;
        memcpy(pbuf->data, buf, len);
        pbuf->len = len;
        pbuf->offset = 0;
        udp_output(sock->local_ip, sock->local_port, dest_ip, dest_port, pbuf);
        return (ssize_t)len;
    }

    return -1;
}

ssize_t sys_recvfrom(int fd, void *buf, size_t len, int flags, struct sockaddr *src_addr, uint32_t *addrlen) {
    (void)flags;
    socket_t *sock = (fd >= 0) ? get_socket_from_fd(fd) : NULL;
    if (!sock && fd >= 0)
        return -1;

    /* If called from vfs_read, sock was validated in caller */
    process_t *proc = sched_get_current_process();
    if (!sock && proc && fd == -1) {
        /* Generic read */
        return 0;
    }

    netif_poll_all();

    /* If non-blocking (MSG_DONTWAIT or O_NONBLOCK) and no data, return -EAGAIN */
    bool is_nonblock = (flags & 0x40);
    if (proc && fd >= 0 && fd < MAX_FD && proc->fds[fd]) {
        if (proc->fds[fd]->flags & 0x800) {
            is_nonblock = true;
        }
    }
    if (sock->domain == AF_UNIX) {
        if (is_nonblock && sock->rx_len == 0) {
            if (sock->state == SS_CLOSED || !sock->peer || sock->peer->state == SS_CLOSED) {
                return 0; /* EOF */
            }
            return -11; /* -EAGAIN */
        }
        while (sock->rx_len == 0) {
            if (sock->state == SS_CLOSED || !sock->peer || sock->peer->state == SS_CLOSED) {
                return 0; /* EOF */
            }
            thread_sleep(1);
        }
    } else if (sock->type == SOCK_DGRAM || sock->type == SOCK_RAW) {
        if (is_nonblock && sock->dgram_count == 0) {
            if (sock->state == SS_CLOSED) {
                return 0; /* EOF */
            }
            return -11; /* -EAGAIN */
        }
        while (sock->dgram_count == 0) {
            if (sock->state == SS_CLOSED) {
                return 0; /* EOF */
            }
            netif_poll_all();
            thread_sleep(1);
        }
    } else {
        if (is_nonblock && sock->rx_len == 0) {
            if (sock->state == SS_CLOSED || (sock->type == SOCK_STREAM && (sock->tcp_state == TCP_STATE_CLOSE_WAIT || sock->tcp_state == TCP_STATE_CLOSED))) {
                return 0; /* EOF */
            }
            return -11; /* -EAGAIN */
        }
        while (sock->rx_len == 0) {
            if (sock->state == SS_CLOSED || (sock->type == SOCK_STREAM && (sock->tcp_state == TCP_STATE_CLOSE_WAIT || sock->tcp_state == TCP_STATE_CLOSED))) {
                return 0; /* EOF */
            }
            netif_poll_all();
            thread_sleep(1);
        }
    }

    spinlock_acquire(&sock->lock);
    if (sock->type == SOCK_DGRAM || sock->type == SOCK_RAW) {
        if (sock->dgram_count == 0) {
            spinlock_release(&sock->lock);
            return is_nonblock ? -11 : 0;
        }

        dgram_meta_t meta = sock->dgram_queue[sock->dgram_head];
        sock->dgram_head = (sock->dgram_head + 1) % SOCK_DGRAM_QUEUE_LEN;
        sock->dgram_count--;

        size_t pkt_len = meta.len;
        size_t to_read = len < pkt_len ? len : pkt_len;
        uint8_t *dst = (uint8_t *)buf;

        for (size_t i = 0; i < to_read; i++) {
            dst[i] = sock->rx_buf[sock->rx_head];
            sock->rx_head = (sock->rx_head + 1) % SOCK_RX_BUF_SIZE;
        }
        if (pkt_len > to_read) {
            sock->rx_head = (sock->rx_head + (pkt_len - to_read)) % SOCK_RX_BUF_SIZE;
        }
        if (sock->rx_len >= pkt_len)
            sock->rx_len -= pkt_len;
        else
            sock->rx_len = 0;

        spinlock_release(&sock->lock);

        if (src_addr && addrlen && *addrlen >= sizeof(struct sockaddr_in)) {
            struct sockaddr_in *in = (struct sockaddr_in *)src_addr;
            in->sin_family = AF_INET;
            in->sin_addr.s_addr = meta.from_ip;
            in->sin_port = htons(meta.from_port);
            *addrlen = sizeof(struct sockaddr_in);
        }

        return (ssize_t)to_read;
    }

    size_t to_read = len < sock->rx_len ? len : sock->rx_len;
    uint8_t *dst = (uint8_t *)buf;

    for (size_t i = 0; i < to_read; i++) {
        dst[i] = sock->rx_buf[sock->rx_head];
        sock->rx_head = (sock->rx_head + 1) % SOCK_RX_BUF_SIZE;
    }
    sock->rx_len -= to_read;
    spinlock_release(&sock->lock);

    if (src_addr && addrlen && *addrlen >= sizeof(struct sockaddr_in)) {
        struct sockaddr_in *in = (struct sockaddr_in *)src_addr;
        in->sin_family = AF_INET;
        in->sin_addr.s_addr = sock->remote_ip;
        in->sin_port = htons(sock->remote_port);
        *addrlen = sizeof(struct sockaddr_in);
    }

    return (ssize_t)to_read;
}

int sys_shutdown(int fd, int how) {
    if (how < 0 || how > 2)
        return -22; /* -EINVAL */

    socket_t *sock = get_socket_from_fd(fd);
    if (!sock)
        return -9; /* -EBADF */

    if (how == SHUT_RD) {
        sock->shutdown_flags |= 1;
    } else if (how == SHUT_WR) {
        sock->shutdown_flags |= 2;
        if (sock->domain == AF_INET && sock->type == SOCK_STREAM &&
            (sock->tcp_state == TCP_STATE_ESTABLISHED || sock->tcp_state == TCP_STATE_SYN_RECEIVED)) {
            if (sock->local_ip == 0) {
                netif_t *def = ((sock->remote_ip & 0xFF) == 127) ? netif_get_loopback() : netif_get_default();
                if (def)
                    sock->local_ip = def->ip;
            }
            tcp_send_segment(sock->local_ip, sock->local_port, sock->remote_ip, sock->remote_port,
                             sock->snd_nxt, sock->rcv_nxt, TCP_FLAG_FIN | TCP_FLAG_ACK, NULL, 0);
            sock->snd_nxt++;
            sock->tcp_state = TCP_STATE_FIN_WAIT_1;
        }
    } else if (how == SHUT_RDWR) {
        sock->shutdown_flags |= 3;
        if (sock->domain == AF_INET && sock->type == SOCK_STREAM &&
            (sock->tcp_state == TCP_STATE_ESTABLISHED || sock->tcp_state == TCP_STATE_SYN_RECEIVED)) {
            if (sock->local_ip == 0) {
                netif_t *def = ((sock->remote_ip & 0xFF) == 127) ? netif_get_loopback() : netif_get_default();
                if (def)
                    sock->local_ip = def->ip;
            }
            tcp_send_segment(sock->local_ip, sock->local_port, sock->remote_ip, sock->remote_port,
                             sock->snd_nxt, sock->rcv_nxt, TCP_FLAG_FIN | TCP_FLAG_ACK, NULL, 0);
            sock->snd_nxt++;
            sock->tcp_state = TCP_STATE_FIN_WAIT_1;
        }
        sock->state = SS_CLOSED;
        if (sock->domain == AF_UNIX && sock->peer) {
            sock->peer->state = SS_CLOSED;
        }
    }
    return 0;
}

int sys_getsockname(int fd, struct sockaddr *addr, uint32_t *addrlen) {
    socket_t *sock = get_socket_from_fd(fd);
    if (!sock || !addr || !addrlen)
        return -1;

    if (sock->domain == AF_INET && *addrlen >= sizeof(struct sockaddr_in)) {
        struct sockaddr_in *in = (struct sockaddr_in *)addr;
        in->sin_family = AF_INET;
        in->sin_addr.s_addr = sock->local_ip;
        in->sin_port = htons(sock->local_port);
        *addrlen = sizeof(struct sockaddr_in);
        return 0;
    }
    return 0;
}

int sys_getpeername(int fd, struct sockaddr *addr, uint32_t *addrlen) {
    socket_t *sock = get_socket_from_fd(fd);
    if (!sock || !addr || !addrlen)
        return -1;

    if (sock->domain == AF_INET && *addrlen >= sizeof(struct sockaddr_in)) {
        struct sockaddr_in *in = (struct sockaddr_in *)addr;
        in->sin_family = AF_INET;
        in->sin_addr.s_addr = sock->remote_ip;
        in->sin_port = htons(sock->remote_port);
        *addrlen = sizeof(struct sockaddr_in);
        return 0;
    }
    return -1;
}

int sys_setsockopt(int fd, int level, int optname, const void *optval, uint32_t optlen) {
    socket_t *sock = get_socket_from_fd(fd);
    if (!sock)
        return -9; /* -EBADF */
    if (!optval && optlen > 0)
        return -14; /* -EFAULT */

    uint8_t koptval[256];
    size_t klen = optlen > sizeof(koptval) ? sizeof(koptval) : (size_t)optlen;
    if (klen > 0) {
        if (!copy_from_user(koptval, (uintptr_t)optval, klen))
            return -14; /* -EFAULT */
    }

    if (level == SOL_SOCKET) {
        switch (optname) {
        case SO_REUSEADDR:
            if (klen < sizeof(int)) return -22;
            sock->so_reuseaddr = *(int *)koptval ? 1 : 0;
            return 0;
        case SO_REUSEPORT:
            if (klen < sizeof(int)) return -22;
            sock->so_reuseport = *(int *)koptval ? 1 : 0;
            return 0;
        case SO_BROADCAST:
            if (klen < sizeof(int)) return -22;
            sock->so_broadcast = *(int *)koptval ? 1 : 0;
            return 0;
        case SO_KEEPALIVE:
            if (klen < sizeof(int)) return -22;
            sock->so_keepalive = *(int *)koptval ? 1 : 0;
            return 0;
        case SO_PASSCRED:
            if (klen < sizeof(int)) return -22;
            sock->so_passcred = *(int *)koptval ? 1 : 0;
            return 0;
        case SO_DONTROUTE:
            if (klen < sizeof(int)) return -22;
            sock->so_dontroute = *(int *)koptval ? 1 : 0;
            return 0;
        case SO_OOBINLINE:
            if (klen < sizeof(int)) return -22;
            sock->so_oobinline = *(int *)koptval ? 1 : 0;
            return 0;
        case SO_PRIORITY:
            if (klen < sizeof(int)) return -22;
            sock->so_priority = *(int *)koptval;
            return 0;
        case SO_RCVBUF: {
            if (klen < sizeof(int)) return -22;
            int val = *(int *)koptval;
            if (val < 1024) val = 1024;
            if (val > 16 * 1024 * 1024) val = 16 * 1024 * 1024;
            sock->so_rcvbuf = val;
            return 0;
        }
        case SO_SNDBUF: {
            if (klen < sizeof(int)) return -22;
            int val = *(int *)koptval;
            if (val < 1024) val = 1024;
            if (val > 16 * 1024 * 1024) val = 16 * 1024 * 1024;
            sock->so_sndbuf = val;
            return 0;
        }
        case SO_RCVTIMEO: {
            struct { int64_t tv_sec; int64_t tv_usec; } tv;
            if (klen >= sizeof(tv)) {
                memcpy(&tv, koptval, sizeof(tv));
                sock->so_rcvtimeo_ms = tv.tv_sec * 1000 + tv.tv_usec / 1000;
            }
            return 0;
        }
        case SO_SNDTIMEO: {
            struct { int64_t tv_sec; int64_t tv_usec; } tv;
            if (klen >= sizeof(tv)) {
                memcpy(&tv, koptval, sizeof(tv));
                sock->so_sndtimeo_ms = tv.tv_sec * 1000 + tv.tv_usec / 1000;
            }
            return 0;
        }
        case SO_LINGER:
            if (klen >= sizeof(struct linger_k)) {
                memcpy(&sock->so_linger, koptval, sizeof(struct linger_k));
            }
            return 0;
        case SO_BINDTODEVICE:
            if (klen == 0 || koptval[0] == '\0') {
                sock->bind_device[0] = '\0';
            } else {
                strncpy(sock->bind_device, (char *)koptval, sizeof(sock->bind_device) - 1);
                sock->bind_device[sizeof(sock->bind_device) - 1] = '\0';
            }
            return 0;
        case SO_ATTACH_FILTER:
        case SO_DETACH_FILTER:
            return 0;
        default:
            return 0;
        }
    } else if (level == IPPROTO_TCP) {
        switch (optname) {
        case TCP_NODELAY:
            if (klen < sizeof(int)) return -22;
            sock->tcp_nodelay = *(int *)koptval ? 1 : 0;
            return 0;
        case TCP_MAXSEG:
            if (klen < sizeof(int)) return -22;
            sock->tcp_mss = *(int *)koptval;
            return 0;
        case TCP_CORK:
            if (klen < sizeof(int)) return -22;
            sock->tcp_cork = *(int *)koptval ? 1 : 0;
            return 0;
        case TCP_KEEPIDLE:
            if (klen < sizeof(int)) return -22;
            sock->tcp_keepidle = *(int *)koptval;
            return 0;
        case TCP_KEEPINTVL:
            if (klen < sizeof(int)) return -22;
            sock->tcp_keepintvl = *(int *)koptval;
            return 0;
        case TCP_KEEPCNT:
            if (klen < sizeof(int)) return -22;
            sock->tcp_keepcnt = *(int *)koptval;
            return 0;
        case TCP_QUICKACK:
            if (klen < sizeof(int)) return -22;
            sock->tcp_quickack = *(int *)koptval ? 1 : 0;
            return 0;
        default:
            return 0;
        }
    } else if (level == IPPROTO_IP) {
        switch (optname) {
        case IP_TTL:
            if (klen < sizeof(int)) return -22;
            sock->ip_ttl = (uint8_t)*(int *)koptval;
            return 0;
        case IP_TOS:
            if (klen < sizeof(int)) return -22;
            sock->ip_tos = (uint8_t)*(int *)koptval;
            return 0;
        case IP_MULTICAST_LOOP:
            if (klen < sizeof(int)) return -22;
            sock->ip_multicast_loop = *(int *)koptval ? 1 : 0;
            return 0;
        case IP_MULTICAST_TTL:
            if (klen < sizeof(int)) return -22;
            sock->ip_multicast_ttl = (uint8_t)*(int *)koptval;
            return 0;
        case IP_PKTINFO:
            if (klen < sizeof(int)) return -22;
            sock->ip_pktinfo = *(int *)koptval ? 1 : 0;
            return 0;
        case IP_ADD_MEMBERSHIP:
        case IP_DROP_MEMBERSHIP:
            return 0;
        default:
            return 0;
        }
    } else if (level == IPPROTO_IPV6) {
        if (optname == IPV6_V6ONLY) {
            if (klen < sizeof(int)) return -22;
            sock->ipv6_v6only = *(int *)koptval ? 1 : 0;
            return 0;
        }
    }

    return 0;
}

int sys_getsockopt(int fd, int level, int optname, void *optval, uint32_t *optlen) {
    socket_t *sock = get_socket_from_fd(fd);
    if (!sock)
        return -9; /* -EBADF */
    if (!optval || !optlen)
        return -14; /* -EFAULT */

    uint32_t max_len = 0;
    if (!copy_from_user(&max_len, (uintptr_t)optlen, sizeof(uint32_t)))
        return -14; /* -EFAULT */

    uint8_t kout[256];
    memset(kout, 0, sizeof(kout));
    size_t out_len = 0;

    if (level == SOL_SOCKET) {
        switch (optname) {
        case SO_TYPE: {
            int v = sock->type;
            memcpy(kout, &v, sizeof(int));
            out_len = sizeof(int);
            break;
        }
        case SO_ERROR: {
            int err = sock->so_error;
            sock->so_error = 0;
            if (err == 0 && sock->domain == AF_INET && sock->type == SOCK_STREAM && sock->tcp_state == TCP_STATE_CLOSED) {
                err = 111; /* ECONNREFUSED */
            }
            memcpy(kout, &err, sizeof(int));
            out_len = sizeof(int);
            break;
        }
        case SO_REUSEADDR: {
            int v = sock->so_reuseaddr;
            memcpy(kout, &v, sizeof(int));
            out_len = sizeof(int);
            break;
        }
        case SO_REUSEPORT: {
            int v = sock->so_reuseport;
            memcpy(kout, &v, sizeof(int));
            out_len = sizeof(int);
            break;
        }
        case SO_KEEPALIVE: {
            int v = sock->so_keepalive;
            memcpy(kout, &v, sizeof(int));
            out_len = sizeof(int);
            break;
        }
        case SO_BROADCAST: {
            int v = sock->so_broadcast;
            memcpy(kout, &v, sizeof(int));
            out_len = sizeof(int);
            break;
        }
        case SO_PASSCRED: {
            int v = sock->so_passcred;
            memcpy(kout, &v, sizeof(int));
            out_len = sizeof(int);
            break;
        }
        case SO_ACCEPTCONN: {
            int v = (sock->state == SS_LISTENING) ? 1 : 0;
            memcpy(kout, &v, sizeof(int));
            out_len = sizeof(int);
            break;
        }
        case SO_RCVBUF: {
            int v = sock->so_rcvbuf ? sock->so_rcvbuf : SOCK_RX_BUF_SIZE;
            memcpy(kout, &v, sizeof(int));
            out_len = sizeof(int);
            break;
        }
        case SO_SNDBUF: {
            int v = sock->so_sndbuf ? sock->so_sndbuf : SOCK_TX_BUF_SIZE;
            memcpy(kout, &v, sizeof(int));
            out_len = sizeof(int);
            break;
        }
        case SO_RCVTIMEO: {
            struct { int64_t tv_sec; int64_t tv_usec; } tv;
            tv.tv_sec = sock->so_rcvtimeo_ms / 1000;
            tv.tv_usec = (sock->so_rcvtimeo_ms % 1000) * 1000;
            memcpy(kout, &tv, sizeof(tv));
            out_len = sizeof(tv);
            break;
        }
        case SO_SNDTIMEO: {
            struct { int64_t tv_sec; int64_t tv_usec; } tv;
            tv.tv_sec = sock->so_sndtimeo_ms / 1000;
            tv.tv_usec = (sock->so_sndtimeo_ms % 1000) * 1000;
            memcpy(kout, &tv, sizeof(tv));
            out_len = sizeof(tv);
            break;
        }
        case SO_LINGER:
            memcpy(kout, &sock->so_linger, sizeof(struct linger_k));
            out_len = sizeof(struct linger_k);
            break;
        case SO_PEERCRED: {
            struct ucred_k ucred;
            process_t *curr = sched_get_current_process();
            ucred.pid = curr ? curr->pid : 1;
            ucred.uid = curr ? curr->uid : 0;
            ucred.gid = curr ? curr->gid : 0;
            memcpy(kout, &ucred, sizeof(ucred));
            out_len = sizeof(ucred);
            break;
        }
        case SO_BINDTODEVICE: {
            size_t slen = strlen(sock->bind_device) + 1;
            memcpy(kout, sock->bind_device, slen);
            out_len = slen;
            break;
        }
        default: {
            int v = 0;
            memcpy(kout, &v, sizeof(int));
            out_len = sizeof(int);
            break;
        }
        }
    } else if (level == IPPROTO_TCP) {
        switch (optname) {
        case TCP_NODELAY: {
            int v = sock->tcp_nodelay;
            memcpy(kout, &v, sizeof(int));
            out_len = sizeof(int);
            break;
        }
        case TCP_MAXSEG: {
            int v = sock->tcp_mss ? sock->tcp_mss : 1460;
            memcpy(kout, &v, sizeof(int));
            out_len = sizeof(int);
            break;
        }
        case TCP_CORK: {
            int v = sock->tcp_cork;
            memcpy(kout, &v, sizeof(int));
            out_len = sizeof(int);
            break;
        }
        case TCP_KEEPIDLE: {
            int v = sock->tcp_keepidle ? sock->tcp_keepidle : 7200;
            memcpy(kout, &v, sizeof(int));
            out_len = sizeof(int);
            break;
        }
        case TCP_KEEPINTVL: {
            int v = sock->tcp_keepintvl ? sock->tcp_keepintvl : 75;
            memcpy(kout, &v, sizeof(int));
            out_len = sizeof(int);
            break;
        }
        case TCP_KEEPCNT: {
            int v = sock->tcp_keepcnt ? sock->tcp_keepcnt : 9;
            memcpy(kout, &v, sizeof(int));
            out_len = sizeof(int);
            break;
        }
        case TCP_QUICKACK: {
            int v = sock->tcp_quickack;
            memcpy(kout, &v, sizeof(int));
            out_len = sizeof(int);
            break;
        }
        default: {
            int v = 0;
            memcpy(kout, &v, sizeof(int));
            out_len = sizeof(int);
            break;
        }
        }
    } else if (level == IPPROTO_IP) {
        switch (optname) {
        case IP_TTL: {
            int v = sock->ip_ttl ? sock->ip_ttl : 64;
            memcpy(kout, &v, sizeof(int));
            out_len = sizeof(int);
            break;
        }
        case IP_TOS: {
            int v = sock->ip_tos;
            memcpy(kout, &v, sizeof(int));
            out_len = sizeof(int);
            break;
        }
        case IP_MULTICAST_LOOP: {
            int v = sock->ip_multicast_loop;
            memcpy(kout, &v, sizeof(int));
            out_len = sizeof(int);
            break;
        }
        case IP_MULTICAST_TTL: {
            int v = sock->ip_multicast_ttl ? sock->ip_multicast_ttl : 1;
            memcpy(kout, &v, sizeof(int));
            out_len = sizeof(int);
            break;
        }
        case IP_PKTINFO: {
            int v = sock->ip_pktinfo;
            memcpy(kout, &v, sizeof(int));
            out_len = sizeof(int);
            break;
        }
        default: {
            int v = 0;
            memcpy(kout, &v, sizeof(int));
            out_len = sizeof(int);
            break;
        }
        }
    } else if (level == IPPROTO_IPV6) {
        if (optname == IPV6_V6ONLY) {
            int v = sock->ipv6_v6only;
            memcpy(kout, &v, sizeof(int));
            out_len = sizeof(int);
        }
    }

    if (out_len == 0) {
        int v = 0;
        memcpy(kout, &v, sizeof(int));
        out_len = sizeof(int);
    }

    if (out_len > max_len)
        out_len = max_len;

    if (!copy_to_user((uintptr_t)optval, kout, out_len))
        return -14; /* -EFAULT */

    uint32_t copied_len = (uint32_t)out_len;
    if (!copy_to_user((uintptr_t)optlen, &copied_len, sizeof(uint32_t)))
        return -14; /* -EFAULT */

    return 0;
}

static void socket_close_fd(int fd) {
    process_t *proc = sched_get_current_process();
    if (proc && fd >= 0 && fd < MAX_FD && proc->fds[fd]) {
        file_descriptor_t *fdesc = proc->fds[fd];
        proc->fds[fd] = NULL;
        proc->fd_cloexec[fd] = false;
        if (fdesc->node) {
            if (fdesc->node->device_data) {
                socket_destroy((socket_t *)fdesc->node->device_data);
            }
            kfree(fdesc->node);
        }
        kfree(fdesc);
    }
}

int sys_socketpair(int domain, int type, int protocol, int sv[2]) {
    if (domain != AF_UNIX && domain != AF_LOCAL)
        return -97; /* -EAFNOSUPPORT */

    int base_type = type & 0xF;
    if (base_type != SOCK_STREAM && base_type != SOCK_DGRAM)
        return -93; /* -EPROTOTYPE */

    int fd1 = sys_socket(AF_UNIX, base_type, protocol);
    int fd2 = sys_socket(AF_UNIX, base_type, protocol);
    if (fd1 < 0 || fd2 < 0) {
        if (fd1 >= 0) socket_close_fd(fd1);
        if (fd2 >= 0) socket_close_fd(fd2);
        return -1;
    }

    socket_t *s1 = get_socket_from_fd(fd1);
    socket_t *s2 = get_socket_from_fd(fd2);
    if (!s1 || !s2) {
        socket_close_fd(fd1);
        socket_close_fd(fd2);
        return -1;
    }

    s1->peer = s2;
    s2->peer = s1;
    s1->state = SS_CONNECTED;
    s2->state = SS_CONNECTED;

    int ksv[2] = { fd1, fd2 };
    if (!copy_to_user((uintptr_t)sv, ksv, sizeof(ksv))) {
        socket_close_fd(fd1);
        socket_close_fd(fd2);
        return -14; /* -EFAULT */
    }
    return 0;
}

ssize_t sys_sendmsg(int fd, const struct msghdr *msg, int flags) {
    if (!msg || (msg->msg_iovlen > 0 && !msg->msg_iov))
        return -22; /* EINVAL */

    socket_t *sock = get_socket_from_fd(fd);
    if (!sock)
        return -9; /* EBADF */

    process_t *proc = sched_get_current_process();
    if (!proc)
        return -1;

    /* Handle SCM_RIGHTS file descriptor passing for AF_UNIX */
    if (sock->domain == AF_UNIX && sock->peer && msg->msg_control && msg->msg_controllen >= sizeof(struct cmsghdr)) {
        struct cmsghdr *cmsg = (struct cmsghdr *)msg->msg_control;
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
            size_t data_len = (cmsg->cmsg_len > sizeof(struct cmsghdr)) ? (cmsg->cmsg_len - sizeof(struct cmsghdr)) : 0;
            size_t num_fds = data_len / sizeof(int);
            const int *fds = (const int *)CMSG_DATA(cmsg);

            spinlock_acquire(&sock->peer->lock);
            for (size_t i = 0; i < num_fds && sock->peer->passed_fd_count < UNIX_MAX_PASSED_FDS; i++) {
                int send_fd = fds[i];
                if (send_fd >= 0 && send_fd < MAX_FD && proc->fds[send_fd]) {
                    file_descriptor_t *src_f = proc->fds[send_fd];
                    src_f->refcount++;
                    sock->peer->passed_fds[sock->peer->passed_fd_count++] = src_f;
                }
            }
            spinlock_release(&sock->peer->lock);
        }
    }

    /* Send data chunks from msg_iov */
    ssize_t total_sent = 0;
    for (size_t i = 0; i < msg->msg_iovlen; i++) {
        void *base = msg->msg_iov[i].iov_base;
        size_t len = msg->msg_iov[i].iov_len;
        if (len == 0)
            continue;
        ssize_t ret = sys_sendto(fd, base, len, flags, (const struct sockaddr *)msg->msg_name, msg->msg_namelen);
        if (ret < 0) {
            return (total_sent > 0) ? total_sent : ret;
        }
        total_sent += ret;
    }

    return total_sent;
}

ssize_t sys_recvmsg(int fd, struct msghdr *msg, int flags) {
    if (!msg || (msg->msg_iovlen > 0 && !msg->msg_iov))
        return -22; /* EINVAL */

    socket_t *sock = get_socket_from_fd(fd);
    if (!sock)
        return -9; /* EBADF */

    process_t *proc = sched_get_current_process();
    if (!proc)
        return -1;

    /* Receive data chunks into msg_iov */
    ssize_t total_recv = 0;
    for (size_t i = 0; i < msg->msg_iovlen; i++) {
        void *base = msg->msg_iov[i].iov_base;
        size_t len = msg->msg_iov[i].iov_len;
        if (len == 0)
            continue;
        ssize_t ret = sys_recvfrom(fd, base, len, flags, (struct sockaddr *)msg->msg_name, &msg->msg_namelen);
        if (ret < 0) {
            return (total_recv > 0) ? total_recv : ret;
        }
        total_recv += ret;
    }

    /* Deliver passed file descriptors if SCM_RIGHTS was queued */
    if (sock->domain == AF_UNIX && msg->msg_control && msg->msg_controllen >= sizeof(struct cmsghdr)) {
        spinlock_acquire(&sock->lock);
        if (sock->passed_fd_count > 0) {
            struct cmsghdr *cmsg = (struct cmsghdr *)msg->msg_control;
            cmsg->cmsg_level = SOL_SOCKET;
            cmsg->cmsg_type = SCM_RIGHTS;

            int *out_fds = (int *)CMSG_DATA(cmsg);
            size_t installed = 0;
            for (size_t i = 0; i < sock->passed_fd_count; i++) {
                file_descriptor_t *fdesc = sock->passed_fds[i];
                sock->passed_fds[i] = NULL;
                if (!fdesc)
                    continue;

                int free_slot = -1;
                for (int slot = 0; slot < MAX_FD; slot++) {
                    if (!proc->fds[slot]) {
                        free_slot = slot;
                        break;
                    }
                }
                if (free_slot != -1) {
                    proc->fds[free_slot] = fdesc;
                    out_fds[installed++] = free_slot;
                } else {
                    fd_release(fdesc);
                }
            }
            sock->passed_fd_count = 0;
            cmsg->cmsg_len = (size_t)CMSG_LEN(sizeof(int) * installed);
            msg->msg_controllen = (size_t)CMSG_SPACE(sizeof(int) * installed);
        } else {
            msg->msg_controllen = 0;
        }
        spinlock_release(&sock->lock);
    }

    return total_recv;
}
