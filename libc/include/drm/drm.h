#ifndef _LIBC_DRM_DRM_H
#define _LIBC_DRM_DRM_H

#include <stdint.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#define DRM_COMMAND_BASE 0x40
#define DRM_COMMAND_END  0xA0
#define DRM_IOCTL_BASE   'd'

#define DRM_IO(nr)       _IO(DRM_IOCTL_BASE, nr)
#define DRM_IOR(nr,type) _IOR(DRM_IOCTL_BASE, nr, type)
#define DRM_IOW(nr,type) _IOW(DRM_IOCTL_BASE, nr, type)
#define DRM_IOWR(nr,type) _IOWR(DRM_IOCTL_BASE, nr, type)

typedef uint32_t __u32;
typedef uint64_t __u64;
typedef uint16_t __u16;
typedef uint8_t  __u8;
typedef int32_t  __s32;
typedef int64_t  __s64;

struct drm_version {
    int version_major;
    int version_minor;
    int version_patchlevel;
    size_t name_len;
    char *name;
    size_t date_len;
    char *date;
    size_t desc_len;
    char *desc;
};

struct drm_unique {
    size_t unique_len;
    char *unique;
};

struct drm_auth {
    uint32_t magic;
};

#define DRM_CAP_DUMB_BUFFER             0x1
#define DRM_CAP_VBLANK_HIGH_CRTC        0x2
#define DRM_CAP_DUMB_PREFERRED_DEPTH    0x3
#define DRM_CAP_DUMB_PREFER_SHADOW      0x4
#define DRM_CAP_PRIME                   0x5
#define DRM_CAP_TIMESTAMP_MONOTONIC     0x6
#define DRM_CAP_ASYNC_PAGE_FLIP         0x7
#define DRM_CAP_CURSOR_WIDTH            0x8
#define DRM_CAP_CURSOR_HEIGHT           0x9
#define DRM_CAP_ADDFB2_MODIFIERS        0x10
typedef enum {
    DRM_VBLANK_ABSOLUTE    = 0x00000000,
    DRM_VBLANK_RELATIVE    = 0x00000001,
    DRM_VBLANK_EVENT       = 0x04000000,
    DRM_VBLANK_FLIP        = 0x08000000,
    DRM_VBLANK_NEXTONMISS  = 0x10000000,
    DRM_VBLANK_SECONDARY   = 0x20000000,
    DRM_VBLANK_SIGNAL      = 0x40000000
} drmVBlankSeqType;

#define DRM_VBLANK_HIGH_CRTC_MASK       0x0000001e
#define DRM_VBLANK_HIGH_CRTC_SHIFT      1

#define DRM_CRTC_SEQUENCE_RELATIVE      0x00000001
#define DRM_CRTC_SEQUENCE_NEXT_ON_MISS  0x00000002

struct drm_wait_vblank_request {
    drmVBlankSeqType type;
    unsigned int sequence;
    unsigned long signal;
};

struct drm_wait_vblank_reply {
    drmVBlankSeqType type;
    unsigned int sequence;
    long tval_sec;
    long tval_usec;
};

typedef union drm_wait_vblank {
    struct drm_wait_vblank_request request;
    struct drm_wait_vblank_reply reply;
} drmVBlank, *drmVBlankPtr;

#define DRM_IOCTL_WAIT_VBLANK           DRM_IOWR(0x3a, union drm_wait_vblank)

struct drm_get_cap {
    uint64_t capability;
    uint64_t value;
};

struct drm_set_client_cap {
    uint64_t capability;
    uint64_t value;
};

#define DRM_IOCTL_VERSION               DRM_IOWR(0x00, struct drm_version)
#define DRM_IOCTL_GET_UNIQUE            DRM_IOWR(0x01, struct drm_unique)
#define DRM_IOCTL_GET_MAGIC             DRM_IOWR(0x02, struct drm_auth)
#define DRM_IOCTL_SET_VERSION           DRM_IOWR(0x07, struct drm_set_version)
#define DRM_IOCTL_GET_CAP               DRM_IOWR(0x0c, struct drm_get_cap)
#define DRM_IOCTL_SET_CLIENT_CAP        DRM_IOW(0x0d, struct drm_set_client_cap)
#define DRM_IOCTL_SET_MASTER            DRM_IO(0x1e)
#define DRM_IOCTL_DROP_MASTER           DRM_IO(0x1f)
#define DRM_IOCTL_AUTH_MAGIC            DRM_IOW(0x11, struct drm_auth)

#define DRM_PRIME_CAP_IMPORT 0x1
#define DRM_PRIME_CAP_EXPORT 0x2

#define DRM_CLIENT_CAP_STEREO_3D            1
#define DRM_CLIENT_CAP_UNIVERSAL_PLANES     2
#define DRM_CLIENT_CAP_ATOMIC               3
#define DRM_CLIENT_CAP_ASPECT_RATIO         4
#define DRM_CLIENT_CAP_WRITEBACK_CONNECTORS 5

typedef struct drm_set_version {
    int drm_di_major;
    int drm_di_minor;
    int drm_dd_major;
    int drm_dd_minor;
} drmSetVersion, *drmSetVersionPtr;

struct drm_prime_handle {
    uint32_t handle;
    uint32_t flags;
    int32_t fd;
};

#define DRM_IOCTL_PRIME_HANDLE_TO_FD DRM_IOWR(0x2d, struct drm_prime_handle)
#define DRM_IOCTL_PRIME_FD_TO_HANDLE DRM_IOWR(0x2e, struct drm_prime_handle)

#include <drm/drm_mode.h>

#endif /* _LIBC_DRM_DRM_H */
