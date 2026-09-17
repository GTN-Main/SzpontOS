/*
 * SzpontOS - SysFS (System & Device Virtual Filesystem) Implementation
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <fs/sysfs.h>
#include <fs/vfs.h>
#include <mm/heap.h>
#include <kernel/string.h>
#include <kernel/kprint.h>
#include <kernel/smp.h>
#include <drivers/pci.h>
#include <net/net.h>

static vfs_ops_t g_sysfs_dir_ops;
static vfs_ops_t g_sysfs_file_ops;
static vfs_ops_t g_sysfs_symlink_ops;
static vfs_node_t *g_sysfs_root = NULL;

static ssize_t sysfs_vfs_read(vfs_node_t *node, off_t offset, size_t size, void *buffer) {
    if (!node || !node->device_data || !buffer)
        return -14; /* -EFAULT */

    sysfs_data_t *data = (sysfs_data_t *)node->device_data;
    char temp[1024];
    const char *src = NULL;
    size_t len = 0;

    if (data->type == SYSFS_NODE_STATIC) {
        src = data->static_content;
        len = data->static_len;
    } else if (data->type == SYSFS_NODE_DYNAMIC) {
        if (!data->dynamic_fn)
            return 0;
        len = data->dynamic_fn(temp, sizeof(temp), data->arg);
        src = temp;
    } else {
        return -21; /* -EISDIR */
    }

    if (!src || (size_t)offset >= len)
        return 0;

    size_t avail = len - (size_t)offset;
    size_t to_copy = (size < avail) ? size : avail;
    memcpy(buffer, src + offset, to_copy);
    return (ssize_t)to_copy;
}

static vfs_dirent_t *sysfs_vfs_readdir(vfs_node_t *node, uint32_t index) {
    if (!node || !node->device_data)
        return NULL;

    sysfs_data_t *data = (sysfs_data_t *)node->device_data;
    if (data->type != SYSFS_NODE_DIR)
        return NULL;

    static vfs_dirent_t de;
    memset(&de, 0, sizeof(vfs_dirent_t));

    if (index == 0) {
        de.inode = 1;
        de.type = VFS_TYPE_DIRECTORY;
        strcpy(de.name, ".");
        return &de;
    } else if (index == 1) {
        de.inode = 2;
        de.type = VFS_TYPE_DIRECTORY;
        strcpy(de.name, "..");
        return &de;
    }

    uint32_t cidx = index - 2;
    if (cidx >= data->child_count)
        return NULL;

    de.inode = index + 100;
    sysfs_data_t *cdata = (sysfs_data_t *)data->children[cidx].node->device_data;
    if (cdata && cdata->type == SYSFS_NODE_DIR) {
        de.type = VFS_TYPE_DIRECTORY;
    } else if (cdata && cdata->type == SYSFS_NODE_SYMLINK) {
        de.type = VFS_TYPE_SYMLINK;
    } else {
        de.type = VFS_TYPE_FILE;
    }
    strncpy(de.name, data->children[cidx].name, sizeof(de.name) - 1);
    return &de;
}

static vfs_node_t *sysfs_vfs_finddir(vfs_node_t *node, const char *name) {
    if (!node || !node->device_data || !name)
        return NULL;

    sysfs_data_t *data = (sysfs_data_t *)node->device_data;
    if (data->type != SYSFS_NODE_DIR)
        return NULL;

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        return node;

    for (size_t i = 0; i < data->child_count; i++) {
        if (strcmp(data->children[i].name, name) == 0) {
            return data->children[i].node;
        }
    }
    return NULL;
}

static ssize_t sysfs_vfs_readlink(vfs_node_t *node, char *buf, size_t size) {
    if (!node || !node->device_data || !buf || size == 0)
        return -14; /* -EFAULT */

    sysfs_data_t *data = (sysfs_data_t *)node->device_data;
    if (data->type != SYSFS_NODE_SYMLINK)
        return -22; /* -EINVAL */

    size_t len = strlen(data->symlink_target);
    size_t to_copy = (len < size) ? len : size;
    memcpy(buf, data->symlink_target, to_copy);
    return (ssize_t)to_copy;
}

static void sysfs_ops_init(void) {
    static bool init = false;
    if (!init) {
        memset(&g_sysfs_dir_ops, 0, sizeof(vfs_ops_t));
        g_sysfs_dir_ops.readdir = sysfs_vfs_readdir;
        g_sysfs_dir_ops.finddir = sysfs_vfs_finddir;

        memset(&g_sysfs_file_ops, 0, sizeof(vfs_ops_t));
        g_sysfs_file_ops.read = sysfs_vfs_read;

        memset(&g_sysfs_symlink_ops, 0, sizeof(vfs_ops_t));
        g_sysfs_symlink_ops.readlink = sysfs_vfs_readlink;
        init = true;
    }
}

static void sysfs_add_child(vfs_node_t *parent, const char *name, vfs_node_t *child) {
    if (!parent || !child || !name)
        return;
    sysfs_data_t *pdata = (sysfs_data_t *)parent->device_data;
    if (!pdata || pdata->type != SYSFS_NODE_DIR || pdata->child_count >= SYSFS_MAX_CHILDREN)
        return;

    strncpy(pdata->children[pdata->child_count].name, name, sizeof(pdata->children[pdata->child_count].name) - 1);
    pdata->children[pdata->child_count].node = child;
    pdata->child_count++;
}

vfs_node_t *sysfs_create_dir(vfs_node_t *parent, const char *name) {
    sysfs_ops_init();
    vfs_node_t *node = (vfs_node_t *)kzalloc(sizeof(vfs_node_t));
    if (!node)
        return NULL;

    strncpy(node->name, name, sizeof(node->name) - 1);
    node->flags = VFS_TYPE_DIRECTORY;
    node->permissions = 0555;
    node->ops = &g_sysfs_dir_ops;

    sysfs_data_t *data = (sysfs_data_t *)kzalloc(sizeof(sysfs_data_t));
    if (!data) {
        kfree(node);
        return NULL;
    }
    data->type = SYSFS_NODE_DIR;
    node->device_data = data;

    if (parent) {
        sysfs_add_child(parent, name, node);
    }
    return node;
}

vfs_node_t *sysfs_create_file(vfs_node_t *parent, const char *name, const char *content) {
    sysfs_ops_init();
    vfs_node_t *node = (vfs_node_t *)kzalloc(sizeof(vfs_node_t));
    if (!node)
        return NULL;

    strncpy(node->name, name, sizeof(node->name) - 1);
    node->flags = VFS_TYPE_FILE;
    node->permissions = 0444;
    node->ops = &g_sysfs_file_ops;

    sysfs_data_t *data = (sysfs_data_t *)kzalloc(sizeof(sysfs_data_t));
    if (!data) {
        kfree(node);
        return NULL;
    }
    data->type = SYSFS_NODE_STATIC;
    data->static_len = content ? strlen(content) : 0;
    data->static_content = content ? strdup(content) : NULL;
    node->length = data->static_len;
    node->device_data = data;

    if (parent) {
        sysfs_add_child(parent, name, node);
    }
    return node;
}

vfs_node_t *sysfs_create_dynamic_file(vfs_node_t *parent, const char *name, sysfs_read_func_t fn, void *arg) {
    sysfs_ops_init();
    vfs_node_t *node = (vfs_node_t *)kzalloc(sizeof(vfs_node_t));
    if (!node)
        return NULL;

    strncpy(node->name, name, sizeof(node->name) - 1);
    node->flags = VFS_TYPE_FILE;
    node->permissions = 0444;
    node->ops = &g_sysfs_file_ops;

    sysfs_data_t *data = (sysfs_data_t *)kzalloc(sizeof(sysfs_data_t));
    if (!data) {
        kfree(node);
        return NULL;
    }
    data->type = SYSFS_NODE_DYNAMIC;
    data->dynamic_fn = fn;
    data->arg = arg;
    node->device_data = data;

    if (parent) {
        sysfs_add_child(parent, name, node);
    }
    return node;
}

vfs_node_t *sysfs_create_symlink(vfs_node_t *parent, const char *name, const char *target) {
    sysfs_ops_init();
    vfs_node_t *node = (vfs_node_t *)kzalloc(sizeof(vfs_node_t));
    if (!node)
        return NULL;

    strncpy(node->name, name, sizeof(node->name) - 1);
    node->flags = VFS_TYPE_SYMLINK;
    node->permissions = 0777;
    node->ops = &g_sysfs_symlink_ops;

    sysfs_data_t *data = (sysfs_data_t *)kzalloc(sizeof(sysfs_data_t));
    if (!data) {
        kfree(node);
        return NULL;
    }
    data->type = SYSFS_NODE_SYMLINK;
    if (target) {
        strncpy(data->symlink_target, target, sizeof(data->symlink_target) - 1);
    }
    node->device_data = data;

    if (parent) {
        sysfs_add_child(parent, name, node);
    }
    return node;
}

/* =========================================================================
 * Dynamic Content Generators
 * ========================================================================= */

static size_t gen_cpu_range(char *buf, size_t max_len, void *arg) {
    (void)arg;
    uint32_t count = smp_get_cpu_count();
    if (count <= 1)
        return ksnprintf(buf, max_len, "0\n");
    return ksnprintf(buf, max_len, "0-%u\n", count - 1);
}

static size_t gen_pci_vendor(char *buf, size_t max_len, void *arg) {
    pci_device_t *pdev = (pci_device_t *)arg;
    return ksnprintf(buf, max_len, "0x%04x\n", pdev->vendor_id);
}

static size_t gen_pci_device(char *buf, size_t max_len, void *arg) {
    pci_device_t *pdev = (pci_device_t *)arg;
    return ksnprintf(buf, max_len, "0x%04x\n", pdev->device_id);
}

static size_t gen_pci_class(char *buf, size_t max_len, void *arg) {
    pci_device_t *pdev = (pci_device_t *)arg;
    return ksnprintf(buf, max_len, "0x%02x%02x%02x\n", pdev->class_code, pdev->subclass, pdev->prog_if);
}

static size_t gen_net_address(char *buf, size_t max_len, void *arg) {
    netif_t *nif = (netif_t *)arg;
    return ksnprintf(buf, max_len, "%02x:%02x:%02x:%02x:%02x:%02x\n",
                     nif->mac[0], nif->mac[1], nif->mac[2],
                     nif->mac[3], nif->mac[4], nif->mac[5]);
}

static size_t gen_net_operstate(char *buf, size_t max_len, void *arg) {
    netif_t *nif = (netif_t *)arg;
    bool is_up = (nif->flags & NETIF_FLAG_UP) != 0;
    return ksnprintf(buf, max_len, "%s\n", is_up ? "up" : "down");
}

/* =========================================================================
 * SysFS Subsystem Initialization
 * ========================================================================= */

vfs_node_t *sysfs_init(void) {
    sysfs_ops_init();

    g_sysfs_root = sysfs_create_dir(NULL, "sys");
    if (!g_sysfs_root)
        return NULL;

    /* /sys/devices */
    vfs_node_t *devices_dir = sysfs_create_dir(g_sysfs_root, "devices");

    /* /sys/devices/system/cpu */
    vfs_node_t *system_dir = sysfs_create_dir(devices_dir, "system");
    vfs_node_t *cpu_dir = sysfs_create_dir(system_dir, "cpu");
    sysfs_create_dynamic_file(cpu_dir, "online", gen_cpu_range, NULL);
    sysfs_create_dynamic_file(cpu_dir, "present", gen_cpu_range, NULL);
    sysfs_create_dynamic_file(cpu_dir, "possible", gen_cpu_range, NULL);
    sysfs_create_file(cpu_dir, "kernel_max", "63\n");

    uint32_t cpu_count = smp_get_cpu_count();
    for (uint32_t i = 0; i < cpu_count && i < 64; i++) {
        char cpuname[16];
        ksnprintf(cpuname, sizeof(cpuname), "cpu%u", i);
        vfs_node_t *per_cpu = sysfs_create_dir(cpu_dir, cpuname);
        vfs_node_t *topo = sysfs_create_dir(per_cpu, "topology");
        char core_id_str[16];
        ksnprintf(core_id_str, sizeof(core_id_str), "%u\n", i);
        sysfs_create_file(topo, "core_id", core_id_str);
    }

    /* /sys/bus/pci */
    vfs_node_t *bus_dir = sysfs_create_dir(g_sysfs_root, "bus");
    vfs_node_t *pci_bus = sysfs_create_dir(bus_dir, "pci");
    vfs_node_t *pci_devices = sysfs_create_dir(pci_bus, "devices");

    /* Populate detected PCI devices */
    for (pci_device_t *pdev = pci_get_device_list(); pdev; pdev = pdev->next) {
        char dev_slot_name[32];
        ksnprintf(dev_slot_name, sizeof(dev_slot_name), "0000:%02x:%02x.%u",
                  pdev->bus, pdev->slot, pdev->func);
        vfs_node_t *dev_node = sysfs_create_dir(pci_devices, dev_slot_name);
        sysfs_create_dynamic_file(dev_node, "vendor", gen_pci_vendor, pdev);
        sysfs_create_dynamic_file(dev_node, "device", gen_pci_device, pdev);
        sysfs_create_dynamic_file(dev_node, "class", gen_pci_class, pdev);
    }

    /* /sys/class */
    vfs_node_t *class_dir = sysfs_create_dir(g_sysfs_root, "class");

    /* /sys/class/drm */
    vfs_node_t *drm_class = sysfs_create_dir(class_dir, "drm");
    vfs_node_t *card0 = sysfs_create_dir(drm_class, "card0");
    sysfs_create_file(card0, "dev", "226:0\n");
    vfs_node_t *render128 = sysfs_create_dir(drm_class, "renderD128");
    sysfs_create_file(render128, "dev", "226:128\n");

    /* /sys/class/net */
    vfs_node_t *net_class = sysfs_create_dir(class_dir, "net");
    for (netif_t *nif = netif_get_list(); nif; nif = nif->next) {
        vfs_node_t *nif_dir = sysfs_create_dir(net_class, nif->name);
        sysfs_create_dynamic_file(nif_dir, "address", gen_net_address, nif);
        sysfs_create_dynamic_file(nif_dir, "operstate", gen_net_operstate, nif);
        sysfs_create_file(nif_dir, "mtu", (nif->flags & NETIF_FLAG_LOOPBACK) ? "65536\n" : "1500\n");
    }

    /* /sys/kernel */
    vfs_node_t *kernel_dir = sysfs_create_dir(g_sysfs_root, "kernel");
    sysfs_create_file(kernel_dir, "uevent_seqnum", "1\n");

    klog_info("SysFS: Virtual System Filesystem initialized at /sys");
    return g_sysfs_root;
}
