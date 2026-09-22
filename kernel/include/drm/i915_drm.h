/*
 * SzpontOS - Intel i915 DRM UAPI Header (<drm/i915_drm.h>)
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef SZPONTOS_DRM_I915_DRM_H
#define SZPONTOS_DRM_I915_DRM_H

#include <drm/drm.h>

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * Intel i915 Specific IOCTL Offsets (Relative to DRM_COMMAND_BASE = 0x40)
 */
#define DRM_I915_INIT                   0x00
#define DRM_I915_FLUSH                  0x01
#define DRM_I915_FLIP                   0x02
#define DRM_I915_BATCHBUFFER            0x03
#define DRM_I915_IRQ_EMIT               0x04
#define DRM_I915_IRQ_WAIT               0x05
#define DRM_I915_GETPARAM               0x06
#define DRM_I915_SETPARAM               0x07
#define DRM_I915_ALLOC                  0x08
#define DRM_I915_FREE                   0x09
#define DRM_I915_INIT_HEAP              0x0a
#define DRM_I915_CMDBUFFER              0x0b
#define DRM_I915_DESTROY_HEAP           0x0c
#define DRM_I915_SET_VBLANK_PIPE        0x0d
#define DRM_I915_GET_VBLANK_PIPE        0x0e
#define DRM_I915_VBLANK_SWAP            0x0f
#define DRM_I915_HWS_ADDR               0x11
#define DRM_I915_GEM_INIT               0x13
#define DRM_I915_GEM_EXECBUFFER         0x14
#define DRM_I915_GEM_PIN                0x15
#define DRM_I915_GEM_UNPIN              0x16
#define DRM_I915_GEM_BUSY               0x17
#define DRM_I915_GEM_THROTTLE           0x18
#define DRM_I915_GEM_ENTERVT            0x19
#define DRM_I915_GEM_LEAVEVT            0x1a
#define DRM_I915_GEM_CREATE             0x1b
#define DRM_I915_GEM_PREAD              0x1c
#define DRM_I915_GEM_PWRITE             0x1d
#define DRM_I915_GEM_MMAP               0x1e
#define DRM_I915_GEM_SET_DOMAIN         0x1f
#define DRM_I915_GEM_SW_FINISH          0x20
#define DRM_I915_GEM_SET_TILING         0x21
#define DRM_I915_GEM_GET_TILING         0x22
#define DRM_I915_GEM_GET_APERTURE       0x23
#define DRM_I915_GEM_MMAP_GTT           0x24
#define DRM_I915_GEM_MMAP_OFFSET        0x24
#define DRM_I915_GET_PIPE_FROM_CRTC_ID  0x25
#define DRM_I915_GEM_MADVISE            0x26
#define DRM_I915_OVERLAY_PUT_IMAGE      0x27
#define DRM_I915_OVERLAY_ATTRS          0x28
#define DRM_I915_GEM_EXECBUFFER2        0x29
#define DRM_I915_GEM_EXECBUFFER2_WR     DRM_I915_GEM_EXECBUFFER2
#define DRM_I915_GET_SPRITE_COLORKEY    0x2a
#define DRM_I915_SET_SPRITE_COLORKEY    0x2b
#define DRM_I915_GEM_WAIT               0x2c
#define DRM_I915_GEM_CONTEXT_CREATE     0x2d
#define DRM_I915_GEM_CONTEXT_DESTROY    0x2e
#define DRM_I915_GEM_SET_CACHING        0x2f
#define DRM_I915_GEM_GET_CACHING        0x30
#define DRM_I915_REG_READ               0x31
#define DRM_I915_GET_RESET_STATS        0x32
#define DRM_I915_GEM_USERPTR            0x33
#define DRM_I915_GEM_CONTEXT_GETPARAM   0x34
#define DRM_I915_GEM_CONTEXT_SETPARAM   0x35
#define DRM_I915_PERF_OPEN              0x36
#define DRM_I915_PERF_ADD_CONFIG        0x37
#define DRM_I915_PERF_REMOVE_CONFIG     0x38
#define DRM_I915_QUERY                  0x39
#define DRM_I915_GEM_VM_CREATE          0x3a
#define DRM_I915_GEM_VM_DESTROY         0x3b
#define DRM_I915_GEM_CREATE_EXT         0x3c

/*
 * IOCTL Request Macros
 */
#define DRM_IOCTL_I915_GETPARAM \
    DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_GETPARAM, struct drm_i915_getparam)

#define DRM_IOCTL_I915_SETPARAM \
    DRM_IOW(DRM_COMMAND_BASE + DRM_I915_SETPARAM, struct drm_i915_setparam)

#define DRM_IOCTL_I915_GEM_CREATE \
    DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_GEM_CREATE, struct drm_i915_gem_create)

#define DRM_IOCTL_I915_GEM_CREATE_EXT \
    DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_GEM_CREATE_EXT, struct drm_i915_gem_create_ext)

#define DRM_IOCTL_I915_GEM_PREAD \
    DRM_IOW(DRM_COMMAND_BASE + DRM_I915_GEM_PREAD, struct drm_i915_gem_pread)

#define DRM_IOCTL_I915_GEM_PWRITE \
    DRM_IOW(DRM_COMMAND_BASE + DRM_I915_GEM_PWRITE, struct drm_i915_gem_pwrite)

#define DRM_IOCTL_I915_GEM_EXECBUFFER \
    DRM_IOW(DRM_COMMAND_BASE + DRM_I915_GEM_EXECBUFFER, struct drm_i915_gem_execbuffer)

#define DRM_IOCTL_I915_GEM_EXECBUFFER2 \
    DRM_IOW(DRM_COMMAND_BASE + DRM_I915_GEM_EXECBUFFER2, struct drm_i915_gem_execbuffer2)

#define DRM_IOCTL_I915_GEM_EXECBUFFER2_WR \
    DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_GEM_EXECBUFFER2_WR, struct drm_i915_gem_execbuffer2)

#define DRM_IOCTL_I915_GEM_BUSY \
    DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_GEM_BUSY, struct drm_i915_gem_busy)

#define DRM_IOCTL_I915_GEM_THROTTLE \
    DRM_IO(DRM_COMMAND_BASE + DRM_I915_GEM_THROTTLE)

#define DRM_IOCTL_I915_GEM_MMAP \
    DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_GEM_MMAP, struct drm_i915_gem_mmap)

#define DRM_IOCTL_I915_GEM_MMAP_GTT \
    DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_GEM_MMAP_GTT, struct drm_i915_gem_mmap_gtt)

#define DRM_IOCTL_I915_GEM_MMAP_OFFSET \
    DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_GEM_MMAP_OFFSET, struct drm_i915_gem_mmap_offset)

#define DRM_IOCTL_I915_GEM_SET_DOMAIN \
    DRM_IOW(DRM_COMMAND_BASE + DRM_I915_GEM_SET_DOMAIN, struct drm_i915_gem_set_domain)

#define DRM_IOCTL_I915_GEM_SW_FINISH \
    DRM_IOW(DRM_COMMAND_BASE + DRM_I915_GEM_SW_FINISH, struct drm_i915_gem_sw_finish)

#define DRM_IOCTL_I915_GEM_SET_TILING \
    DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_GEM_SET_TILING, struct drm_i915_gem_set_tiling)

#define DRM_IOCTL_I915_GEM_GET_TILING \
    DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_GEM_GET_TILING, struct drm_i915_gem_get_tiling)

#define DRM_IOCTL_I915_GEM_GET_APERTURE \
    DRM_IOR(DRM_COMMAND_BASE + DRM_I915_GEM_GET_APERTURE, struct drm_i915_gem_get_aperture)

#define DRM_IOCTL_I915_GEM_MADVISE \
    DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_GEM_MADVISE, struct drm_i915_gem_madvise)

#define DRM_IOCTL_I915_GEM_WAIT \
    DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_GEM_WAIT, struct drm_i915_gem_wait)

#define DRM_IOCTL_I915_GEM_CONTEXT_CREATE \
    DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_GEM_CONTEXT_CREATE, struct drm_i915_gem_context_create)

#define DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT \
    DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_GEM_CONTEXT_CREATE_EXT, struct drm_i915_gem_context_create_ext)

#define DRM_IOCTL_I915_GEM_CONTEXT_DESTROY \
    DRM_IOW(DRM_COMMAND_BASE + DRM_I915_GEM_CONTEXT_DESTROY, struct drm_i915_gem_context_destroy)

#define DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM \
    DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_GEM_CONTEXT_GETPARAM, struct drm_i915_gem_context_param)

#define DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM \
    DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_GEM_CONTEXT_SETPARAM, struct drm_i915_gem_context_param)

#define DRM_IOCTL_I915_GEM_SET_CACHING \
    DRM_IOW(DRM_COMMAND_BASE + DRM_I915_GEM_SET_CACHING, struct drm_i915_gem_caching)

#define DRM_IOCTL_I915_GEM_GET_CACHING \
    DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_GEM_GET_CACHING, struct drm_i915_gem_caching)

#define DRM_IOCTL_I915_GET_RESET_STATS \
    DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_GET_RESET_STATS, struct drm_i915_reset_stats)

#define DRM_IOCTL_I915_QUERY \
    DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_QUERY, struct drm_i915_query)

/*
 * GETPARAM Parameter IDs
 */
#define I915_PARAM_IRQ_ACTIVE            1
#define I915_PARAM_ALLOW_BATCHBUFFER     2
#define I915_PARAM_LAST_DISPATCH         3
#define I915_PARAM_CHIPSET_ID            4
#define I915_PARAM_HAS_GEM               5
#define I915_PARAM_NUM_FENCES_AVAIL      6
#define I915_PARAM_HAS_OVERLAY           7
#define I915_PARAM_HAS_PAGEFLIPPING      8
#define I915_PARAM_HAS_EXECBUF2          9
#define I915_PARAM_HAS_BSD              10
#define I915_PARAM_HAS_BLT              11
#define I915_PARAM_HAS_RELAXED_FENCING  12
#define I915_PARAM_HAS_COHERENT_RINGS   13
#define I915_PARAM_HAS_EXEC_CONSTANTS   14
#define I915_PARAM_HAS_RELAXED_DELTA    15
#define I915_PARAM_GEN7_SOL_RESET       16
#define I915_PARAM_HAS_GEN7_SOL_RESET   16
#define I915_PARAM_LLC                  17
#define I915_PARAM_HAS_LLC              17
#define I915_PARAM_ALIASING_PPGTT       18
#define I915_PARAM_HAS_ALIASING_PPGTT   18
#define I915_PARAM_WAIT_TIMEOUT         19
#define I915_PARAM_HAS_WAIT_TIMEOUT     19
#define I915_PARAM_SEMAPHORES           20
#define I915_PARAM_HAS_SEMAPHORES       20
#define I915_PARAM_PRIME_VMAP_FLUSH     21
#define I915_PARAM_HAS_PRIME_VMAP_FLUSH 21
#define I915_PARAM_VEBOX                22
#define I915_PARAM_SECURE_BATCHES       23
#define I915_PARAM_HAS_SECURE_BATCHES   23
#define I915_PARAM_PINNED_BATCHES       24
#define I915_PARAM_HAS_PINNED_BATCHES   24
#define I915_PARAM_EXEC_NO_RELOC        25
#define I915_PARAM_HAS_EXEC_NO_RELOC    25
#define I915_PARAM_EXEC_HANDLE_LUT      26
#define I915_PARAM_HAS_EXEC_HANDLE_LUT  26
#define I915_PARAM_WT                   27
#define I915_PARAM_HAS_WT               27
#define I915_PARAM_CMD_PARSER_VERSION   28
#define I915_PARAM_COHERENT_PHYS_GTT    29
#define I915_PARAM_MMAP_VERSION         30
#define I915_PARAM_BSD2                 31
#define I915_PARAM_REVISION             32
#define I915_PARAM_SUBSLICE_TOTAL       33
#define I915_PARAM_EU_TOTAL             34
#define I915_PARAM_GPU_RESET            35
#define I915_PARAM_HAS_GPU_RESET        35
#define I915_PARAM_RESOURCE_STREAMER    36
#define I915_PARAM_HAS_RESOURCE_STREAMER 36
#define I915_PARAM_EXEC_SOFTPIN          37
#define I915_PARAM_HAS_EXEC_SOFTPIN      37
#define I915_PARAM_POOLED_EU            38
#define I915_PARAM_MIN_EU_IN_POOL       39
#define I915_PARAM_MMAP_GTT_VERSION     40
#define I915_PARAM_SCHEDULER            41
#define I915_PARAM_HUC_STATUS           42
#define I915_PARAM_EXEC_ASYNC           43
#define I915_PARAM_EXEC_FENCE           44
#define I915_PARAM_HAS_EXEC_FENCE       44
#define I915_PARAM_EXEC_CAPTURE         45
#define I915_PARAM_HAS_EXEC_CAPTURE     45
#define I915_PARAM_SLICE_MASK           46
#define I915_PARAM_SUBSLICE_MASK        47
#define I915_PARAM_EXEC_BATCH_FIRST     48
#define I915_PARAM_HAS_EXEC_BATCH_FIRST 48
#define I915_PARAM_EXEC_FENCE_ARRAY     49
#define I915_PARAM_HAS_EXEC_FENCE_ARRAY 49
#define I915_PARAM_CONTEXT_ISOLATION    50
#define I915_PARAM_HAS_CONTEXT_ISOLATION 50
#define I915_PARAM_CS_TIMESTAMP_FREQUENCY 51
#define I915_PARAM_MMAP_GTT_COHERENT    52
#define I915_PARAM_HAS_PINNED_CONTEXTS  53
#define I915_PARAM_HAS_USERPTR_PROBE    56

typedef struct drm_i915_getparam {
    int32_t param;
    int32_t *value;
} drm_i915_getparam_t;

typedef struct drm_i915_setparam {
    int32_t param;
    int32_t value;
} drm_i915_setparam_t;

/*
 * GEM Buffer Object Creation
 */
struct drm_i915_gem_create {
    uint64_t size;
    uint32_t handle;
    uint32_t pad;
};

struct drm_i915_gem_pread {
    uint32_t handle;
    uint32_t pad;
    uint64_t offset;
    uint64_t size;
    uint64_t data_ptr;
};

struct drm_i915_gem_pwrite {
    uint32_t handle;
    uint32_t pad;
    uint64_t offset;
    uint64_t size;
    uint64_t data_ptr;
};

struct drm_i915_gem_execbuffer {
    uint64_t buffers_ptr;
    uint32_t buffer_count;
    uint32_t batch_start_offset;
    uint32_t batch_len;
    uint32_t DR1;
    uint32_t DR4;
    uint32_t num_cliprects;
    uint64_t cliprects_ptr;
};

struct drm_i915_gem_create_ext {
    uint64_t size;
    uint32_t handle;
    uint32_t flags;
    uint64_t extensions;
};

/*
 * GEM Tiling Modes
 */
#define I915_TILING_NONE 0
#define I915_TILING_X    1
#define I915_TILING_Y    2
#define I915_TILING_Yf   3

struct drm_i915_gem_set_tiling {
    uint32_t handle;
    uint32_t tiling_mode;
    uint32_t stride;
    uint32_t swizzle_mode;
};

struct drm_i915_gem_get_tiling {
    uint32_t handle;
    uint32_t tiling_mode;
    uint32_t swizzle_mode;
    uint32_t phys_swizzle_mode;
};

#define I915_BIT_6_SWIZZLE_NONE  0
#define I915_BIT_6_SWIZZLE_9     1
#define I915_BIT_6_SWIZZLE_9_10  2
#define I915_BIT_6_SWIZZLE_9_11  3
#define I915_BIT_6_SWIZZLE_9_10_11 4
#define I915_BIT_6_SWIZZLE_UNKNOWN 5
#define I915_BIT_6_SWIZZLE_9_17  6
#define I915_BIT_6_SWIZZLE_9_10_17 7

/*
 * GEM Mmap
 */
struct drm_i915_gem_mmap {
    uint32_t handle;
    uint32_t pad;
    uint64_t offset;
    uint64_t size;
    uint64_t addr_ptr;
    uint64_t flags;
};

struct drm_i915_gem_mmap_gtt {
    uint32_t handle;
    uint32_t pad;
    uint64_t offset;
};

struct drm_i915_gem_mmap_offset {
    uint32_t handle;
    uint32_t pad;
    uint64_t offset;
    uint64_t flags;
    uint64_t extensions;
};

#define I915_MMAP_OFFSET_GTT 0
#define I915_MMAP_OFFSET_WC  1
#define I915_MMAP_OFFSET_WB  2
#define I915_MMAP_OFFSET_UC  3

/*
 * GEM Domain Sync & Caching
 */
#define I915_GEM_DOMAIN_CPU      0x00000001
#define I915_GEM_DOMAIN_RENDER   0x00000002
#define I915_GEM_DOMAIN_SAMPLER  0x00000004
#define I915_GEM_DOMAIN_COMMAND  0x00000008
#define I915_GEM_DOMAIN_INSTRUCTION 0x00000010
#define I915_GEM_DOMAIN_VERTEX   0x00000020
#define I915_GEM_DOMAIN_GTT      0x00000040

struct drm_i915_gem_set_domain {
    uint32_t handle;
    uint32_t read_domains;
    uint32_t write_domain;
};

struct drm_i915_gem_sw_finish {
    uint32_t handle;
};

#define I915_CACHING_NONE   0
#define I915_CACHING_CACHED 1
#define I915_CACHING_DISPLAY 2

#define I915_CACHE_NONE     0
#define I915_CACHE_LLC      1
#define I915_CACHE_L3_LLC   2
#define I915_CACHE_WT       3

struct drm_i915_gem_caching {
    uint32_t handle;
    uint32_t caching;
};

/*
 * GEM Aperture & Busy Check
 */
struct drm_i915_gem_get_aperture {
    uint64_t aper_size;
    uint64_t aper_available_size;
};

struct drm_i915_gem_busy {
    uint32_t handle;
    uint32_t busy;
};

struct drm_i915_gem_wait {
    uint32_t handle;
    uint32_t flags;
    int64_t  timeout_ns;
};

struct drm_i915_gem_madvise {
    uint32_t handle;
    uint32_t madv;
    uint32_t retained;
};

#define I915_MADV_WILLNEED 0
#define I915_MADV_DONTNEED 1
#define I915_MADV_PURGED   2

/*
 * GEM Relocation & ExecBuffer2 (Core 3D Engine Submission)
 */
struct drm_i915_gem_relocation_entry {
    uint32_t target_handle;
    uint32_t delta;
    uint64_t offset;
    uint64_t presumed_offset;
    uint32_t read_domains;
    uint32_t write_domain;
};

struct drm_i915_gem_exec_object2 {
    uint32_t handle;
    uint32_t relocation_count;
    uint64_t relocs_ptr;
    uint64_t alignment;
    uint64_t offset;
    uint64_t flags;
    union {
        uint64_t rsvd1;
        uint64_t pad_to_size;
    };
    uint64_t rsvd2;
};

#define EXEC_OBJECT_NEEDS_FENCE          (1 << 0)
#define EXEC_OBJECT_NEEDS_GTT            (1 << 1)
#define EXEC_OBJECT_WRITE                (1 << 2)
#define EXEC_OBJECT_SUPPORTS_48B_ADDRESS (1 << 3)
#define EXEC_OBJECT_PINNED               (1 << 4)
#define EXEC_OBJECT_PAD_TO_SIZE          (1 << 5)
#define EXEC_OBJECT_ASYNC                (1 << 6)
#define EXEC_OBJECT_CAPTURE              (1 << 7)

struct drm_i915_gem_execbuffer2 {
    uint64_t buffers_ptr;
    uint32_t buffer_count;
    uint32_t batch_start_offset;
    uint32_t batch_len;
    uint32_t DR1;
    uint32_t DR4;
    uint32_t num_cliprects;
    uint64_t cliprects_ptr;
    uint64_t flags;
    uint64_t rsvd1; /* Context ID */
    uint64_t rsvd2;
};

#define I915_EXEC_RING_MASK              (0x3f)
#define I915_EXEC_DEFAULT                (0 << 0)
#define I915_EXEC_RENDER                 (1 << 0)
#define I915_EXEC_BSD                    (2 << 0)
#define I915_EXEC_BLT                    (3 << 0)
#define I915_EXEC_VEBOX                  (4 << 0)

#define I915_EXEC_CONSTANTS_MASK         (3 << 6)
#define I915_EXEC_CONSTANTS_REL_GENERAL  (0 << 6)
#define I915_EXEC_CONSTANTS_ABSOLUTE     (1 << 6)
#define I915_EXEC_CONSTANTS_REL_SURFACE  (2 << 6)

#define I915_EXEC_GEN7_SOL_RESET         (1 << 8)
#define I915_EXEC_SECURE                 (1 << 9)
#define I915_EXEC_IS_PINNED              (1 << 10)
#define I915_EXEC_NO_RELOC               (1 << 11)
#define I915_EXEC_HANDLE_LUT             (1 << 12)
#define I915_EXEC_BATCH_FIRST            (1 << 15)
#define I915_EXEC_FENCE_IN               (1 << 16)
#define I915_EXEC_FENCE_OUT              (1 << 17)
#define I915_EXEC_FENCE_ARRAY            (1 << 19)

/*
 * GEM Hardware Contexts
 */
struct drm_i915_gem_context_create {
    uint32_t ctx_id;
    uint32_t pad;
};

struct drm_i915_gem_context_create_ext {
    uint32_t ctx_id;
    uint32_t flags;
    uint64_t extensions;
};

struct drm_i915_gem_context_destroy {
    uint32_t ctx_id;
    uint32_t pad;
};

#define I915_CONTEXT_PARAM_BAN_PERIOD   0x1
#define I915_CONTEXT_PARAM_NO_ZEROMAP   0x2
#define I915_CONTEXT_PARAM_GTT_SIZE     0x3
#define I915_CONTEXT_PARAM_NO_ERROR_CAPTURE 0x4
#define I915_CONTEXT_PARAM_BANNABLE     0x5
#define I915_CONTEXT_PARAM_PRIORITY     0x6
#define I915_CONTEXT_PARAM_SSEU         0x7
#define I915_CONTEXT_PARAM_RECOVERABLE  0x8
#define I915_CONTEXT_PARAM_VM           0x9
#define I915_CONTEXT_PARAM_ENGINES      0xa
#define I915_CONTEXT_PARAM_PERSISTENCE  0xb

struct drm_i915_gem_context_param {
    uint32_t ctx_id;
    uint32_t size;
    uint64_t param;
    uint64_t value;
};

/*
 * Reset Stats & Query
 */
struct drm_i915_reset_stats {
    uint32_t ctx_id;
    uint32_t flags;
    uint32_t reset_count;
    uint32_t batch_active;
    uint32_t batch_pending;
    uint32_t pad;
};

struct drm_i915_query_item {
    uint64_t query_id;
#define DRM_I915_QUERY_TOPOLOGY_INFO    1
#define DRM_I915_QUERY_ENGINE_INFO      2
#define DRM_I915_QUERY_PERF_CONFIG      3
#define DRM_I915_QUERY_MEMORY_REGIONS   4
#define DRM_I915_QUERY_HWCONFIG_BLOB    5
#define DRM_I915_QUERY_GEOMETRY_SUBSLICES 6
    int32_t  length;
    uint32_t flags;
    uint64_t data_ptr;
};

struct drm_i915_query {
    uint32_t num_items;
    uint32_t flags;
    uint64_t items_ptr;
};

#if defined(__cplusplus)
}
#endif

#endif /* SZPONTOS_DRM_I915_DRM_H */
