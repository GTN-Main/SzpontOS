/*
 * SzpontOS - In-Memory RAM Filesystem (TmpFS)
 * Inspired by FreeBSD sys/fs/tmpfs/
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <fs/tmpfs.h>
#include <fs/vfs.h>
#include <mm/heap.h>
#include <sched/process.h>
#include <sched/sched.h>
#include <kernel/string.h>
#include <kernel/spinlock.h>
#include <kernel/kprint.h>

#define TMPFS_MAX_CHILDREN 256

typedef struct tmpfs_node {
    vfs_node_t node;
    void *data;
    size_t capacity;
    struct tmpfs_node **children;
    size_t child_count;
    size_t child_capacity;
    struct tmpfs_node *parent;
    spinlock_t lock;
} tmpfs_node_t;

static vfs_ops_t g_tmpfs_file_ops;
static vfs_ops_t g_tmpfs_dir_ops;
static uint32_t g_tmpfs_next_inode = 10000;

static tmpfs_node_t *tmpfs_alloc_node(const char *name, uint32_t flags, mode_t mode, uid_t uid, gid_t gid) {
    tmpfs_node_t *tnode = (tmpfs_node_t *)kzalloc(sizeof(tmpfs_node_t));
    if (!tnode)
        return NULL;

    strncpy(tnode->node.name, name, sizeof(tnode->node.name) - 1);
    tnode->node.flags = flags;
    tnode->node.inode = g_tmpfs_next_inode++;
    tnode->node.length = 0;
    tnode->node.uid = uid;
    tnode->node.gid = gid;
    tnode->node.permissions = mode;
    tnode->node.device_data = tnode;
    tnode->lock = SPINLOCK_INIT;

    if (flags == VFS_TYPE_DIRECTORY) {
        tnode->node.ops = &g_tmpfs_dir_ops;
        tnode->child_capacity = 16;
        tnode->children = (tmpfs_node_t **)kzalloc(tnode->child_capacity * sizeof(tmpfs_node_t *));
    } else {
        tnode->node.ops = &g_tmpfs_file_ops;
    }

    return tnode;
}

static ssize_t tmpfs_file_read(vfs_node_t *node, off_t offset, size_t size, void *buffer) {
    tmpfs_node_t *tnode = (tmpfs_node_t *)node;
    if (!tnode || !buffer)
        return 0;

    spinlock_acquire(&tnode->lock);
    if (offset >= (off_t)tnode->node.length) {
        spinlock_release(&tnode->lock);
        return 0;
    }

    if (offset + size > tnode->node.length) {
        size = tnode->node.length - offset;
    }

    if (size > 0 && tnode->data) {
        memcpy(buffer, (const char *)tnode->data + offset, size);
    }
    spinlock_release(&tnode->lock);
    return (ssize_t)size;
}

static ssize_t tmpfs_file_write(vfs_node_t *node, off_t offset, size_t size, const void *buffer) {
    tmpfs_node_t *tnode = (tmpfs_node_t *)node;
    if (!tnode || !buffer)
        return -1;

    spinlock_acquire(&tnode->lock);
    size_t required = (size_t)offset + size;
    if (required > tnode->capacity) {
        size_t new_cap = (required > tnode->capacity * 2) ? (required + 512) : (tnode->capacity * 2 + 512);
        void *new_data = kmalloc(new_cap);
        if (!new_data) {
            spinlock_release(&tnode->lock);
            return -1;
        }

        if (tnode->data && tnode->node.length > 0) {
            memcpy(new_data, tnode->data, tnode->node.length);
            kfree(tnode->data);
        }
        tnode->data = new_data;
        tnode->capacity = new_cap;
    }

    memcpy((char *)tnode->data + offset, buffer, size);
    if ((size_t)offset + size > tnode->node.length) {
        tnode->node.length = (size_t)offset + size;
    }

    spinlock_release(&tnode->lock);
    return (ssize_t)size;
}

static struct vfs_dirent *tmpfs_dir_readdir(vfs_node_t *node, uint32_t index) {
    tmpfs_node_t *tnode = (tmpfs_node_t *)node;
    if (!tnode)
        return NULL;

    spinlock_acquire(&tnode->lock);
    if (index >= tnode->child_count) {
        spinlock_release(&tnode->lock);
        return NULL;
    }

    static vfs_dirent_t dirent;
    memset(&dirent, 0, sizeof(dirent));
    strncpy(dirent.name, tnode->children[index]->node.name, sizeof(dirent.name) - 1);
    dirent.inode = tnode->children[index]->node.inode;
    dirent.type = tnode->children[index]->node.flags;

    spinlock_release(&tnode->lock);
    return &dirent;
}

static vfs_node_t *tmpfs_dir_finddir(vfs_node_t *node, const char *name) {
    tmpfs_node_t *tnode = (tmpfs_node_t *)node;
    if (!tnode || !name)
        return NULL;

    spinlock_acquire(&tnode->lock);
    for (size_t i = 0; i < tnode->child_count; i++) {
        if (strcmp(tnode->children[i]->node.name, name) == 0) {
            vfs_node_t *res = &tnode->children[i]->node;
            spinlock_release(&tnode->lock);
            return res;
        }
    }
    spinlock_release(&tnode->lock);
    return NULL;
}

static int tmpfs_add_child(tmpfs_node_t *parent, tmpfs_node_t *child) {
    spinlock_acquire(&parent->lock);
    if (parent->child_count >= parent->child_capacity) {
        size_t new_cap = parent->child_capacity * 2;
        tmpfs_node_t **new_children = (tmpfs_node_t **)kmalloc(new_cap * sizeof(tmpfs_node_t *));
        if (!new_children) {
            spinlock_release(&parent->lock);
            return -1;
        }
        memcpy(new_children, parent->children, parent->child_count * sizeof(tmpfs_node_t *));
        kfree(parent->children);
        parent->children = new_children;
        parent->child_capacity = new_cap;
    }

    parent->children[parent->child_count++] = child;
    child->parent = parent;
    spinlock_release(&parent->lock);
    return 0;
}

static int tmpfs_dir_create(vfs_node_t *parent, const char *name, mode_t mode) {
    tmpfs_node_t *pnode = (tmpfs_node_t *)parent;
    if (!pnode || !name)
        return -1;

    process_t *proc = sched_get_current_process();
    uid_t uid = proc ? proc->euid : 0;
    gid_t gid = proc ? proc->egid : 0;

    tmpfs_node_t *child = tmpfs_alloc_node(name, VFS_TYPE_FILE, mode, uid, gid);
    if (!child)
        return -1;

    return tmpfs_add_child(pnode, child);
}

static int tmpfs_dir_mkdir(vfs_node_t *parent, const char *name, mode_t mode) {
    tmpfs_node_t *pnode = (tmpfs_node_t *)parent;
    if (!pnode || !name)
        return -1;

    process_t *proc = sched_get_current_process();
    uid_t uid = proc ? proc->euid : 0;
    gid_t gid = proc ? proc->egid : 0;

    tmpfs_node_t *child = tmpfs_alloc_node(name, VFS_TYPE_DIRECTORY, mode, uid, gid);
    if (!child)
        return -1;

    return tmpfs_add_child(pnode, child);
}

static int tmpfs_dir_unlink(vfs_node_t *parent, const char *name) {
    tmpfs_node_t *pnode = (tmpfs_node_t *)parent;
    if (!pnode || !name)
        return -1;

    spinlock_acquire(&pnode->lock);
    for (size_t i = 0; i < pnode->child_count; i++) {
        if (strcmp(pnode->children[i]->node.name, name) == 0) {
            tmpfs_node_t *victim = pnode->children[i];

            if (victim->data) {
                kfree(victim->data);
            }
            if (victim->children) {
                kfree(victim->children);
            }
            kfree(victim);

            for (size_t j = i; j < pnode->child_count - 1; j++) {
                pnode->children[j] = pnode->children[j + 1];
            }
            pnode->child_count--;
            spinlock_release(&pnode->lock);
            return 0;
        }
    }
    spinlock_release(&pnode->lock);
    return -1; /* ENOENT */
}

static int tmpfs_chmod(vfs_node_t *node, mode_t mode) {
    if (!node)
        return -1;
    tmpfs_node_t *tnode = (tmpfs_node_t *)node;
    spinlock_acquire(&tnode->lock);
    node->permissions = mode;
    spinlock_release(&tnode->lock);
    return 0;
}

static int tmpfs_chown(vfs_node_t *node, uid_t uid, gid_t gid) {
    if (!node)
        return -1;
    tmpfs_node_t *tnode = (tmpfs_node_t *)node;
    spinlock_acquire(&tnode->lock);
    if (uid != (uid_t)-1)
        node->uid = uid;
    if (gid != (gid_t)-1)
        node->gid = gid;
    spinlock_release(&tnode->lock);
    return 0;
}

static vfs_ops_t g_tmpfs_file_ops = {.read = tmpfs_file_read,
                                     .write = tmpfs_file_write,
                                     .open = NULL,
                                     .close = NULL,
                                     .chmod = tmpfs_chmod,
                                     .chown = tmpfs_chown};

static vfs_ops_t g_tmpfs_dir_ops = {.readdir = tmpfs_dir_readdir,
                                    .finddir = tmpfs_dir_finddir,
                                    .create = tmpfs_dir_create,
                                    .mkdir = tmpfs_dir_mkdir,
                                    .unlink = tmpfs_dir_unlink,
                                    .chmod = tmpfs_chmod,
                                    .chown = tmpfs_chown};

vfs_node_t *tmpfs_create_fs(const char *name, mode_t mode, uid_t uid, gid_t gid) {
    tmpfs_node_t *root = tmpfs_alloc_node(name, VFS_TYPE_DIRECTORY, mode, uid, gid);
    return root ? &root->node : NULL;
}

void tmpfs_init(void) {
    vfs_node_t *tmp_root = tmpfs_create_fs("tmp", 01777, 0, 0);
    if (tmp_root) {
        vfs_mount("/tmp", tmp_root);
        klog_info("tmpfs: Mounted in-memory filesystem at '/tmp'");
    }

    vfs_node_t *run_root = tmpfs_create_fs("run", 0755, 0, 0);
    if (run_root) {
        vfs_mount("/run", run_root);
        klog_info("tmpfs: Mounted in-memory filesystem at '/run'");
    }
}
