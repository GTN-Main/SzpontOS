/*
 * SzpontOS - Demonstrative Dynamic Device Driver Kernel Module (dummy_dev.sko)
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <kernel/module.h>
#include <kernel/kprint.h>
#include <kernel/string.h>
#include <mm/heap.h>
#include <fs/devfs.h>

MODULE_NAME("dummy_dev");
MODULE_AUTHOR("Szpont Industries");
MODULE_DESCRIPTION("Dynamic Character Device Driver (/dev/szpont_device)");
MODULE_LICENSE("GPL/MIT");
MODULE_VERSION("1.0.0");

static vfs_ops_t g_szpont_dev_ops;
static char g_device_msg[] = "SzpontOS Dynamic Character Device (.sko driver active!)\n";

static ssize_t szpont_dev_read(vfs_node_t *node, off_t offset, size_t size, void *buffer) {
    (void)node;
    if (!buffer || size == 0)
        return 0;

    size_t msg_len = strlen(g_device_msg);
    if (offset >= (off_t)msg_len)
        return 0; /* EOF */

    size_t available = msg_len - offset;
    size_t to_copy = (size < available) ? size : available;
    memcpy(buffer, g_device_msg + offset, to_copy);
    return (ssize_t)to_copy;
}

static ssize_t szpont_dev_write(vfs_node_t *node, off_t offset, size_t size, const void *buffer) {
    (void)node;
    (void)offset;
    if (!buffer || size == 0)
        return 0;

    kprintf("[szpont_device.sko] Received %lu bytes write: ", size);
    const char *str = (const char *)buffer;
    for (size_t i = 0; i < size && i < 64; i++) {
        kprintf("%c", str[i]);
    }
    if (size > 64)
        kprintf("...");
    kprintf("\n");

    return (ssize_t)size;
}

static int __init_dummy_dev(void) {
    klog_info("[dummy_dev.sko] Initializing dynamic driver...");

    memset(&g_szpont_dev_ops, 0, sizeof(vfs_ops_t));
    g_szpont_dev_ops.read = szpont_dev_read;
    g_szpont_dev_ops.write = szpont_dev_write;

    vfs_node_t *dev_node = (vfs_node_t *)kzalloc(sizeof(vfs_node_t));
    if (!dev_node)
        return -1;

    dev_node->flags = VFS_TYPE_CHARDEVICE;
    dev_node->permissions = 0666;
    dev_node->ops = &g_szpont_dev_ops;

    int ret = devfs_register_device("szpont_device", dev_node);
    if (ret != 0) {
        kfree(dev_node);
        klog_err("[dummy_dev.sko] Failed to register /dev/szpont_device");
        return ret;
    }

    klog_info("[dummy_dev.sko] Character device registered at '/dev/szpont_device'");
    return 0;
}

static void __cleanup_dummy_dev(void) {
    klog_info("[dummy_dev.sko] Unregistering '/dev/szpont_device'...");
    devfs_unregister_device("szpont_device");
    klog_info("[dummy_dev.sko] Driver cleaned up.");
}

module_init(__init_dummy_dev);
module_exit(__cleanup_dummy_dev);
