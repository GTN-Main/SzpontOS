/*
 * SzpontOS - Kernel Sysctl Subsystem (Hierarchical MIB Tree)
 * Inspired by FreeBSD sys/kern/kern_sysctl.c
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <kernel/sysctl.h>
#include <kernel/string.h>
#include <kernel/kprint.h>
#include <kernel/spinlock.h>
#include <mm/pmm.h>
#include <sched/process.h>
#include <sched/sched.h>

static sysctl_node_t g_sysctl_nodes[SYSCTL_MAX_NODES];
static size_t g_sysctl_count = 0;
static spinlock_t g_sysctl_lock = SPINLOCK_INIT;

/* Static MIB buffers */
static char g_kern_ostype[32] = "SzpontOS";
static char g_kern_osrelease[32] = "0.1.0";
static uint64_t g_kern_osrevision = 199506;
static char g_kern_version[128] = "SzpontOS 0.1.0 (Higher-Half x86_64) #1 SMP";
static char g_kern_hostname[64] = "szpontos-box";
static char g_kern_domainname[64] = "local";
static uint64_t g_kern_maxproc = 1024;
static char g_hw_machine[32] = "x86_64";
static char g_hw_model[64] = "x86_64 CPU";
static uint64_t g_hw_ncpu = 1;
static uint64_t g_hw_pagesize = 4096;
static int g_net_ip_forwarding = 0;

static int handle_hw_physmem(sysctl_node_t *node, void *oldp, size_t *oldlenp, const void *newp, size_t newlen) {
    (void)node;
    (void)newp;
    (void)newlen;
    uint64_t total = pmm_get_total_memory();
    if (oldp && oldlenp) {
        size_t copysz = (*oldlenp < sizeof(uint64_t)) ? *oldlenp : sizeof(uint64_t);
        memcpy(oldp, &total, copysz);
        *oldlenp = sizeof(uint64_t);
    }
    return 0;
}

static int handle_hw_usermem(sysctl_node_t *node, void *oldp, size_t *oldlenp, const void *newp, size_t newlen) {
    (void)node;
    (void)newp;
    (void)newlen;
    uint64_t free_mem = pmm_get_free_memory();
    if (oldp && oldlenp) {
        size_t copysz = (*oldlenp < sizeof(uint64_t)) ? *oldlenp : sizeof(uint64_t);
        memcpy(oldp, &free_mem, copysz);
        *oldlenp = sizeof(uint64_t);
    }
    return 0;
}

int sysctl_register(const char *name, uint32_t type, uint32_t flags, void *val_ptr, size_t val_size,
                    sysctl_handler_t handler, const char *description) {
    if (!name)
        return -1;
    spinlock_acquire(&g_sysctl_lock);
    if (g_sysctl_count >= SYSCTL_MAX_NODES) {
        spinlock_release(&g_sysctl_lock);
        return -1;
    }

    /* Check for duplicate */
    for (size_t i = 0; i < g_sysctl_count; i++) {
        if (strcmp(g_sysctl_nodes[i].name, name) == 0) {
            spinlock_release(&g_sysctl_lock);
            return -1;
        }
    }

    sysctl_node_t *node = &g_sysctl_nodes[g_sysctl_count++];
    strncpy(node->name, name, sizeof(node->name) - 1);
    node->name[sizeof(node->name) - 1] = '\0';
    node->type = type;
    node->flags = flags;
    node->val_ptr = val_ptr;
    node->val_size = val_size;
    node->handler = handler;
    node->description = description;

    spinlock_release(&g_sysctl_lock);
    return 0;
}

int sysctl_byname(const char *name, void *oldp, size_t *oldlenp, const void *newp, size_t newlen) {
    if (!name)
        return -1;

    spinlock_acquire(&g_sysctl_lock);
    sysctl_node_t *node = NULL;
    for (size_t i = 0; i < g_sysctl_count; i++) {
        if (strcmp(g_sysctl_nodes[i].name, name) == 0) {
            node = &g_sysctl_nodes[i];
            break;
        }
    }

    if (!node) {
        spinlock_release(&g_sysctl_lock);
        return -1; /* ENOENT */
    }

    if (newp && newlen > 0) {
        if (!(node->flags & CTLFLAG_WR)) {
            spinlock_release(&g_sysctl_lock);
            return -1; /* EPERM */
        }
        process_t *proc = sched_get_current_process();
        if (proc && proc->euid != 0) {
            spinlock_release(&g_sysctl_lock);
            return -1; /* EPERM */
        }
    }

    if (node->handler) {
        int res = node->handler(node, oldp, oldlenp, newp, newlen);
        spinlock_release(&g_sysctl_lock);
        return res;
    }

    /* Read old value */
    if (oldp && oldlenp && node->val_ptr) {
        size_t sz = node->val_size;
        if (node->type == CTLTYPE_STRING) {
            sz = strlen((const char *)node->val_ptr) + 1;
        }
        size_t copy_sz = (*oldlenp < sz) ? *oldlenp : sz;
        memcpy(oldp, node->val_ptr, copy_sz);
        *oldlenp = sz;
    } else if (oldlenp && node->val_ptr) {
        if (node->type == CTLTYPE_STRING) {
            *oldlenp = strlen((const char *)node->val_ptr) + 1;
        } else {
            *oldlenp = node->val_size;
        }
    }

    /* Write new value */
    if (newp && newlen > 0 && node->val_ptr) {
        if (node->type == CTLTYPE_STRING) {
            size_t copy_sz = (newlen < node->val_size) ? newlen : (node->val_size - 1);
            memcpy(node->val_ptr, newp, copy_sz);
            ((char *)node->val_ptr)[copy_sz] = '\0';
        } else if (node->type == CTLTYPE_INT && newlen >= sizeof(int)) {
            memcpy(node->val_ptr, newp, sizeof(int));
        } else if (node->type == CTLTYPE_ULONG && newlen >= sizeof(uint64_t)) {
            memcpy(node->val_ptr, newp, sizeof(uint64_t));
        }
    }

    spinlock_release(&g_sysctl_lock);
    return 0;
}

size_t sysctl_get_all(char *buf, size_t max_len) {
    if (!buf || max_len == 0)
        return 0;
    spinlock_acquire(&g_sysctl_lock);

    size_t off = 0;
    for (size_t i = 0; i < g_sysctl_count; i++) {
        sysctl_node_t *node = &g_sysctl_nodes[i];
        char line[256];
        int len = 0;

        if (node->type == CTLTYPE_STRING) {
            len = ksnprintf(line, sizeof(line), "%s = %s\n", node->name, (const char *)node->val_ptr);
        } else if (node->type == CTLTYPE_INT) {
            len = ksnprintf(line, sizeof(line), "%s = %d\n", node->name, *(int *)node->val_ptr);
        } else if (node->type == CTLTYPE_ULONG) {
            if (node->handler) {
                uint64_t val = 0;
                size_t valsz = sizeof(val);
                node->handler(node, &val, &valsz, NULL, 0);
                len = ksnprintf(line, sizeof(line), "%s = %lu\n", node->name, val);
            } else {
                len = ksnprintf(line, sizeof(line), "%s = %lu\n", node->name, *(uint64_t *)node->val_ptr);
            }
        }

        if (len > 0 && off + (size_t)len < max_len) {
            memcpy(buf + off, line, (size_t)len);
            off += (size_t)len;
        }
    }

    buf[off] = '\0';
    spinlock_release(&g_sysctl_lock);
    return off;
}

void sysctl_init(void) {
    spinlock_init(&g_sysctl_lock);
    g_sysctl_count = 0;

    sysctl_register("kern.ostype", CTLTYPE_STRING, CTLFLAG_RD, g_kern_ostype, sizeof(g_kern_ostype), NULL,
                    "Operating system type");
    sysctl_register("kern.osrelease", CTLTYPE_STRING, CTLFLAG_RD, g_kern_osrelease, sizeof(g_kern_osrelease), NULL,
                    "Operating system release");
    sysctl_register("kern.osrevision", CTLTYPE_ULONG, CTLFLAG_RD, &g_kern_osrevision, sizeof(g_kern_osrevision), NULL,
                    "Operating system revision");
    sysctl_register("kern.version", CTLTYPE_STRING, CTLFLAG_RD, g_kern_version, sizeof(g_kern_version), NULL,
                    "Kernel build version");
    sysctl_register("kern.hostname", CTLTYPE_STRING, CTLFLAG_RW, g_kern_hostname, sizeof(g_kern_hostname), NULL,
                    "System hostname");
    sysctl_register("kern.domainname", CTLTYPE_STRING, CTLFLAG_RW, g_kern_domainname, sizeof(g_kern_domainname), NULL,
                    "System domain name");
    sysctl_register("kern.maxproc", CTLTYPE_ULONG, CTLFLAG_RD, &g_kern_maxproc, sizeof(g_kern_maxproc), NULL,
                    "Maximum number of processes");

    sysctl_register("hw.machine", CTLTYPE_STRING, CTLFLAG_RD, g_hw_machine, sizeof(g_hw_machine), NULL,
                    "Target machine architecture");
    sysctl_register("hw.model", CTLTYPE_STRING, CTLFLAG_RD, g_hw_model, sizeof(g_hw_model), NULL, "Processor model");
    sysctl_register("hw.ncpu", CTLTYPE_ULONG, CTLFLAG_RD, &g_hw_ncpu, sizeof(g_hw_ncpu), NULL, "Number of active CPUs");
    sysctl_register("hw.pagesize", CTLTYPE_ULONG, CTLFLAG_RD, &g_hw_pagesize, sizeof(g_hw_pagesize), NULL,
                    "Hardware page size in bytes");
    sysctl_register("hw.physmem", CTLTYPE_ULONG, CTLFLAG_RD, NULL, sizeof(uint64_t), handle_hw_physmem,
                    "Total physical memory in bytes");
    sysctl_register("hw.usermem", CTLTYPE_ULONG, CTLFLAG_RD, NULL, sizeof(uint64_t), handle_hw_usermem,
                    "Available user memory in bytes");

    sysctl_register("net.inet.ip.forwarding", CTLTYPE_INT, CTLFLAG_RW, &g_net_ip_forwarding, sizeof(int), NULL,
                    "IPv4 packet forwarding flag");

    klog_info("sysctl: MIB tree registered (%lu nodes initialized)", g_sysctl_count);
}
