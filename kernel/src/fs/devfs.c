#include <fs/devfs.h>
#include <drivers/serial.h>
#include <drivers/framebuffer.h>
#include <drivers/keyboard.h>
#include <drivers/ps2_mouse.h>
#include <drivers/speaker.h>
#include <drivers/tty.h>
#include <sched/sched.h>
#include <sched/process.h>
#include <kernel/signal.h>
#include <mm/heap.h>
#include <kernel/string.h>
#include <kernel/kprint.h>

#define MAX_DEVFS_ENTRIES 128

typedef struct devfs_entry {
    vfs_node_t *node;
} devfs_entry_t;

static vfs_node_t *g_devfs_root = NULL;
static devfs_entry_t g_devices[MAX_DEVFS_ENTRIES];
static size_t g_device_count = 0;

static vfs_ops_t g_devfs_dir_ops;
static vfs_ops_t g_null_ops;
static vfs_ops_t g_zero_ops;
static vfs_ops_t g_serial_ops;
static vfs_ops_t g_tty_ops;
static vfs_ops_t g_psaux_ops;
static vfs_ops_t g_speaker_ops;
static vfs_ops_t g_block_ops;

/* /dev/null */
static ssize_t devfs_null_read(vfs_node_t *node, off_t offset, size_t size, void *buffer) {
    UNUSED(node);
    UNUSED(offset);
    UNUSED(size);
    UNUSED(buffer);
    return 0; /* EOF */
}

static ssize_t devfs_null_write(vfs_node_t *node, off_t offset, size_t size, const void *buffer) {
    UNUSED(node);
    UNUSED(offset);
    UNUSED(buffer);
    return (ssize_t)size; /* Discarded */
}

/* /dev/zero */
static ssize_t devfs_zero_read(vfs_node_t *node, off_t offset, size_t size, void *buffer) {
    UNUSED(node);
    UNUSED(offset);
    if (buffer)
        memset(buffer, 0, size);
    return (ssize_t)size;
}

static ssize_t devfs_zero_write(vfs_node_t *node, off_t offset, size_t size, const void *buffer) {
    UNUSED(node);
    UNUSED(offset);
    UNUSED(buffer);
    return (ssize_t)size;
}

/* /dev/serial */
static ssize_t devfs_serial_read(vfs_node_t *node, off_t offset, size_t size, void *buffer) {
    UNUSED(node);
    UNUSED(offset);
    if (!buffer || size == 0)
        return 0;
    char *buf = (char *)buffer;
    for (size_t i = 0; i < size; i++) {
        buf[i] = serial_getc();
    }
    return (ssize_t)size;
}

static ssize_t devfs_serial_write(vfs_node_t *node, off_t offset, size_t size, const void *buffer) {
    UNUSED(node);
    UNUSED(offset);
    if (buffer && size > 0) {
        serial_write((const char *)buffer, size);
    }
    return (ssize_t)size;
}

/* /dev/tty & /dev/console */
static ssize_t devfs_tty_read(vfs_node_t *node, off_t offset, size_t size, void *buffer) {
    UNUSED(node);
    UNUSED(offset);
    return tty_read(buffer, size);
}

static ssize_t devfs_tty_write(vfs_node_t *node, off_t offset, size_t size, const void *buffer) {
    UNUSED(node);
    UNUSED(offset);
    return tty_write(buffer, size);
}

static int devfs_tty_ioctl(vfs_node_t *node, uint64_t request, uintptr_t arg) {
    UNUSED(node);
    return tty_ioctl(request, (void *)arg);
}

/* /dev/psaux (PS/2 Mouse) */
static ssize_t devfs_psaux_read(vfs_node_t *node, off_t offset, size_t size, void *buffer) {
    UNUSED(node);
    UNUSED(offset);
    return ps2_mouse_devfs_read(buffer, size);
}

static ssize_t devfs_psaux_write(vfs_node_t *node, off_t offset, size_t size, const void *buffer) {
    UNUSED(node);
    UNUSED(offset);
    UNUSED(buffer);
    return (ssize_t)size;
}

/* /dev/speaker */
static ssize_t devfs_speaker_write(vfs_node_t *node, off_t offset, size_t size, const void *buffer) {
    UNUSED(node);
    UNUSED(offset);
    if (!buffer || size < sizeof(uint32_t))
        return 0;
    uint32_t freq = *(const uint32_t *)buffer;
    speaker_beep(freq, 100);
    return (ssize_t)size;
}

/* Block device read/write */
static ssize_t devfs_block_read(vfs_node_t *node, off_t offset, size_t size, void *buffer) {
    block_device_t *bdev = (block_device_t *)node->device_data;
    if (!bdev || !buffer || size == 0)
        return 0;

    uint64_t start_sector = offset / bdev->sector_size;
    uint32_t num_sectors = (uint32_t)((size + bdev->sector_size - 1) / bdev->sector_size);

    uint8_t *tmp_buf = kmalloc(num_sectors * bdev->sector_size);
    if (!tmp_buf)
        return -1;

    if (bdev->read_blocks(bdev, start_sector, num_sectors, tmp_buf) != 0) {
        kfree(tmp_buf);
        return -1;
    }

    size_t in_sector_offset = offset % bdev->sector_size;
    memcpy(buffer, tmp_buf + in_sector_offset, size);
    kfree(tmp_buf);
    return (ssize_t)size;
}

static ssize_t devfs_block_write(vfs_node_t *node, off_t offset, size_t size, const void *buffer) {
    block_device_t *bdev = (block_device_t *)node->device_data;
    if (!bdev || !buffer || size == 0)
        return 0;

    uint64_t start_sector = offset / bdev->sector_size;
    uint32_t num_sectors = (uint32_t)((size + bdev->sector_size - 1) / bdev->sector_size);

    uint8_t *tmp_buf = kmalloc(num_sectors * bdev->sector_size);
    if (!tmp_buf)
        return -1;

    if (bdev->read_blocks(bdev, start_sector, num_sectors, tmp_buf) != 0) {
        kfree(tmp_buf);
        return -1;
    }

    size_t in_sector_offset = offset % bdev->sector_size;
    memcpy(tmp_buf + in_sector_offset, buffer, size);

    if (bdev->write_blocks(bdev, start_sector, num_sectors, tmp_buf) != 0) {
        kfree(tmp_buf);
        return -1;
    }

    kfree(tmp_buf);
    return (ssize_t)size;
}

/* DevFS directory operations */
static struct vfs_dirent *devfs_readdir(vfs_node_t *node, uint32_t index) {
    UNUSED(node);
    if (index >= g_device_count)
        return NULL;

    static vfs_dirent_t dirent;
    memset(&dirent, 0, sizeof(dirent));
    strncpy(dirent.name, g_devices[index].node->name, sizeof(dirent.name) - 1);
    dirent.inode = g_devices[index].node->inode;
    dirent.type = g_devices[index].node->flags;
    return &dirent;
}

static vfs_node_t *devfs_finddir(vfs_node_t *node, const char *name) {
    UNUSED(node);
    for (size_t i = 0; i < g_device_count; i++) {
        if (strcmp(g_devices[i].node->name, name) == 0) {
            return g_devices[i].node;
        }
    }
    return NULL;
}

int devfs_register_device(const char *name, vfs_node_t *node) {
    if (!name || !node || g_device_count >= MAX_DEVFS_ENTRIES)
        return -1;

    strncpy(node->name, name, sizeof(node->name) - 1);
    g_devices[g_device_count++].node = node;
    return 0;
}

int devfs_unregister_device(const char *name) {
    if (!name)
        return -1;
    for (size_t i = 0; i < g_device_count; i++) {
        if (g_devices[i].node && strcmp(g_devices[i].node->name, name) == 0) {
            vfs_node_t *node = g_devices[i].node;
            g_devices[i] = g_devices[g_device_count - 1];
            g_device_count--;
            kfree(node);
            return 0;
        }
    }
    return -1;
}

int devfs_register_block_device(block_device_t *bdev) {
    if (!bdev)
        return -1;

    vfs_node_t *node = (vfs_node_t *)kzalloc(sizeof(vfs_node_t));
    node->flags = VFS_TYPE_BLOCKDEVICE;
    node->length = bdev->sector_count * bdev->sector_size;
    node->permissions = 0660;
    node->uid = 0;
    node->gid = 0;
    node->device_data = bdev;
    node->ops = &g_block_ops;

    return devfs_register_device(bdev->name, node);
}

void devfs_init(void) {
    g_devfs_dir_ops.readdir = devfs_readdir;
    g_devfs_dir_ops.finddir = devfs_finddir;

    g_null_ops.read = devfs_null_read;
    g_null_ops.write = devfs_null_write;

    g_zero_ops.read = devfs_zero_read;
    g_zero_ops.write = devfs_zero_write;

    g_serial_ops.read = devfs_serial_read;
    g_serial_ops.write = devfs_serial_write;

    g_tty_ops.read = devfs_tty_read;
    g_tty_ops.write = devfs_tty_write;
    g_tty_ops.ioctl = devfs_tty_ioctl;

    g_psaux_ops.read = devfs_psaux_read;
    g_psaux_ops.write = devfs_psaux_write;

    g_speaker_ops.write = devfs_speaker_write;

    g_block_ops.read = devfs_block_read;
    g_block_ops.write = devfs_block_write;

    g_devfs_root = (vfs_node_t *)kzalloc(sizeof(vfs_node_t));
    strcpy(g_devfs_root->name, "dev");
    g_devfs_root->flags = VFS_TYPE_DIRECTORY;
    g_devfs_root->permissions = 0755;
    g_devfs_root->ops = &g_devfs_dir_ops;

    /* Register standard character device nodes */
    vfs_node_t *null_dev = (vfs_node_t *)kzalloc(sizeof(vfs_node_t));
    null_dev->flags = VFS_TYPE_CHARDEVICE;
    null_dev->ops = &g_null_ops;
    devfs_register_device("null", null_dev);

    vfs_node_t *zero_dev = (vfs_node_t *)kzalloc(sizeof(vfs_node_t));
    zero_dev->flags = VFS_TYPE_CHARDEVICE;
    zero_dev->ops = &g_zero_ops;
    devfs_register_device("zero", zero_dev);

    vfs_node_t *serial_dev = (vfs_node_t *)kzalloc(sizeof(vfs_node_t));
    serial_dev->flags = VFS_TYPE_CHARDEVICE;
    serial_dev->ops = &g_serial_ops;
    devfs_register_device("serial", serial_dev);

    vfs_node_t *tty_dev = (vfs_node_t *)kzalloc(sizeof(vfs_node_t));
    tty_dev->flags = VFS_TYPE_CHARDEVICE;
    tty_dev->ops = &g_tty_ops;
    devfs_register_device("tty", tty_dev);

    vfs_node_t *console_dev = (vfs_node_t *)kzalloc(sizeof(vfs_node_t));
    console_dev->flags = VFS_TYPE_CHARDEVICE;
    console_dev->ops = &g_tty_ops;
    devfs_register_device("console", console_dev);

    vfs_node_t *psaux_dev = (vfs_node_t *)kzalloc(sizeof(vfs_node_t));
    psaux_dev->flags = VFS_TYPE_CHARDEVICE;
    psaux_dev->ops = &g_psaux_ops;
    devfs_register_device("psaux", psaux_dev);

    vfs_node_t *speaker_dev = (vfs_node_t *)kzalloc(sizeof(vfs_node_t));
    speaker_dev->flags = VFS_TYPE_CHARDEVICE;
    speaker_dev->ops = &g_speaker_ops;
    devfs_register_device("speaker", speaker_dev);

    vfs_mount("/dev", g_devfs_root);
    klog_info("DevFS mounted at /dev with devices: null, zero, serial, tty, console, psaux, speaker");
}
