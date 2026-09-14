/*
 * SzpontOS - Native DRM/KMS Userland API Header (xf86drm.h)
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef _XF86DRM_H
#define _XF86DRM_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <drm/drm.h>

#ifndef DRM_CLOEXEC
#define DRM_CLOEXEC O_CLOEXEC
#endif
#ifndef DRM_RDWR
#define DRM_RDWR O_RDWR
#endif

#define DRM_DIR_NAME          "/dev/dri"
#define DRM_DEV_NAME          "%s/card%d"
#define DRM_CONTROL_DEV_NAME  "%s/controlD%d"
#define DRM_RENDER_DEV_NAME   "%s/renderD%d"
#define DRM_PROC_NAME         "/proc/dri"

#define DRM_NODE_NAME_CARD    "card"
#define DRM_NODE_NAME_RENDER  "renderD"
#define DRM_NODE_NAME_CONTROL "controlD"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t drm_magic_t;

typedef struct _drmEventContext {
    int version;
    void (*vblank_handler)(int fd, unsigned int sequence, unsigned int tv_sec,
                           unsigned int tv_usec, void *user_data);
    void (*page_flip_handler)(int fd, unsigned int sequence, unsigned int tv_sec,
                              unsigned int tv_usec, void *user_data);
    void (*page_flip_handler2)(int fd, unsigned int sequence, unsigned int tv_sec,
                               unsigned int tv_usec, unsigned int crtc_id, void *user_data);
    void (*sequence_handler)(int fd, uint64_t sequence, uint64_t ns, uint64_t user_data);
} drmEventContext, *drmEventContextPtr;

typedef struct _drmVersion {
    int version_major;
    int version_minor;
    int version_patchlevel;
    int name_len;
    char *name;
    int date_len;
    char *date;
    int desc_len;
    char *desc;
} drmVersion, *drmVersionPtr;

enum {
    DRM_NODE_PRIMARY = 0,
    DRM_NODE_CONTROL = 1,
    DRM_NODE_RENDER  = 2,
    DRM_NODE_MAX     = 3,
};

#define DRM_BUS_PCI      0
#define DRM_BUS_USB      1
#define DRM_BUS_PLATFORM 2
#define DRM_BUS_HOST1X   3

typedef struct _drmPciBusInfo {
    uint16_t domain;
    uint8_t bus;
    uint8_t dev;
    uint8_t func;
} drmPciBusInfo, *drmPciBusInfoPtr;

typedef struct _drmPciDeviceInfo {
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t subvendor_id;
    uint16_t subdevice_id;
    uint8_t revision;
} drmPciDeviceInfo, *drmPciDeviceInfoPtr;

typedef struct _drmPlatformBusInfo {
    char fullname[512];
} drmPlatformBusInfo, *drmPlatformBusInfoPtr;

typedef struct _drmPlatformDeviceInfo {
    char **compatible;
} drmPlatformDeviceInfo, *drmPlatformDeviceInfoPtr;

typedef struct _drmHost1xBusInfo {
    char fullname[512];
} drmHost1xBusInfo, *drmHost1xBusInfoPtr;

typedef struct _drmHost1xDeviceInfo {
    char **compatible;
} drmHost1xDeviceInfo, *drmHost1xDeviceInfoPtr;

typedef struct _drmDevice {
    char **nodes;
    int available_nodes;
    int bustype;
    union {
        drmPciBusInfoPtr pci;
        drmPlatformBusInfoPtr platform;
        drmHost1xBusInfoPtr host1x;
    } businfo;
    union {
        drmPciDeviceInfoPtr pci;
        drmPlatformDeviceInfoPtr platform;
        drmHost1xDeviceInfoPtr host1x;
    } deviceinfo;
} drmDevice, *drmDevicePtr;

int drmOpen(const char *name, const char *busid);
int drmClose(int fd);
int drmIoctl(int fd, unsigned long request, void *arg);
int drmGetCap(int fd, uint64_t capability, uint64_t *value);
int drmSetMaster(int fd);
int drmDropMaster(int fd);
int drmHandleEvent(int fd, drmEventContextPtr evctx);
drmVersionPtr drmGetVersion(int fd);
void drmFreeVersion(drmVersionPtr v);
int drmSetInterfaceVersion(int fd, drmSetVersionPtr version);
char *drmGetBusid(int fd);
void drmFreeBusid(char *busid);
int drmSetClientCap(int fd, uint64_t capability, uint64_t value);
int drmGetMagic(int fd, drm_magic_t *magic);
int drmAuthMagic(int fd, drm_magic_t magic);
int drmPrimeFDToHandle(int fd, int prime_fd, uint32_t *handle);
int drmPrimeHandleToFD(int fd, uint32_t handle, uint32_t flags, int *prime_fd);
int drmWaitVBlank(int fd, drmVBlankPtr vbl);
int drmCrtcGetSequence(int fd, uint32_t crtcId, uint64_t *sequence, uint64_t *ns);
int drmCrtcQueueSequence(int fd, uint32_t crtcId, uint32_t flags, uint64_t sequence, uint64_t *sequence_queued, uint64_t user_data);

int drmGetDevice2(int fd, uint32_t flags, drmDevicePtr *device);
int drmGetDevices2(uint32_t flags, drmDevicePtr devices[], int max_devices);
int drmGetDeviceFromDevId(dev_t dev_id, uint32_t flags, drmDevicePtr *device);
void drmFreeDevice(drmDevicePtr *device);
void drmFreeDevices(drmDevicePtr devices[], int count);
int drmGetNodeTypeFromFd(int fd);
char *drmGetRenderDeviceNameFromFd(int fd);
char *drmGetPrimaryDeviceNameFromFd(int fd);
char *drmGetDeviceNameFromFd2(int fd);
int drmDevicesEqual(drmDevicePtr a, drmDevicePtr b);

int drmSyncobjCreate(int fd, uint32_t flags, uint32_t *handle);
int drmSyncobjDestroy(int fd, uint32_t handle);
int drmSyncobjHandleToFD(int fd, uint32_t handle, int *obj_fd);
int drmSyncobjFDToHandle(int fd, int obj_fd, uint32_t *handle);
int drmSyncobjWait(int fd, uint32_t *handles, uint32_t num_handles, int64_t timeout_nsec, uint32_t flags, uint32_t *first_signaled);

#ifdef __cplusplus
}
#endif

#endif /* _XF86DRM_H */
