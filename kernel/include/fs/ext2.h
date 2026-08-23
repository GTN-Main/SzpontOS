#ifndef SZPONTOS_FS_EXT2_H
#define SZPONTOS_FS_EXT2_H

#include <kernel/types.h>
#include <drivers/block.h>
#include <fs/vfs.h>

#define EXT2_MAGIC            0xEF53
#define EXT2_ROOT_INO         2
#define EXT2_NAME_LEN         255

/* File type flags in dir_entry */
#define EXT2_FT_UNKNOWN       0
#define EXT2_FT_REG_FILE      1
#define EXT2_FT_DIR           2
#define EXT2_FT_CHRDEV        3
#define EXT2_FT_BLKDEV        4
#define EXT2_FT_FIFO          5
#define EXT2_FT_SOCK          6
#define EXT2_FT_SYMLINK       7

/* Inode mode flags */
#define EXT2_S_IFMT           0xF000
#define EXT2_S_IFSOCK         0xC000
#define EXT2_S_IFLNK          0xA000
#define EXT2_S_IFREG          0x8000
#define EXT2_S_IFBLK          0x6000
#define EXT2_S_IFDIR          0x4000
#define EXT2_S_IFCHR          0x2000
#define EXT2_S_IFIFO          0x1000

typedef struct __attribute__((packed)) ext2_superblock {
    uint32_t s_inodes_count;
    uint32_t s_blocks_count;
    uint32_t s_r_blocks_count;
    uint32_t s_free_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;      /* 1 for 1KB, 0 for >1KB */
    uint32_t s_log_block_size;        /* Block size = 1024 << s_log_block_size */
    uint32_t s_log_frag_size;
    uint32_t s_blocks_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;                 /* 0xEF53 */
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;
    /* Extended fields (for rev >= 1) */
    uint32_t s_first_ino;
    uint16_t s_inode_size;            /* Typically 128 or 256 */
    uint16_t s_block_group_nr;
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t  s_uuid[16];
    char     s_volume_name[16];
    char     s_last_mounted[64];
    uint32_t s_algo_bitmap;
    uint8_t  s_prealloc_blocks;
    uint8_t  s_prealloc_dir_blocks;
    uint16_t s_padding1;
    uint8_t  s_journal_uuid[16];
    uint32_t s_journal_inum;
    uint32_t s_journal_dev;
    uint32_t s_last_orphan;
    uint32_t s_hash_seed[4];
    uint8_t  s_def_hash_version;
    uint8_t  s_reserved_char_pad;
    uint16_t s_reserved_word_pad;
    uint32_t s_default_mount_opts;
    uint32_t s_first_meta_bg;
    uint8_t  s_reserved[760];
} ext2_superblock_t;

typedef struct __attribute__((packed)) ext2_group_desc {
    uint32_t bg_block_bitmap;         /* Block address of block usage bitmap */
    uint32_t bg_inode_bitmap;         /* Block address of inode usage bitmap */
    uint32_t bg_inode_table;          /* Starting block address of inode table */
    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_used_dirs_count;
    uint16_t bg_pad;
    uint8_t  bg_reserved[12];
} ext2_group_desc_t;

typedef struct __attribute__((packed)) ext2_inode {
    uint16_t i_mode;                  /* Format and permissions */
    uint16_t i_uid;                   /* User ID */
    uint32_t i_size;                  /* File size in bytes */
    uint32_t i_atime;                 /* Access time */
    uint32_t i_ctime;                 /* Creation time */
    uint32_t i_mtime;                 /* Modification time */
    uint32_t i_dtime;                 /* Deletion time */
    uint16_t i_gid;                   /* Group ID */
    uint16_t i_links_count;           /* Hard links count */
    uint32_t i_blocks;                /* 512-byte blocks count */
    uint32_t i_flags;
    uint32_t i_osd1;
    uint32_t i_block[15];             /* 0..11 Direct, 12 Singly, 13 Doubly, 14 Triply */
    uint32_t i_generation;
    uint32_t i_file_acl;
    uint32_t i_dir_acl;
    uint32_t i_faddr;
    uint8_t  i_osd2[12];
} ext2_inode_t;

typedef struct __attribute__((packed)) ext2_dir_entry {
    uint32_t inode;                   /* Inode number */
    uint16_t rec_len;                 /* Directory entry length */
    uint8_t  name_len;                /* Name length */
    uint8_t  file_type;               /* File type code */
    char     name[EXT2_NAME_LEN];     /* Name (not null-terminated on disk) */
} ext2_dir_entry_t;

typedef struct ext2_fs {
    block_device_t *dev;
    ext2_superblock_t sb;
    size_t block_size;
    size_t group_count;
    size_t inode_size;
    ext2_group_desc_t *group_descs;
    vfs_node_t root_node;
} ext2_fs_t;

vfs_node_t *ext2_mount(block_device_t *dev);

#endif /* SZPONTOS_FS_EXT2_H */
