#ifndef _DRM_H
#define _DRM_H
#include <stdint.h>
#include <sys/ioctl.h>
#define DRM_COMMAND_BASE 0x40
#define DRM_COMMAND_END  0xA0
#define DRM_IOCTL_BASE   'd'
#define DRM_IO(nr)       _IO(DRM_IOCTL_BASE,nr)
#define DRM_IOR(nr,type) _IOR(DRM_IOCTL_BASE,nr,type)
#define DRM_IOW(nr,type) _IOW(DRM_IOCTL_BASE,nr,type)
#define DRM_IOWR(nr,type) _IOWR(DRM_IOCTL_BASE,nr,type)
typedef uint32_t __u32;
typedef uint64_t __u64;
typedef uint16_t __u16;
typedef uint8_t  __u8;
typedef int32_t  __s32;
typedef int64_t  __s64;
#endif
