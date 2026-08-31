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
#include <drm/drm.h>

#ifdef __cplusplus
extern "C" {
#endif

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
    char *name;
    char *date;
    char *desc;
} drmVersion, *drmVersionPtr;

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
int drmPrimeFDToHandle(int fd, int prime_fd, uint32_t *handle);
int drmPrimeHandleToFD(int fd, uint32_t handle, uint32_t flags, int *prime_fd);
int drmWaitVBlank(int fd, drmVBlankPtr vbl);
int drmCrtcGetSequence(int fd, uint32_t crtcId, uint64_t *sequence, uint64_t *ns);
int drmCrtcQueueSequence(int fd, uint32_t crtcId, uint32_t flags, uint64_t sequence, uint64_t *sequence_queued, uint64_t user_data);

#ifdef __cplusplus
}
#endif

#endif /* _XF86DRM_H */
