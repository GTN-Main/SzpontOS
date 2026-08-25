#include <fs/vfs.h>
#include <sched/process.h>
#include <sched/sched.h>
#include <mm/heap.h>
#include <kernel/string.h>
#include <kernel/kprint.h>
#include <kernel/spinlock.h>

#define MAX_MOUNTS 32

typedef struct vfs_mount {
    char path[128];
    vfs_node_t *node;
} vfs_mount_t;

static vfs_node_t *g_vfs_root = NULL;
static vfs_mount_t g_mounts[MAX_MOUNTS];
static size_t g_mount_count = 0;
static spinlock_t g_vfs_lock = SPINLOCK_INIT;

void vfs_init(void) {
    spinlock_init(&g_vfs_lock);
    memset(g_mounts, 0, sizeof(g_mounts));
    g_mount_count = 0;

    g_vfs_root = (vfs_node_t *)kzalloc(sizeof(vfs_node_t));
    strcpy(g_vfs_root->name, "/");
    g_vfs_root->flags = VFS_TYPE_DIRECTORY;
    g_vfs_root->permissions = 0755;
    g_vfs_root->uid = 0;
    g_vfs_root->gid = 0;

    klog_info("VFS initialized with root node '/'");
}

int vfs_mount(const char *path, vfs_node_t *node) {
    if (!path || !node)
        return -1;

    spinlock_acquire(&g_vfs_lock);
    if (g_mount_count >= MAX_MOUNTS) {
        spinlock_release(&g_vfs_lock);
        return -1;
    }

    if (strcmp(path, "/") == 0) {
        g_vfs_root = node;
        klog_info("VFS: Mounted root filesystem '/' at %s", node->name);
        spinlock_release(&g_vfs_lock);
        return 0;
    }

    /* Auto-create mount point directory in parent filesystem if not existing */
    if (g_vfs_root && g_vfs_root->ops && g_vfs_root->ops->mkdir) {
        const char *mount_name = (path[0] == '/') ? path + 1 : path;
        if (mount_name[0] && strchr(mount_name, '/') == NULL) {
            g_vfs_root->ops->mkdir(g_vfs_root, mount_name, 0755);
        }
    }

    strncpy(g_mounts[g_mount_count].path, path, 127);
    g_mounts[g_mount_count].node = node;
    g_mount_count++;

    klog_info("VFS: Mounted filesystem at '%s'", path);
    spinlock_release(&g_vfs_lock);
    return 0;
}

size_t vfs_get_mount_list(vfs_mount_info_t *buf, size_t max_count) {
    if (!buf || max_count == 0)
        return 0;
    spinlock_acquire(&g_vfs_lock);
    size_t count = 0;

    if (g_vfs_root && count < max_count) {
        strncpy(buf[count].path, "/", 127);
        strncpy(buf[count].name,
                (g_vfs_root->name[0] && strcmp(g_vfs_root->name, "/") != 0) ? g_vfs_root->name : "rootfs", 127);
        buf[count].flags = g_vfs_root->flags;
        count++;
    }

    for (size_t i = 0; i < g_mount_count && count < max_count; i++) {
        strncpy(buf[count].path, g_mounts[i].path, 127);
        if (g_mounts[i].node) {
            strncpy(buf[count].name, g_mounts[i].node->name, 127);
            buf[count].flags = g_mounts[i].node->flags;
        } else {
            strcpy(buf[count].name, "unknown");
        }
        count++;
    }

    spinlock_release(&g_vfs_lock);
    return count;
}

void vfs_normalize_path(const char *src, char *dst, size_t dst_size) {
    if (!src || !dst || dst_size == 0)
        return;

    char temp[256];
    strncpy(temp, src, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char *tokens[32];
    int token_count = 0;

    char *p = temp;
    while (*p == '/')
        p++;

    while (*p) {
        char *slash = strchr(p, '/');
        if (slash)
            *slash = '\0';

        if (strcmp(p, ".") == 0 || *p == '\0') {
            /* ignore current dir */
        } else if (strcmp(p, "..") == 0) {
            if (token_count > 0) {
                token_count--;
            }
        } else {
            if (token_count < 32) {
                tokens[token_count++] = p;
            }
        }

        if (slash)
            p = slash + 1;
        else
            break;
    }

    if (token_count == 0) {
        strncpy(dst, "/", dst_size - 1);
        dst[dst_size - 1] = '\0';
        return;
    }

    dst[0] = '\0';
    for (int i = 0; i < token_count; i++) {
        size_t cur_len = strlen(dst);
        ksnprintf(dst + cur_len, dst_size - cur_len, "/%s", tokens[i]);
    }
}

vfs_node_t *vfs_lookup(const char *path) {
    if (!path || !g_vfs_root)
        return NULL;

    char norm_path[256];
    vfs_normalize_path(path, norm_path, sizeof(norm_path));
    if (strcmp(norm_path, "/") == 0)
        return g_vfs_root;

    /* Check mount point prefix matching */
    vfs_node_t *current = g_vfs_root;
    const char *subpath = norm_path;

    for (size_t i = 0; i < g_mount_count; i++) {
        size_t mlen = strlen(g_mounts[i].path);
        if (strncmp(path, g_mounts[i].path, mlen) == 0) {
            if (path[mlen] == '/' || path[mlen] == '\0') {
                current = g_mounts[i].node;
                subpath = path + mlen;
                if (*subpath == '/')
                    subpath++;
                if (*subpath == '\0')
                    return current;
                break;
            }
        }
    }

    /* Normalize path and traverse */
    char temp[256];
    strncpy(temp, subpath, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char *token = temp;
    while (*token == '/')
        token++;

    char *next_slash;
    while (token && *token) {
        next_slash = strchr(token, '/');
        if (next_slash) {
            *next_slash = '\0';
        }

        if (strcmp(token, ".") == 0) {
            /* Current directory */
        } else if (strcmp(token, "..") == 0) {
            /* Parent directory */
        } else {
            if (!current->ops || !current->ops->finddir) {
                return NULL;
            }
            vfs_node_t *next = current->ops->finddir(current, token);
            if (!next) {
                return NULL;
            }
            if (next->ptr) {
                next = next->ptr;
            }
            current = next;
        }

        if (next_slash) {
            token = next_slash + 1;
        } else {
            break;
        }
    }

    return current;
}

int vfs_check_permission(vfs_node_t *node, int mask) {
    if (!node)
        return -1;

    process_t *curr = sched_get_current_process();
    if (!curr)
        return 0; /* Kernel has full access */

    /* Superuser (root) bypasses normal checks */
    if (curr->euid == 0) {
        /* If execute is requested on a file, at least one execute bit must be set */
        if ((mask & VFS_EXEC) && (node->flags != VFS_TYPE_DIRECTORY)) {
            if ((node->permissions & 0111) == 0) {
                return -1; /* Cannot execute file with no exec bits */
            }
        }
        return 0;
    }

    /* Owner check */
    if (curr->euid == node->uid) {
        uint32_t owner_perm = (node->permissions >> 6) & 7;
        if ((owner_perm & mask) == (uint32_t)mask) {
            return 0;
        }
        return -1;
    }

    /* Group check (primary and supplementary groups) */
    bool group_match = (curr->egid == node->gid);
    if (!group_match) {
        for (int i = 0; i < curr->ngroups; i++) {
            if (curr->groups[i] == node->gid) {
                group_match = true;
                break;
            }
        }
    }
    if (group_match) {
        uint32_t group_perm = (node->permissions >> 3) & 7;
        if ((group_perm & mask) == (uint32_t)mask) {
            return 0;
        }
        return -1;
    }

    /* Others check */
    uint32_t other_perm = node->permissions & 7;
    if ((other_perm & mask) == (uint32_t)mask) {
        return 0;
    }

    return -1;
}

int vfs_chmod(const char *path, mode_t mode) {
    vfs_node_t *node = vfs_lookup(path);
    if (!node)
        return -1;

    process_t *curr = sched_get_current_process();
    if (curr && curr->euid != 0 && curr->euid != node->uid) {
        return -1; /* EPERM */
    }

    node->permissions = (mode & 07777);
    if (node->ops && node->ops->chmod) {
        return node->ops->chmod(node, mode);
    }
    return 0;
}

int vfs_chown(const char *path, uid_t uid, gid_t gid) {
    vfs_node_t *node = vfs_lookup(path);
    if (!node)
        return -1;

    process_t *curr = sched_get_current_process();
    if (curr && curr->euid != 0) {
        return -1; /* Only root can change owner */
    }

    if (uid != (uid_t)-1) {
        node->uid = uid;
    }
    if (gid != (gid_t)-1) {
        node->gid = gid;
    }

    if (node->ops && node->ops->chown) {
        return node->ops->chown(node, uid, gid);
    }
    return 0;
}

int vfs_mkdir(const char *path, mode_t mode) {
    if (!path || !*path)
        return -1;

    /* Extract parent path and dir name */
    char parent_path[256];
    char dir_name[128];

    strncpy(parent_path, path, sizeof(parent_path) - 1);
    parent_path[sizeof(parent_path) - 1] = '\0';

    /* Strip trailing slashes */
    size_t plen = strlen(parent_path);
    while (plen > 1 && parent_path[plen - 1] == '/') {
        parent_path[--plen] = '\0';
    }

    char *last_slash = strrchr(parent_path, '/');
    if (!last_slash) {
        strcpy(dir_name, parent_path);
        strcpy(parent_path, ".");
    } else if (last_slash == parent_path) {
        strcpy(dir_name, last_slash + 1);
        strcpy(parent_path, "/");
    } else {
        strcpy(dir_name, last_slash + 1);
        *last_slash = '\0';
    }

    vfs_node_t *parent = vfs_lookup(parent_path);
    if (!parent || parent->flags != VFS_TYPE_DIRECTORY)
        return -1;

    if (vfs_check_permission(parent, VFS_WRITE | VFS_EXEC) != 0) {
        return -1; /* Permission denied in parent directory */
    }

    if (parent->ops && parent->ops->mkdir) {
        return parent->ops->mkdir(parent, dir_name, mode);
    }

    return -1;
}

int vfs_unlink(const char *path) {
    if (!path || !*path)
        return -1;

    char parent_path[256];
    char entry_name[128];

    strncpy(parent_path, path, sizeof(parent_path) - 1);
    parent_path[sizeof(parent_path) - 1] = '\0';

    /* Strip trailing slashes */
    size_t plen = strlen(parent_path);
    while (plen > 1 && parent_path[plen - 1] == '/') {
        parent_path[--plen] = '\0';
    }

    char *last_slash = strrchr(parent_path, '/');
    if (!last_slash) {
        strcpy(entry_name, parent_path);
        strcpy(parent_path, ".");
    } else if (last_slash == parent_path) {
        strcpy(entry_name, last_slash + 1);
        strcpy(parent_path, "/");
    } else {
        strcpy(entry_name, last_slash + 1);
        *last_slash = '\0';
    }

    vfs_node_t *parent = vfs_lookup(parent_path);
    if (!parent || parent->flags != VFS_TYPE_DIRECTORY)
        return -1;

    if (vfs_check_permission(parent, VFS_WRITE | VFS_EXEC) != 0) {
        return -1; /* Permission denied */
    }

    if (parent->ops && parent->ops->unlink) {
        return parent->ops->unlink(parent, entry_name);
    }

    return -1;
}
