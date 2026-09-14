/*
 * SzpontOS Native Generic Buffer Management (GBM) Library
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef _GBM_H_
#define _GBM_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum gbm_bo_format {
    GBM_BO_FORMAT_XRGB8888 = 0x34325258,
    GBM_BO_FORMAT_ARGB8888 = 0x34325241,
};

#define GBM_FORMAT_BIG_ENDIAN (1 << 31)

#define __gbm_fourcc_code(a,b,c,d) ((uint32_t)(a) | ((uint32_t)(b) << 8) | \
			      ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))

#define GBM_FORMAT_C8           __gbm_fourcc_code('C', '8', ' ', ' ')
#define GBM_FORMAT_R8           __gbm_fourcc_code('R', '8', ' ', ' ')
#define GBM_FORMAT_RGB565       __gbm_fourcc_code('R', 'G', '1', '6')
#define GBM_FORMAT_ARGB1555     __gbm_fourcc_code('A', 'R', '1', '5')
#define GBM_FORMAT_XRGB1555     __gbm_fourcc_code('X', 'R', '1', '5')
#define GBM_FORMAT_XRGB8888     GBM_BO_FORMAT_XRGB8888
#define GBM_FORMAT_ARGB8888     GBM_BO_FORMAT_ARGB8888
#define GBM_FORMAT_XBGR8888     __gbm_fourcc_code('X', 'B', '2', '4')
#define GBM_FORMAT_ABGR8888     __gbm_fourcc_code('A', 'B', '2', '4')
#define GBM_FORMAT_ARGB2101010  __gbm_fourcc_code('A', 'R', '3', '0')
#define GBM_FORMAT_XRGB2101010  __gbm_fourcc_code('X', 'R', '3', '0')

enum gbm_bo_flags {
    GBM_BO_USE_SCANOUT      = (1 << 0),
    GBM_BO_USE_CURSOR       = (1 << 1),
    GBM_BO_USE_RENDERING    = (1 << 2),
    GBM_BO_USE_WRITE        = (1 << 3),
    GBM_BO_USE_LINEAR       = (1 << 4),
    GBM_BO_USE_PROTECTED    = (1 << 5),
    GBM_BO_USE_FRONT_RENDERING = (1 << 6),
};

enum gbm_bo_transfer_flags {
    GBM_BO_TRANSFER_READ       = (1 << 0),
    GBM_BO_TRANSFER_WRITE      = (1 << 1),
    GBM_BO_TRANSFER_READ_WRITE = (GBM_BO_TRANSFER_READ | GBM_BO_TRANSFER_WRITE),
};

union gbm_bo_handle {
    void *ptr;
    int32_t s32;
    uint32_t u32;
    int64_t s64;
    uint64_t u64;
};

#define GBM_BO_IMPORT_WL_BUFFER         0x5501
#define GBM_BO_IMPORT_EGL_IMAGE         0x5502
#define GBM_BO_IMPORT_FD                0x5503
#define GBM_BO_IMPORT_FD_MODIFIER       0x5504

struct gbm_import_fd_data {
    int fd;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t format;
};

struct gbm_import_fd_modifier_data {
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint32_t num_fds;
    int fds[4];
    int strides[4];
    int offsets[4];
    uint64_t modifier;
};

struct gbm_device;
struct gbm_bo;
struct gbm_surface;

struct gbm_device *gbm_create_device(int fd);
void gbm_device_destroy(struct gbm_device *gbm);
int gbm_device_get_fd(struct gbm_device *gbm);
const char *gbm_device_get_backend_name(struct gbm_device *gbm);
int gbm_device_is_format_supported(struct gbm_device *gbm, uint32_t format, uint32_t flags);

struct gbm_bo *gbm_bo_create(struct gbm_device *gbm, uint32_t width, uint32_t height, uint32_t format, uint32_t flags);
struct gbm_bo *gbm_bo_create_with_modifiers(struct gbm_device *gbm, uint32_t width, uint32_t height, uint32_t format, const uint64_t *modifiers, const unsigned int count);
struct gbm_bo *gbm_bo_import(struct gbm_device *gbm, uint32_t type, void *buffer, uint32_t flags);
void gbm_bo_destroy(struct gbm_bo *bo);

struct gbm_device *gbm_bo_get_device(struct gbm_bo *bo);
uint32_t gbm_bo_get_width(struct gbm_bo *bo);
uint32_t gbm_bo_get_height(struct gbm_bo *bo);
uint32_t gbm_bo_get_stride(struct gbm_bo *bo);
uint32_t gbm_bo_get_format(struct gbm_bo *bo);
union gbm_bo_handle gbm_bo_get_handle(struct gbm_bo *bo);
uint64_t gbm_bo_get_modifier(struct gbm_bo *bo);
int gbm_bo_get_fd(struct gbm_bo *bo);

void *gbm_bo_map(struct gbm_bo *bo, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t flags, uint32_t *stride, void **map_data);
void gbm_bo_unmap(struct gbm_bo *bo, void *map_data);

struct gbm_format_name_desc {
    char name[5];
};

struct gbm_bo *gbm_bo_create_with_modifiers2(struct gbm_device *gbm, uint32_t width, uint32_t height,
                                             uint32_t format, const uint64_t *modifiers,
                                             const unsigned int count, uint32_t flags);
int gbm_bo_get_bpp(struct gbm_bo *bo);
char *gbm_format_get_name(uint32_t gbm_format, struct gbm_format_name_desc *desc);

void gbm_bo_set_user_data(struct gbm_bo *bo, void *data, void (*destroy_user_data)(struct gbm_bo *, void *));
void *gbm_bo_get_user_data(struct gbm_bo *bo);

#ifdef __cplusplus
}
#endif

#endif /* _GBM_H_ */
