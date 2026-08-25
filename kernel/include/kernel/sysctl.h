#ifndef SZPONTOS_KERNEL_SYSCTL_H
#define SZPONTOS_KERNEL_SYSCTL_H

#include <kernel/types.h>

/* Sysctl node types */
#define CTLTYPE_NODE 1
#define CTLTYPE_INT 2
#define CTLTYPE_STRING 3
#define CTLTYPE_ULONG 4
#define CTLTYPE_OPAQUE 5

/* Sysctl access flags */
#define CTLFLAG_RD 0x01
#define CTLFLAG_WR 0x02
#define CTLFLAG_RW (CTLFLAG_RD | CTLFLAG_WR)

typedef struct sysctl_node sysctl_node_t;

typedef int (*sysctl_handler_t)(sysctl_node_t *node, void *oldp, size_t *oldlenp, const void *newp, size_t newlen);

struct sysctl_node {
    char name[64];
    uint32_t type;
    uint32_t flags;
    void *val_ptr;
    size_t val_size;
    sysctl_handler_t handler;
    const char *description;
};

#define SYSCTL_MAX_NODES 128

void sysctl_init(void);
int sysctl_register(const char *name, uint32_t type, uint32_t flags, void *val_ptr, size_t val_size,
                    sysctl_handler_t handler, const char *description);
int sysctl_byname(const char *name, void *oldp, size_t *oldlenp, const void *newp, size_t newlen);
size_t sysctl_get_all(char *buf, size_t max_len);

#endif /* SZPONTOS_KERNEL_SYSCTL_H */
