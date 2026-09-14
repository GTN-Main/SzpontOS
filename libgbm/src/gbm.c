/*
 * SzpontOS Native Generic Buffer Management (GBM) Library
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include "gbm.h"
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>

struct gbm_device {
    int fd;
};

struct gbm_bo {
    struct gbm_device *gbm;
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint32_t flags;
    uint32_t handle;
    uint32_t pitch;
    uint64_t size;
    void *map_addr;
    void *user_data;
    void (*user_data_destroy)(struct gbm_bo *, void *);
};

struct gbm_device *gbm_create_device(int fd) {
    if (fd < 0) return NULL;
    struct gbm_device *dev = (struct gbm_device *)malloc(sizeof(struct gbm_device));
    if (!dev) return NULL;
    dev->fd = fd;
    return dev;
}

void gbm_device_destroy(struct gbm_device *gbm) {
    if (gbm) {
        free(gbm);
    }
}

int gbm_device_get_fd(struct gbm_device *gbm) {
    return gbm ? gbm->fd : -1;
}

const char *gbm_device_get_backend_name(struct gbm_device *gbm) {
    (void)gbm;
    return "dumb";
}

int gbm_device_is_format_supported(struct gbm_device *gbm, uint32_t format, uint32_t flags) {
    (void)gbm;
    (void)flags;
    switch (format) {
    case GBM_FORMAT_XRGB8888:
    case GBM_FORMAT_ARGB8888:
    case GBM_FORMAT_XBGR8888:
    case GBM_FORMAT_ABGR8888:
    case GBM_FORMAT_RGB565:
        return 1;
    default:
        return 0;
    }
}

static uint32_t format_to_bpp(uint32_t format) {
    switch (format) {
    case GBM_FORMAT_RGB565:
        return 16;
    case GBM_FORMAT_C8:
        return 8;
    default:
        return 32;
    }
}

struct gbm_bo *gbm_bo_create(struct gbm_device *gbm, uint32_t width, uint32_t height,
                             uint32_t format, uint32_t flags) {
    if (!gbm || !width || !height) return NULL;

    struct gbm_bo *bo = (struct gbm_bo *)calloc(1, sizeof(struct gbm_bo));
    if (!bo) return NULL;

    bo->gbm = gbm;
    bo->width = width;
    bo->height = height;
    bo->format = format;
    bo->flags = flags;

    uint32_t bpp = format_to_bpp(format);
    int ret = drmModeCreateDumb(gbm->fd, width, height, bpp, 0,
                                &bo->handle, &bo->pitch, &bo->size);
    if (ret != 0) {
        free(bo);
        return NULL;
    }

    uint64_t offset = 0;
    ret = drmModeMapDumb(gbm->fd, bo->handle, &offset);
    if (ret == 0) {
        bo->map_addr = mmap(NULL, bo->size, PROT_READ | PROT_WRITE,
                            MAP_SHARED, gbm->fd, (off_t)offset);
        if (bo->map_addr == MAP_FAILED) {
            bo->map_addr = NULL;
        }
    }

    return bo;
}

struct gbm_bo *gbm_bo_create_with_modifiers(struct gbm_device *gbm, uint32_t width, uint32_t height,
                                            uint32_t format, const uint64_t *modifiers,
                                            const unsigned int count) {
    (void)modifiers;
    (void)count;
    return gbm_bo_create(gbm, width, height, format, GBM_BO_USE_SCANOUT);
}

struct gbm_bo *gbm_bo_create_with_modifiers2(struct gbm_device *gbm, uint32_t width, uint32_t height,
                                             uint32_t format, const uint64_t *modifiers,
                                             const unsigned int count, uint32_t flags) {
    (void)modifiers;
    (void)count;
    return gbm_bo_create(gbm, width, height, format, flags);
}

int gbm_bo_get_bpp(struct gbm_bo *bo) {
    return bo ? (int)format_to_bpp(bo->format) : 0;
}

char *gbm_format_get_name(uint32_t gbm_format, struct gbm_format_name_desc *desc) {
    if (!desc) return NULL;
    desc->name[0] = (char)(gbm_format & 0xff);
    desc->name[1] = (char)((gbm_format >> 8) & 0xff);
    desc->name[2] = (char)((gbm_format >> 16) & 0xff);
    desc->name[3] = (char)((gbm_format >> 24) & 0xff);
    desc->name[4] = '\0';
    return desc->name;
}

struct gbm_bo *gbm_bo_import(struct gbm_device *gbm, uint32_t type, void *buffer, uint32_t flags) {
    (void)gbm;
    (void)type;
    (void)buffer;
    (void)flags;
    return NULL;
}

void gbm_bo_destroy(struct gbm_bo *bo) {
    if (!bo) return;

    if (bo->user_data_destroy && bo->user_data) {
        bo->user_data_destroy(bo, bo->user_data);
    }

    if (bo->map_addr) {
        munmap(bo->map_addr, bo->size);
        bo->map_addr = NULL;
    }

    if (bo->gbm && bo->handle) {
        drmModeDestroyDumb(bo->gbm->fd, bo->handle);
        bo->handle = 0;
    }

    free(bo);
}

struct gbm_device *gbm_bo_get_device(struct gbm_bo *bo) {
    return bo ? bo->gbm : NULL;
}

uint32_t gbm_bo_get_width(struct gbm_bo *bo) {
    return bo ? bo->width : 0;
}

uint32_t gbm_bo_get_height(struct gbm_bo *bo) {
    return bo ? bo->height : 0;
}

uint32_t gbm_bo_get_stride(struct gbm_bo *bo) {
    return bo ? bo->pitch : 0;
}

uint32_t gbm_bo_get_format(struct gbm_bo *bo) {
    return bo ? bo->format : 0;
}

union gbm_bo_handle gbm_bo_get_handle(struct gbm_bo *bo) {
    union gbm_bo_handle h;
    h.u64 = 0;
    h.u32 = bo ? bo->handle : 0;
    return h;
}

uint64_t gbm_bo_get_modifier(struct gbm_bo *bo) {
    (void)bo;
    return 0; /* DRM_FORMAT_MOD_LINEAR */
}

int gbm_bo_get_fd(struct gbm_bo *bo) {
    return (bo && bo->gbm) ? bo->gbm->fd : -1;
}

void *gbm_bo_map(struct gbm_bo *bo, uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                 uint32_t flags, uint32_t *stride, void **map_data) {
    (void)width;
    (void)height;
    (void)flags;
    if (!bo) return NULL;

    if (stride) {
        *stride = bo->pitch;
    }
    if (map_data) {
        *map_data = bo;
    }

    if (!bo->map_addr) {
        uint64_t offset = 0;
        if (drmModeMapDumb(bo->gbm->fd, bo->handle, &offset) == 0) {
            bo->map_addr = mmap(NULL, bo->size, PROT_READ | PROT_WRITE,
                                MAP_SHARED, bo->gbm->fd, (off_t)offset);
            if (bo->map_addr == MAP_FAILED) {
                bo->map_addr = NULL;
                return NULL;
            }
        } else {
            return NULL;
        }
    }

    uint32_t bytes_per_pixel = format_to_bpp(bo->format) / 8;
    return (uint8_t *)bo->map_addr + (y * bo->pitch) + (x * bytes_per_pixel);
}

void gbm_bo_unmap(struct gbm_bo *bo, void *map_data) {
    (void)bo;
    (void)map_data;
}

void gbm_bo_set_user_data(struct gbm_bo *bo, void *data,
                          void (*destroy_user_data)(struct gbm_bo *, void *)) {
    if (!bo) return;
    bo->user_data = data;
    bo->user_data_destroy = destroy_user_data;
}

void *gbm_bo_get_user_data(struct gbm_bo *bo) {
    return bo ? bo->user_data : NULL;
}
