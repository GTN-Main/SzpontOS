#include <fs/initramfs.h>
#include <sched/process.h>
#include <sched/sched.h>
#include <mm/heap.h>
#include <kernel/string.h>
#include <kernel/kprint.h>

typedef struct initramfs_entry {
    vfs_node_t node;
    void *data;
    size_t capacity;
    bool is_dynamic_data;
    struct initramfs_entry *children[128];
    size_t child_count;
} initramfs_entry_t;

static vfs_ops_t g_initramfs_file_ops;
static vfs_ops_t g_initramfs_dir_ops;

static size_t parse_octal(const char *str, size_t max_len) {
    size_t result = 0;
    for (size_t i = 0; i < max_len && str[i] >= '0' && str[i] <= '7'; i++) {
        result = (result << 3) | (str[i] - '0');
    }
    return result;
}

static ssize_t initramfs_read(vfs_node_t *node, off_t offset, size_t size, void *buffer) {
    initramfs_entry_t *entry = (initramfs_entry_t *)node;
    if (!entry || !entry->data || !buffer) return 0;

    if (offset >= (off_t)node->length) return 0;
    if (offset + size > node->length) {
        size = node->length - offset;
    }

    memcpy(buffer, (const char *)entry->data + offset, size);
    return (ssize_t)size;
}

static ssize_t initramfs_write(vfs_node_t *node, off_t offset, size_t size, const void *buffer) {
    initramfs_entry_t *entry = (initramfs_entry_t *)node;
    if (!entry || !buffer) return -1;

    size_t required = offset + size;
    if (!entry->is_dynamic_data || required > entry->capacity) {
        size_t new_cap = (required > entry->capacity * 2) ? required + 512 : (entry->capacity * 2 + 512);
        void *new_data = kmalloc(new_cap);
        if (!new_data) return -1;

        if (entry->data && node->length > 0) {
            memcpy(new_data, entry->data, node->length);
        }
        if (entry->is_dynamic_data && entry->data) {
            kfree(entry->data);
        }

        entry->data = new_data;
        entry->capacity = new_cap;
        entry->is_dynamic_data = true;
    }

    memcpy((char *)entry->data + offset, buffer, size);
    if (offset + size > node->length) {
        node->length = offset + size;
    }

    return (ssize_t)size;
}

static struct vfs_dirent *initramfs_readdir(vfs_node_t *node, uint32_t index) {
    initramfs_entry_t *entry = (initramfs_entry_t *)node;
    if (!entry || index >= entry->child_count) return NULL;

    static vfs_dirent_t dirent;
    memset(&dirent, 0, sizeof(dirent));
    strncpy(dirent.name, entry->children[index]->node.name, sizeof(dirent.name) - 1);
    dirent.inode = entry->children[index]->node.inode;
    dirent.type = entry->children[index]->node.flags;

    return &dirent;
}

static vfs_node_t *initramfs_finddir(vfs_node_t *node, const char *name) {
    initramfs_entry_t *entry = (initramfs_entry_t *)node;
    if (!entry || !name) return NULL;

    for (size_t i = 0; i < entry->child_count; i++) {
        if (strcmp(entry->children[i]->node.name, name) == 0) {
            return &entry->children[i]->node;
        }
    }
    return NULL;
}

static initramfs_entry_t *create_entry(const char *name, uint32_t flags, size_t size, void *data, mode_t mode, uid_t uid, gid_t gid) {
    initramfs_entry_t *entry = (initramfs_entry_t *)kzalloc(sizeof(initramfs_entry_t));
    strncpy(entry->node.name, name, sizeof(entry->node.name) - 1);
    entry->node.flags = flags;
    entry->node.length = size;
    entry->node.permissions = mode ? mode : ((flags == VFS_TYPE_DIRECTORY) ? 0755 : 0644);
    entry->node.uid = uid;
    entry->node.gid = gid;
    entry->node.ops = (flags == VFS_TYPE_DIRECTORY) ? &g_initramfs_dir_ops : &g_initramfs_file_ops;
    entry->data = data;
    entry->capacity = size;
    entry->is_dynamic_data = false;
    return entry;
}

static int initramfs_create(vfs_node_t *parent, const char *name, mode_t mode) {
    initramfs_entry_t *p = (initramfs_entry_t *)parent;
    if (!p || p->node.flags != VFS_TYPE_DIRECTORY || p->child_count >= 128) return -1;

    /* Check if already exists */
    for (size_t i = 0; i < p->child_count; i++) {
        if (strcmp(p->children[i]->node.name, name) == 0) {
            return 0; /* Already exists */
        }
    }

    process_t *curr = sched_get_current_process();
    uid_t uid = curr ? curr->euid : 0;
    gid_t gid = curr ? curr->egid : 0;

    initramfs_entry_t *child = create_entry(name, VFS_TYPE_FILE, 0, NULL, mode ? mode : 0644, uid, gid);
    child->is_dynamic_data = true;
    p->children[p->child_count++] = child;

    return 0;
}

static int initramfs_mkdir(vfs_node_t *parent, const char *name, mode_t mode) {
    initramfs_entry_t *p = (initramfs_entry_t *)parent;
    if (!p || p->node.flags != VFS_TYPE_DIRECTORY || p->child_count >= 128) return -1;

    /* Check if already exists */
    for (size_t i = 0; i < p->child_count; i++) {
        if (strcmp(p->children[i]->node.name, name) == 0) {
            return 0; /* Already exists */
        }
    }

    process_t *curr = sched_get_current_process();
    uid_t uid = curr ? curr->euid : 0;
    gid_t gid = curr ? curr->egid : 0;

    initramfs_entry_t *child = create_entry(name, VFS_TYPE_DIRECTORY, 0, NULL, mode ? mode : 0755, uid, gid);
    p->children[p->child_count++] = child;

    return 0;
}

static int initramfs_chmod(vfs_node_t *node, mode_t mode) {
    if (!node) return -1;
    node->permissions = (mode & 07777);
    return 0;
}

static int initramfs_chown(vfs_node_t *node, uid_t uid, gid_t gid) {
    if (!node) return -1;
    if (uid != (uid_t)-1) node->uid = uid;
    if (gid != (gid_t)-1) node->gid = gid;
    return 0;
}

static int initramfs_unlink(vfs_node_t *parent, const char *name) {
    initramfs_entry_t *p = (initramfs_entry_t *)parent;
    if (!p || p->node.flags != VFS_TYPE_DIRECTORY || !name) return -1;

    for (size_t i = 0; i < p->child_count; i++) {
        if (strcmp(p->children[i]->node.name, name) == 0) {
            initramfs_entry_t *target = p->children[i];

            /* If it's a directory, ensure it is empty */
            if (target->node.flags == VFS_TYPE_DIRECTORY && target->child_count > 0) {
                return -1; /* Directory not empty */
            }

            if (target->is_dynamic_data && target->data) {
                kfree(target->data);
            }

            /* Shift remaining children */
            for (size_t j = i; j + 1 < p->child_count; j++) {
                p->children[j] = p->children[j + 1];
            }
            p->child_count--;
            kfree(target);
            return 0;
        }
    }

    return -1; /* No such file or directory */
}

static initramfs_entry_t *add_path_to_tree(initramfs_entry_t *root, const char *path, uint32_t flags, size_t size, void *data, mode_t mode, uid_t uid, gid_t gid) {
    char clean_path[256];
    strncpy(clean_path, path, sizeof(clean_path) - 1);
    clean_path[sizeof(clean_path) - 1] = '\0';

    /* Strip leading slashes */
    char *p = clean_path;
    while (*p == '/') p++;

    /* Strip trailing slashes */
    size_t len = strlen(p);
    while (len > 0 && p[len - 1] == '/') {
        p[--len] = '\0';
    }

    if (len == 0) return root;

    initramfs_entry_t *curr = root;
    char *token = p;
    char *slash;

    while (token && *token) {
        slash = strchr(token, '/');
        if (slash) {
            *slash = '\0';
        }

        /* Check if child exists */
        initramfs_entry_t *next = NULL;
        for (size_t i = 0; i < curr->child_count; i++) {
            if (strcmp(curr->children[i]->node.name, token) == 0) {
                next = curr->children[i];
                break;
            }
        }

        if (!next) {
            uint32_t entry_flags = (slash != NULL) ? VFS_TYPE_DIRECTORY : flags;
            size_t entry_size = (slash != NULL) ? 0 : size;
            void *entry_data = (slash != NULL) ? NULL : data;
            mode_t entry_mode = (slash != NULL) ? 0755 : mode;

            next = create_entry(token, entry_flags, entry_size, entry_data, entry_mode, uid, gid);
            if (curr->child_count < 128) {
                curr->children[curr->child_count++] = next;
            }
        }

        curr = next;
        if (slash) {
            token = slash + 1;
        } else {
            break;
        }
    }

    return curr;
}

vfs_node_t *initramfs_init(void *archive_ptr, size_t archive_size) {
    if (!archive_ptr || archive_size < 512) return NULL;

    g_initramfs_file_ops.read = initramfs_read;
    g_initramfs_file_ops.write = initramfs_write;
    g_initramfs_file_ops.open = NULL;
    g_initramfs_file_ops.close = NULL;
    g_initramfs_file_ops.readdir = NULL;
    g_initramfs_file_ops.finddir = NULL;
    g_initramfs_file_ops.create = NULL;
    g_initramfs_file_ops.mkdir = NULL;
    g_initramfs_file_ops.chmod = initramfs_chmod;
    g_initramfs_file_ops.chown = initramfs_chown;

    g_initramfs_dir_ops.read = NULL;
    g_initramfs_dir_ops.write = NULL;
    g_initramfs_dir_ops.open = NULL;
    g_initramfs_dir_ops.close = NULL;
    g_initramfs_dir_ops.readdir = initramfs_readdir;
    g_initramfs_dir_ops.finddir = initramfs_finddir;
    g_initramfs_dir_ops.create = initramfs_create;
    g_initramfs_dir_ops.mkdir = initramfs_mkdir;
    g_initramfs_dir_ops.chmod = initramfs_chmod;
    g_initramfs_dir_ops.chown = initramfs_chown;
    g_initramfs_dir_ops.unlink = initramfs_unlink;

    initramfs_entry_t *root = create_entry("/", VFS_TYPE_DIRECTORY, 0, NULL, 0755, 0, 0);

    uint8_t *ptr = (uint8_t *)archive_ptr;
    uint8_t *end = ptr + archive_size;
    size_t file_count = 0;

    while (ptr + 512 <= end) {
        ustar_header_t *hdr = (ustar_header_t *)ptr;
        if (hdr->name[0] == '\0') {
            break; /* Zero block indicates end of archive */
        }

        size_t file_size = parse_octal(hdr->size, sizeof(hdr->size));
        mode_t mode = (mode_t)parse_octal(hdr->mode, sizeof(hdr->mode));
        uid_t uid = (uid_t)parse_octal(hdr->uid, sizeof(hdr->uid));
        gid_t gid = (gid_t)parse_octal(hdr->gid, sizeof(hdr->gid));
        uint32_t flags = (hdr->typeflag == '5') ? VFS_TYPE_DIRECTORY : VFS_TYPE_FILE;
        void *file_data = (void *)(ptr + 512);

        add_path_to_tree(root, hdr->name, flags, file_size, file_data, mode, uid, gid);
        file_count++;

        /* Move to next block: header (512) + aligned data blocks */
        size_t block_count = DIV_ROUND_UP(file_size, 512);
        ptr += 512 + block_count * 512;
    }

    klog_info("Initramfs parsed: %lu files/directories unpacked into VFS", file_count);
    return &root->node;
}
