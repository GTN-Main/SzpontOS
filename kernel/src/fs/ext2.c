#include <fs/ext2.h>
#include <fs/bcache.h>
#include <mm/heap.h>
#include <kernel/string.h>
#include <kernel/kprint.h>

typedef struct ext2_node_info {
    ext2_fs_t *fs;
    uint32_t ino;
    ext2_inode_t inode;
} ext2_node_info_t;

static vfs_ops_t g_ext2_file_ops;
static vfs_ops_t g_ext2_dir_ops;

static int ext2_read_inode(ext2_fs_t *fs, uint32_t ino, ext2_inode_t *inode_out) {
    if (!fs || ino == 0 || !inode_out) return -1;

    uint32_t group = (ino - 1) / fs->sb.s_inodes_per_group;
    uint32_t index = (ino - 1) % fs->sb.s_inodes_per_group;

    if (group >= fs->group_count) return -1;

    uint32_t block = fs->group_descs[group].bg_inode_table + (index * fs->inode_size) / fs->block_size;
    uint32_t offset = (index * fs->inode_size) % fs->block_size;

    buffer_t *buf = bread(fs->dev, block, fs->block_size);
    if (!buf) return -1;

    memcpy(inode_out, buf->data + offset, sizeof(ext2_inode_t));
    brelse(buf);
    return 0;
}

static int ext2_write_inode(ext2_fs_t *fs, uint32_t ino, const ext2_inode_t *inode_in) {
    if (!fs || ino == 0 || !inode_in) return -1;

    uint32_t group = (ino - 1) / fs->sb.s_inodes_per_group;
    uint32_t index = (ino - 1) % fs->sb.s_inodes_per_group;

    if (group >= fs->group_count) return -1;

    uint32_t block = fs->group_descs[group].bg_inode_table + (index * fs->inode_size) / fs->block_size;
    uint32_t offset = (index * fs->inode_size) % fs->block_size;

    buffer_t *buf = bread(fs->dev, block, fs->block_size);
    if (!buf) return -1;

    memcpy(buf->data + offset, inode_in, sizeof(ext2_inode_t));
    buf->dirty = true;
    bwrite(buf);
    brelse(buf);
    return 0;
}

static uint32_t ext2_get_inode_block(ext2_fs_t *fs, ext2_inode_t *inode, uint32_t block_idx) {
    uint32_t ptrs_per_block = fs->block_size / sizeof(uint32_t);

    /* 1. Direct Blocks (0..11) */
    if (block_idx < 12) {
        return inode->i_block[block_idx];
    }
    block_idx -= 12;

    /* 2. Singly Indirect Block (12) */
    if (block_idx < ptrs_per_block) {
        if (inode->i_block[12] == 0) return 0;
        buffer_t *buf = bread(fs->dev, inode->i_block[12], fs->block_size);
        if (!buf) return 0;
        uint32_t ptr = ((uint32_t *)buf->data)[block_idx];
        brelse(buf);
        return ptr;
    }
    block_idx -= ptrs_per_block;

    /* 3. Doubly Indirect Block (13) */
    uint32_t doubly_cap = ptrs_per_block * ptrs_per_block;
    if (block_idx < doubly_cap) {
        if (inode->i_block[13] == 0) return 0;
        buffer_t *buf1 = bread(fs->dev, inode->i_block[13], fs->block_size);
        if (!buf1) return 0;

        uint32_t idx1 = block_idx / ptrs_per_block;
        uint32_t idx2 = block_idx % ptrs_per_block;
        uint32_t ptr1 = ((uint32_t *)buf1->data)[idx1];
        brelse(buf1);

        if (ptr1 == 0) return 0;
        buffer_t *buf2 = bread(fs->dev, ptr1, fs->block_size);
        if (!buf2) return 0;
        uint32_t ptr2 = ((uint32_t *)buf2->data)[idx2];
        brelse(buf2);
        return ptr2;
    }

    return 0;
}

static ssize_t ext2_read(vfs_node_t *node, off_t offset, size_t size, void *buffer) {
    ext2_node_info_t *info = (ext2_node_info_t *)node->device_data;
    if (!info || !buffer) return -1;

    ext2_fs_t *fs = info->fs;
    if (offset >= (off_t)node->length) return 0;
    if (offset + size > node->length) {
        size = node->length - offset;
    }

    size_t bytes_read = 0;
    uint8_t *dest = (uint8_t *)buffer;

    while (bytes_read < size) {
        uint32_t block_idx = (offset + bytes_read) / fs->block_size;
        uint32_t block_offset = (offset + bytes_read) % fs->block_size;
        size_t to_read = fs->block_size - block_offset;
        if (to_read > size - bytes_read) {
            to_read = size - bytes_read;
        }

        uint32_t disk_block = ext2_get_inode_block(fs, &info->inode, block_idx);
        if (disk_block == 0) {
            /* Sparse block (hole) */
            memset(dest + bytes_read, 0, to_read);
        } else {
            buffer_t *buf = bread(fs->dev, disk_block, fs->block_size);
            if (!buf) break;
            memcpy(dest + bytes_read, buf->data + block_offset, to_read);
            brelse(buf);
        }

        bytes_read += to_read;
    }

    return (ssize_t)bytes_read;
}

static struct vfs_dirent *ext2_readdir(vfs_node_t *node, uint32_t index) {
    ext2_node_info_t *info = (ext2_node_info_t *)node->device_data;
    if (!info) return NULL;

    ext2_fs_t *fs = info->fs;
    static vfs_dirent_t dirent;
    uint32_t current_idx = 0;
    off_t offset = 0;

    while (offset < (off_t)node->length) {
        uint32_t block_idx = offset / fs->block_size;
        uint32_t block_offset = offset % fs->block_size;
        uint32_t disk_block = ext2_get_inode_block(fs, &info->inode, block_idx);

        if (disk_block == 0) {
            offset += fs->block_size - block_offset;
            continue;
        }

        buffer_t *buf = bread(fs->dev, disk_block, fs->block_size);
        if (!buf) return NULL;

        while (block_offset < fs->block_size && offset < (off_t)node->length) {
            ext2_dir_entry_t *entry = (ext2_dir_entry_t *)(buf->data + block_offset);
            if (entry->rec_len == 0) {
                brelse(buf);
                return NULL;
            }

            if (entry->inode != 0) {
                if (current_idx == index) {
                    memset(&dirent, 0, sizeof(dirent));
                    size_t name_len = (entry->name_len < sizeof(dirent.name) - 1) ?
                                      entry->name_len : sizeof(dirent.name) - 1;
                    memcpy(dirent.name, entry->name, name_len);
                    dirent.name[name_len] = '\0';
                    dirent.inode = entry->inode;
                    dirent.type = (entry->file_type == EXT2_FT_DIR) ? VFS_TYPE_DIRECTORY : VFS_TYPE_FILE;
                    brelse(buf);
                    return &dirent;
                }
                current_idx++;
            }

            block_offset += entry->rec_len;
            offset += entry->rec_len;
        }

        brelse(buf);
    }

    return NULL;
}

static vfs_node_t *ext2_create_vfs_node(ext2_fs_t *fs, uint32_t ino, const char *name) {
    ext2_inode_t inode;
    if (ext2_read_inode(fs, ino, &inode) != 0) return NULL;

    vfs_node_t *v_node = (vfs_node_t *)kzalloc(sizeof(vfs_node_t));
    ext2_node_info_t *info = (ext2_node_info_t *)kzalloc(sizeof(ext2_node_info_t));

    info->fs = fs;
    info->ino = ino;
    memcpy(&info->inode, &inode, sizeof(ext2_inode_t));

    strncpy(v_node->name, name, sizeof(v_node->name) - 1);
    v_node->inode = ino;
    v_node->length = inode.i_size;
    v_node->uid = inode.i_uid;
    v_node->gid = inode.i_gid;
    v_node->permissions = inode.i_mode & 07777;
    v_node->device_data = info;

    if ((inode.i_mode & EXT2_S_IFMT) == EXT2_S_IFDIR) {
        v_node->flags = VFS_TYPE_DIRECTORY;
        v_node->ops = &g_ext2_dir_ops;
    } else {
        v_node->flags = VFS_TYPE_FILE;
        v_node->ops = &g_ext2_file_ops;
    }

    return v_node;
}

static vfs_node_t *ext2_finddir(vfs_node_t *node, const char *name) {
    ext2_node_info_t *info = (ext2_node_info_t *)node->device_data;
    if (!info || !name) return NULL;

    ext2_fs_t *fs = info->fs;
    off_t offset = 0;
    size_t search_len = strlen(name);

    while (offset < (off_t)node->length) {
        uint32_t block_idx = offset / fs->block_size;
        uint32_t block_offset = offset % fs->block_size;
        uint32_t disk_block = ext2_get_inode_block(fs, &info->inode, block_idx);

        if (disk_block == 0) {
            offset += fs->block_size - block_offset;
            continue;
        }

        buffer_t *buf = bread(fs->dev, disk_block, fs->block_size);
        if (!buf) return NULL;

        while (block_offset < fs->block_size && offset < (off_t)node->length) {
            ext2_dir_entry_t *entry = (ext2_dir_entry_t *)(buf->data + block_offset);
            if (entry->rec_len == 0) {
                brelse(buf);
                return NULL;
            }

            if (entry->inode != 0 && entry->name_len == search_len &&
                strncmp(entry->name, name, search_len) == 0) {
                uint32_t child_ino = entry->inode;
                brelse(buf);
                return ext2_create_vfs_node(fs, child_ino, name);
            }

            block_offset += entry->rec_len;
            offset += entry->rec_len;
        }

        brelse(buf);
    }

    return NULL;
}

static int ext2_chmod(vfs_node_t *node, mode_t mode) {
    ext2_node_info_t *info = (ext2_node_info_t *)node->device_data;
    if (!info) return -1;

    info->inode.i_mode = (info->inode.i_mode & ~07777) | (mode & 07777);
    node->permissions = mode & 07777;
    return ext2_write_inode(info->fs, info->ino, &info->inode);
}

static int ext2_chown(vfs_node_t *node, uid_t uid, gid_t gid) {
    ext2_node_info_t *info = (ext2_node_info_t *)node->device_data;
    if (!info) return -1;

    if (uid != (uid_t)-1) info->inode.i_uid = (uint16_t)uid;
    if (gid != (gid_t)-1) info->inode.i_gid = (uint16_t)gid;
    node->uid = uid;
    node->gid = gid;
    return ext2_write_inode(info->fs, info->ino, &info->inode);
}

vfs_node_t *ext2_mount(block_device_t *dev) {
    if (!dev) return NULL;

    /* 1. Read Superblock at offset 1024 bytes (LBA 2) */
    uint8_t sb_buf[1024];
    if (dev->read_blocks(dev, 2, 2, sb_buf) != 0) {
        return NULL;
    }

    ext2_superblock_t *sb = (ext2_superblock_t *)sb_buf;
    if (sb->s_magic != EXT2_MAGIC) {
        return NULL; /* Not an ext2 filesystem */
    }

    ext2_fs_t *fs = (ext2_fs_t *)kzalloc(sizeof(ext2_fs_t));
    fs->dev = dev;
    memcpy(&fs->sb, sb, sizeof(ext2_superblock_t));

    fs->block_size = 1024 << sb->s_log_block_size;
    fs->inode_size = (sb->s_rev_level >= 1) ? sb->s_inode_size : 128;
    fs->group_count = (sb->s_blocks_count + sb->s_blocks_per_group - 1) / sb->s_blocks_per_group;

    klog_info("ext2: Valid filesystem found on '%s'", dev->name);
    klog_info("ext2: Block size: %lu bytes, Inodes: %u, Blocks: %u, Groups: %lu",
              fs->block_size, sb->s_inodes_count, sb->s_blocks_count, fs->group_count);

    /* 2. Read Block Group Descriptors */
    size_t bg_table_size = fs->group_count * sizeof(ext2_group_desc_t);
    fs->group_descs = (ext2_group_desc_t *)kmalloc(bg_table_size);

    uint32_t bg_block = (fs->block_size == 1024) ? 2 : 1;
    size_t bg_blocks_needed = (bg_table_size + fs->block_size - 1) / fs->block_size;

    for (size_t i = 0; i < bg_blocks_needed; i++) {
        buffer_t *buf = bread(dev, bg_block + i, fs->block_size);
        if (!buf) {
            kfree(fs->group_descs);
            kfree(fs);
            return NULL;
        }
        size_t copy_len = bg_table_size - i * fs->block_size;
        if (copy_len > fs->block_size) copy_len = fs->block_size;
        memcpy((uint8_t *)fs->group_descs + i * fs->block_size, buf->data, copy_len);
        brelse(buf);
    }

    /* 3. Setup File Operations */
    g_ext2_file_ops.read = ext2_read;
    g_ext2_file_ops.write = NULL;
    g_ext2_file_ops.open = NULL;
    g_ext2_file_ops.close = NULL;
    g_ext2_file_ops.readdir = NULL;
    g_ext2_file_ops.finddir = NULL;
    g_ext2_file_ops.chmod = ext2_chmod;
    g_ext2_file_ops.chown = ext2_chown;

    g_ext2_dir_ops.read = NULL;
    g_ext2_dir_ops.write = NULL;
    g_ext2_dir_ops.open = NULL;
    g_ext2_dir_ops.close = NULL;
    g_ext2_dir_ops.readdir = ext2_readdir;
    g_ext2_dir_ops.finddir = ext2_finddir;
    g_ext2_dir_ops.chmod = ext2_chmod;
    g_ext2_dir_ops.chown = ext2_chown;

    /* 4. Create Root VFS Node (Root Inode is always 2) */
    vfs_node_t *root_vfs = ext2_create_vfs_node(fs, EXT2_ROOT_INO, "ext2_root");
    if (!root_vfs) {
        kfree(fs->group_descs);
        kfree(fs);
        return NULL;
    }

    return root_vfs;
}
