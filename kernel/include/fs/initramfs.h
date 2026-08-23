#ifndef SZPONTOS_FS_INITRAMFS_H
#define SZPONTOS_FS_INITRAMFS_H

#include <fs/vfs.h>

struct __attribute__((packed)) ustar_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
};

typedef struct ustar_header ustar_header_t;

vfs_node_t *initramfs_init(void *archive_ptr, size_t archive_size);

#endif /* SZPONTOS_FS_INITRAMFS_H */
