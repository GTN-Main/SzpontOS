/*
 * SzpontOS - VirtIO GPU Driver Header (<drivers/virtio_gpu.h>)
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef SZPONTOS_DRIVERS_VIRTIO_GPU_H
#define SZPONTOS_DRIVERS_VIRTIO_GPU_H

#include <kernel/types.h>
#include <drivers/pci.h>

#define VIRTIO_GPU_VENDOR_ID         0x1AF4
#define VIRTIO_GPU_DEVICE_ID_MODERN  0x1050
#define VIRTIO_GPU_DEVICE_ID_LEGACY  0x1010

/* VirtIO PCI Capability types */
#define VIRTIO_PCI_CAP_COMMON_CFG    1
#define VIRTIO_PCI_CAP_NOTIFY_CFG    2
#define VIRTIO_PCI_CAP_ISR_CFG       3
#define VIRTIO_PCI_CAP_DEVICE_CFG    4
#define VIRTIO_PCI_CAP_PCI_CFG       5

/* VirtIO Device Status Bits */
#define VIRTIO_STATUS_ACKNOWLEDGE    1
#define VIRTIO_STATUS_DRIVER         2
#define VIRTIO_STATUS_DRIVER_OK      4
#define VIRTIO_STATUS_FEATURES_OK    8
#define VIRTIO_STATUS_FAILED         128

/* VirtIO Common Features */
#define VIRTIO_F_VERSION_1           (1ULL << 32)
#define VIRTIO_GPU_F_VIRGL           (1 << 0)
#define VIRTIO_GPU_F_EDID            (1 << 1)

/* VirtIO GPU Control Types */
enum virtio_gpu_ctrl_type {
    VIRTIO_GPU_UNDEFINED = 0,

    /* 2D Commands */
    VIRTIO_GPU_CMD_GET_DISPLAY_INFO = 0x0100,
    VIRTIO_GPU_CMD_RESOURCE_CREATE_2D,
    VIRTIO_GPU_CMD_RESOURCE_UNREF,
    VIRTIO_GPU_CMD_SET_SCANOUT,
    VIRTIO_GPU_CMD_RESOURCE_FLUSH,
    VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D,
    VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING,
    VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING,
    VIRTIO_GPU_CMD_GET_CAPSET_INFO,
    VIRTIO_GPU_CMD_GET_CAPSET,
    VIRTIO_GPU_CMD_GET_EDID,

    /* 3D Commands */
    VIRTIO_GPU_CMD_CTX_CREATE = 0x0200,
    VIRTIO_GPU_CMD_CTX_DESTROY,
    VIRTIO_GPU_CMD_CTX_ATTACH_RESOURCE,
    VIRTIO_GPU_CMD_CTX_DETACH_RESOURCE,
    VIRTIO_GPU_CMD_RESOURCE_CREATE_3D,
    VIRTIO_GPU_CMD_TRANSFER_TO_HOST_3D,
    VIRTIO_GPU_CMD_TRANSFER_FROM_HOST_3D,
    VIRTIO_GPU_CMD_SUBMIT_3D,
    VIRTIO_GPU_CMD_RESOURCE_MAP_BLOB,
    VIRTIO_GPU_CMD_RESOURCE_UNMAP_BLOB,

    /* Success Responses */
    VIRTIO_GPU_RESP_OK_NODATA = 0x1100,
    VIRTIO_GPU_RESP_OK_DISPLAY_INFO,
    VIRTIO_GPU_RESP_OK_CAPSET_INFO,
    VIRTIO_GPU_RESP_OK_CAPSET,
    VIRTIO_GPU_RESP_OK_EDID,

    /* Error Responses */
    VIRTIO_GPU_RESP_ERR_UNSPEC = 0x1200,
    VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY,
    VIRTIO_GPU_RESP_ERR_INVALID_SCANOUT_ID,
    VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID,
    VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID,
    VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER,
};

/* Pixel Formats */
enum virtio_gpu_formats {
    VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM = 1,
    VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM = 2,
    VIRTIO_GPU_FORMAT_A8R8G8B8_UNORM = 3,
    VIRTIO_GPU_FORMAT_X8R8G8B8_UNORM = 4,
    VIRTIO_GPU_FORMAT_R8G8B8A8_UNORM = 67,
    VIRTIO_GPU_FORMAT_X8B8G8R8_UNORM = 68,
};

/* VirtIO PCI Capability Header */
typedef struct __attribute__((packed)) {
    uint8_t cap_vndr;
    uint8_t cap_next;
    uint8_t cap_len;
    uint8_t cfg_type;
    uint8_t bar;
    uint8_t id;
    uint8_t padding[2];
    uint32_t offset;
    uint32_t length;
} virtio_pci_cap_t;

typedef struct __attribute__((packed)) {
    virtio_pci_cap_t cap;
    uint32_t notify_off_multiplier;
} virtio_pci_notify_cap_t;

/* VirtIO Modern Common Configuration Layout */
typedef struct __attribute__((packed)) {
    uint32_t device_feature_select;
    uint32_t device_feature;
    uint32_t guest_feature_select;
    uint32_t guest_feature;
    uint16_t msix_config;
    uint16_t num_queues;
    uint8_t  device_status;
    uint8_t  config_generation;

    uint16_t queue_select;
    uint16_t queue_size;
    uint16_t queue_msix_vector;
    uint16_t queue_enable;
    uint16_t queue_notify_off;
    uint32_t queue_desc_lo;
    uint32_t queue_desc_hi;
    uint32_t queue_avail_lo;
    uint32_t queue_avail_hi;
    uint32_t queue_used_lo;
    uint32_t queue_used_hi;
} virtio_pci_common_cfg_t;

/* VirtIO Ring Descriptors */
typedef struct __attribute__((packed)) {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} vring_desc_t;

#define VRING_DESC_F_NEXT     1
#define VRING_DESC_F_WRITE    2
#define VRING_DESC_F_INDIRECT 4

typedef struct __attribute__((packed)) {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];
} vring_avail_t;

typedef struct __attribute__((packed)) {
    uint32_t id;
    uint32_t len;
} vring_used_elem_t;

typedef struct __attribute__((packed)) {
    uint16_t flags;
    uint16_t idx;
    vring_used_elem_t ring[];
} vring_used_t;

#define VIRTIO_GPU_FLAG_FENCE (1 << 0)

/* VirtIO GPU Command Header */
typedef struct __attribute__((packed)) {
    uint32_t type;
    uint32_t flags;
    uint64_t fence_id;
    uint32_t ctx_id;
    uint8_t  ring_idx;
    uint8_t  padding[3];
} virtio_gpu_ctrl_hdr_t;

typedef struct __attribute__((packed)) {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} virtio_gpu_rect_t;

/* VIRTIO_GPU_CMD_RESOURCE_CREATE_2D */
typedef struct __attribute__((packed)) {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t resource_id;
    uint32_t format;
    uint32_t width;
    uint32_t height;
} virtio_gpu_resource_create_2d_t;

/* VIRTIO_GPU_CMD_SET_SCANOUT */
typedef struct __attribute__((packed)) {
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_rect_t r;
    uint32_t scanout_id;
    uint32_t resource_id;
} virtio_gpu_set_scanout_t;

/* VIRTIO_GPU_CMD_RESOURCE_FLUSH */
typedef struct __attribute__((packed)) {
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_rect_t r;
    uint32_t resource_id;
    uint32_t padding;
} virtio_gpu_resource_flush_t;

/* VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D */
typedef struct __attribute__((packed)) {
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_rect_t r;
    uint64_t offset;
    uint32_t resource_id;
    uint32_t padding;
} virtio_gpu_transfer_to_host_2d_t;

/* Memory entry for attach backing */
typedef struct __attribute__((packed)) {
    uint64_t addr;
    uint32_t length;
    uint32_t padding;
} virtio_gpu_mem_entry_t;

/* VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING */
typedef struct __attribute__((packed)) {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t resource_id;
    uint32_t nr_entries;
} virtio_gpu_resource_attach_backing_t;

/* VIRTIO_GPU_RESP_OK_DISPLAY_INFO */
#define VIRTIO_GPU_MAX_SCANOUTS 16
typedef struct __attribute__((packed)) {
    virtio_gpu_ctrl_hdr_t hdr;
    struct {
        virtio_gpu_rect_t r;
        uint32_t enabled;
        uint32_t flags;
    } pmodes[VIRTIO_GPU_MAX_SCANOUTS];
} virtio_gpu_resp_display_info_t;

/* 3D Box definition */
typedef struct __attribute__((packed)) {
    uint32_t x;
    uint32_t y;
    uint32_t z;
    uint32_t w;
    uint32_t h;
    uint32_t d;
} virtio_gpu_box_t;

/* VIRTIO_GPU_CMD_RESOURCE_CREATE_3D */
typedef struct __attribute__((packed)) {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t resource_id;
    uint32_t target;
    uint32_t format;
    uint32_t bind;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t array_size;
    uint32_t last_level;
    uint32_t nr_samples;
    uint32_t flags;
    uint32_t padding;
} virtio_gpu_resource_create_3d_t;

/* VIRTIO_GPU_CMD_CTX_CREATE */
typedef struct __attribute__((packed)) {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t nlen;
    uint32_t context_init;
    char debug_name[64];
} virtio_gpu_ctx_create_t;

/* VIRTIO_GPU_CMD_CTX_DESTROY */
typedef struct __attribute__((packed)) {
    virtio_gpu_ctrl_hdr_t hdr;
} virtio_gpu_ctx_destroy_t;

/* VIRTIO_GPU_CMD_CTX_ATTACH_RESOURCE, VIRTIO_GPU_CMD_CTX_DETACH_RESOURCE */
typedef struct __attribute__((packed)) {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t resource_id;
    uint32_t padding;
} virtio_gpu_ctx_resource_t;

/* VIRTIO_GPU_CMD_SUBMIT_3D */
typedef struct __attribute__((packed)) {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t size;
    uint32_t padding;
} virtio_gpu_cmd_submit_t;

/* VIRTIO_GPU_CMD_TRANSFER_TO_HOST_3D, VIRTIO_GPU_CMD_TRANSFER_FROM_HOST_3D */
typedef struct __attribute__((packed)) {
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_box_t box;
    uint64_t offset;
    uint32_t resource_id;
    uint32_t level;
    uint32_t stride;
    uint32_t layer_stride;
} virtio_gpu_transfer_host_3d_t;

/* Virgl Capability Sets */
#define VIRTIO_GPU_CAPSET_VIRGL  1
#define VIRTIO_GPU_CAPSET_VIRGL2 2

/* VIRTIO_GPU_CMD_GET_CAPSET_INFO */
typedef struct __attribute__((packed)) {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t capset_index;
    uint32_t padding;
} virtio_gpu_get_capset_info_t;

/* VIRTIO_GPU_RESP_OK_CAPSET_INFO */
typedef struct __attribute__((packed)) {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t capset_id;
    uint32_t capset_max_version;
    uint32_t capset_max_size;
    uint32_t padding;
} virtio_gpu_resp_capset_info_t;

/* VIRTIO_GPU_CMD_GET_CAPSET */
typedef struct __attribute__((packed)) {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t capset_id;
    uint32_t capset_version;
} virtio_gpu_get_capset_t;

/* Public API */
bool virtio_gpu_init(pci_device_t *pci_dev);
bool virtio_gpu_is_active(void);
bool virtio_gpu_has_virgl(void);
pci_device_t *virtio_gpu_get_pci_dev(void);
void virtio_gpu_get_resolution(uint32_t *w, uint32_t *h);
void *virtio_gpu_get_framebuffer(void);
uint32_t virtio_gpu_get_pitch(void);
void virtio_gpu_flush(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
void virtio_gpu_blit(const uint32_t *src, size_t pitch_pixels, size_t dst_x, size_t dst_y, size_t w, size_t h);

/* 3D / Virgl API */
int virtio_gpu_create_context(uint32_t ctx_id, const char *debug_name);
int virtio_gpu_destroy_context(uint32_t ctx_id);
int virtio_gpu_create_3d_resource(uint32_t res_id, uint32_t target, uint32_t format,
                                  uint32_t bind, uint32_t width, uint32_t height,
                                  uint32_t depth, uint32_t array_size, uint32_t last_level,
                                  uint32_t nr_samples, uint32_t flags, uintptr_t phys_addr, size_t size);
int virtio_gpu_attach_resource_to_ctx(uint32_t ctx_id, uint32_t res_id);
int virtio_gpu_submit_3d(uint32_t ctx_id, const void *cmd_buf, size_t size, uint64_t fence_id);
int virtio_gpu_transfer_to_host_3d(uint32_t ctx_id, uint32_t res_id, uint64_t offset,
                                   uint32_t level, uint32_t stride, uint32_t layer_stride,
                                   const virtio_gpu_box_t *box);
int virtio_gpu_transfer_from_host_3d(uint32_t ctx_id, uint32_t res_id, uint64_t offset,
                                     uint32_t level, uint32_t stride, uint32_t layer_stride,
                                     const virtio_gpu_box_t *box);
int virtio_gpu_get_caps(uint32_t cap_set_id, uint32_t cap_set_ver, void *out_caps, size_t max_size);
uint32_t virtio_gpu_get_supported_capsets(void);
uint32_t virtio_gpu_alloc_resource_id(void);
uint64_t virtio_gpu_alloc_fence_id(void);

#endif /* SZPONTOS_DRIVERS_VIRTIO_GPU_H */
