/*
 * SzpontOS - VirtIO GPU / VirtIO-VGA 2D Driver (virtio_gpu.c)
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <drivers/virtio_gpu.h>
#include <drivers/framebuffer.h>
#include <drivers/pci.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/heap.h>
#include <kernel/kprint.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>

static bool g_virtio_gpu_active = false;
static spinlock_t g_virtio_gpu_lock = SPINLOCK_INIT;

static pci_device_t *g_vgpu_pci = NULL;
static volatile virtio_pci_common_cfg_t *g_common_cfg = NULL;
static volatile uint8_t *g_notify_base = NULL;
static uint32_t g_notify_mult = 0;
static volatile uint8_t *g_device_cfg = NULL;

static uint16_t g_ctrl_qsize = 0;
static vring_desc_t *g_ctrl_desc = NULL;
static vring_avail_t *g_ctrl_avail = NULL;
static vring_used_t *g_ctrl_used = NULL;
static uint16_t g_ctrl_last_used_idx = 0;
static volatile uint16_t *g_ctrl_notify_addr = NULL;

#define VGPU_SCRATCH_SIZE (512 * 1024)

/* Virgl 3D State */
static bool g_has_virgl_3d = false;
static uint32_t g_next_resource_id = 2;
static uint64_t g_next_fence_id = 1;

/* Virgl Capset Cache */
typedef struct {
    uint32_t id;
    uint32_t version;
    uint32_t size;
    uint8_t data[2048];
    bool valid;
} virtio_gpu_capset_cache_t;

static virtio_gpu_capset_cache_t g_capset_virgl;
static virtio_gpu_capset_cache_t g_capset_virgl2;

/* Preallocated DMA buffers for commands and responses */
static uint8_t *g_cmd_buf = NULL;
static uintptr_t g_cmd_buf_phys = 0;
static uint8_t *g_resp_buf = NULL;
static uintptr_t g_resp_buf_phys = 0;
static uint8_t *g_extra_buf = NULL;
static uintptr_t g_extra_buf_phys = 0;

/* Primary Framebuffer Scanout Buffer */
static uint32_t g_screen_width = 1280;
static uint32_t g_screen_height = 800;
static uint32_t g_screen_pitch = 1280 * 4;
static uint32_t *g_fb_virt = NULL;
static uintptr_t g_fb_phys = 0;
static size_t g_fb_size = 0;
static size_t g_fb_num_pages = 0;

static void *alloc_dma_zero(size_t size, uintptr_t *phys_out) {
    size_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    uintptr_t phys = (uintptr_t)pmm_alloc_pages(num_pages);
    if (!phys)
        return NULL;
    *phys_out = phys;
    void *virt = (void *)PHYS_TO_VIRT(phys);
    memset(virt, 0, num_pages * PAGE_SIZE);
    return virt;
}

static bool virtio_gpu_send_command(const void *cmd, size_t cmd_size,
                                    const void *extra, size_t extra_size,
                                    void *resp, size_t resp_size) {
    if (!g_virtio_gpu_active && !g_common_cfg)
        return false;

    if (cmd_size > VGPU_SCRATCH_SIZE || extra_size > VGPU_SCRATCH_SIZE || resp_size > VGPU_SCRATCH_SIZE)
        return false;

    memcpy(g_cmd_buf, cmd, cmd_size);
    if (extra && extra_size > 0) {
        memcpy(g_extra_buf, extra, extra_size);
    }
    memset(g_resp_buf, 0, resp_size);

    uint16_t desc_idx = 0;

    /* Descriptor 0: Command */
    g_ctrl_desc[desc_idx].addr = g_cmd_buf_phys;
    g_ctrl_desc[desc_idx].len = (uint32_t)cmd_size;
    g_ctrl_desc[desc_idx].flags = VRING_DESC_F_NEXT;
    g_ctrl_desc[desc_idx].next = (extra && extra_size > 0) ? (desc_idx + 1) : (desc_idx + 1);

    uint16_t resp_desc_idx = desc_idx + 1;

    /* Optional Descriptor 1: Extra data (e.g. mem entries) */
    if (extra && extra_size > 0) {
        desc_idx++;
        g_ctrl_desc[desc_idx].addr = g_extra_buf_phys;
        g_ctrl_desc[desc_idx].len = (uint32_t)extra_size;
        g_ctrl_desc[desc_idx].flags = VRING_DESC_F_NEXT;
        g_ctrl_desc[desc_idx].next = desc_idx + 1;
        resp_desc_idx = desc_idx + 1;
    }

    /* Response Descriptor */
    g_ctrl_desc[resp_desc_idx].addr = g_resp_buf_phys;
    g_ctrl_desc[resp_desc_idx].len = (uint32_t)resp_size;
    g_ctrl_desc[resp_desc_idx].flags = VRING_DESC_F_WRITE;
    g_ctrl_desc[resp_desc_idx].next = 0;

    /* Place head in available ring */
    uint16_t avail_idx = g_ctrl_avail->idx;
    g_ctrl_avail->ring[avail_idx % g_ctrl_qsize] = 0;
    __asm__ volatile ("" ::: "memory");
    g_ctrl_avail->idx = avail_idx + 1;
    __asm__ volatile ("" ::: "memory");

    /* Ring doorbell */
    *g_ctrl_notify_addr = 0;

    /* Wait for host response with timeout */
    uint32_t timeout = 50000000;
    while (g_ctrl_used->idx == g_ctrl_last_used_idx && --timeout > 0) {
        __asm__ volatile ("pause");
    }

    if (timeout == 0) {
        klog_warn("VirtIO-GPU: Command timeout! (type: 0x%x)", ((virtio_gpu_ctrl_hdr_t *)cmd)->type);
        return false;
    }

    g_ctrl_last_used_idx = g_ctrl_used->idx;
    memcpy(resp, g_resp_buf, resp_size);

    virtio_gpu_ctrl_hdr_t *hdr = (virtio_gpu_ctrl_hdr_t *)resp;
    if (hdr->type < VIRTIO_GPU_RESP_OK_NODATA || hdr->type >= VIRTIO_GPU_RESP_ERR_UNSPEC) {
        klog_warn("VirtIO-GPU: Command returned error response 0x%04x", hdr->type);
        return false;
    }

    return true;
}

static bool virtio_gpu_send_batch_xfer_and_flush(const virtio_gpu_transfer_to_host_2d_t *xfer,
                                                 const virtio_gpu_resource_flush_t *flush) {
    if (!g_virtio_gpu_active && !g_common_cfg)
        return false;

    if (g_ctrl_qsize < 4)
        return false;

    memcpy(g_cmd_buf, xfer, sizeof(*xfer));
    memcpy(g_cmd_buf + 256, flush, sizeof(*flush));

    memset(g_resp_buf, 0, 512);

    /* Chain 1 (Desc 0 -> Desc 1): Transfer to host 2D */
    g_ctrl_desc[0].addr = g_cmd_buf_phys;
    g_ctrl_desc[0].len = (uint32_t)sizeof(*xfer);
    g_ctrl_desc[0].flags = VRING_DESC_F_NEXT;
    g_ctrl_desc[0].next = 1;

    g_ctrl_desc[1].addr = g_resp_buf_phys;
    g_ctrl_desc[1].len = (uint32_t)sizeof(virtio_gpu_ctrl_hdr_t);
    g_ctrl_desc[1].flags = VRING_DESC_F_WRITE;
    g_ctrl_desc[1].next = 0;

    /* Chain 2 (Desc 2 -> Desc 3): Resource flush */
    g_ctrl_desc[2].addr = g_cmd_buf_phys + 256;
    g_ctrl_desc[2].len = (uint32_t)sizeof(*flush);
    g_ctrl_desc[2].flags = VRING_DESC_F_NEXT;
    g_ctrl_desc[2].next = 3;

    g_ctrl_desc[3].addr = g_resp_buf_phys + 256;
    g_ctrl_desc[3].len = (uint32_t)sizeof(virtio_gpu_ctrl_hdr_t);
    g_ctrl_desc[3].flags = VRING_DESC_F_WRITE;
    g_ctrl_desc[3].next = 0;

    /* Enqueue both head descriptors (0 and 2) */
    uint16_t avail_idx = g_ctrl_avail->idx;
    g_ctrl_avail->ring[avail_idx % g_ctrl_qsize] = 0;
    g_ctrl_avail->ring[(avail_idx + 1) % g_ctrl_qsize] = 2;
    __asm__ volatile ("" ::: "memory");
    g_ctrl_avail->idx = avail_idx + 2;
    __asm__ volatile ("" ::: "memory");

    /* Single doorbell ring to trigger host batch processing */
    *g_ctrl_notify_addr = 0;

    /* Wait for host responses */
    uint16_t target_used = g_ctrl_last_used_idx + 2;
    uint32_t timeout = 50000000;
    while (g_ctrl_used->idx != target_used && --timeout > 0) {
        __asm__ volatile ("pause");
    }

    if (timeout == 0) {
        klog_warn("VirtIO-GPU: Batch flush timeout! (used=%u, target=%u)", g_ctrl_used->idx, target_used);
        g_ctrl_last_used_idx = g_ctrl_used->idx;
        return false;
    }

    g_ctrl_last_used_idx = g_ctrl_used->idx;
    return true;
}

void virtio_gpu_flush(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    if (!g_virtio_gpu_active)
        return;

    if (w == 0 || h == 0)
        return;

    if (x + w > g_screen_width)
        w = (x < g_screen_width) ? (g_screen_width - x) : 0;
    if (y + h > g_screen_height)
        h = (y < g_screen_height) ? (g_screen_height - y) : 0;

    if (w == 0 || h == 0)
        return;

    spinlock_acquire(&g_virtio_gpu_lock);

    /* 1. Transfer dirty rectangle to host 2D resource */
    virtio_gpu_transfer_to_host_2d_t xfer;
    memset(&xfer, 0, sizeof(xfer));
    xfer.hdr.type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D;
    xfer.resource_id = 1;
    xfer.offset = ((uint64_t)y * g_screen_pitch) + ((uint64_t)x * 4);
    xfer.r.x = x;
    xfer.r.y = y;
    xfer.r.width = w;
    xfer.r.height = h;

    /* 2. Flush resource to host display scanout */
    virtio_gpu_resource_flush_t flush;
    memset(&flush, 0, sizeof(flush));
    flush.hdr.type = VIRTIO_GPU_CMD_RESOURCE_FLUSH;
    flush.resource_id = 1;
    flush.r.x = x;
    flush.r.y = y;
    flush.r.width = w;
    flush.r.height = h;

    /* Batch both commands to minimize VM exit overhead */
    if (!virtio_gpu_send_batch_xfer_and_flush(&xfer, &flush)) {
        /* Fallback to sequential execution if batching failed */
        virtio_gpu_ctrl_hdr_t resp;
        virtio_gpu_send_command(&xfer, sizeof(xfer), NULL, 0, &resp, sizeof(resp));
        virtio_gpu_send_command(&flush, sizeof(flush), NULL, 0, &resp, sizeof(resp));
    }

    spinlock_release(&g_virtio_gpu_lock);
}

void virtio_gpu_blit(const uint32_t *src, size_t pitch_pixels, size_t dst_x, size_t dst_y, size_t w, size_t h) {
    if (!g_virtio_gpu_active || !g_fb_virt || !src)
        return;

    if (dst_x >= g_screen_width || dst_y >= g_screen_height)
        return;

    if (dst_x + w > g_screen_width)
        w = g_screen_width - dst_x;
    if (dst_y + h > g_screen_height)
        h = g_screen_height - dst_y;

    if (w == 0 || h == 0)
        return;

    size_t dst_stride = g_screen_width;
    for (size_t row = 0; row < h; row++) {
        const uint32_t *s = src + ((dst_y + row) * pitch_pixels) + dst_x;
        uint32_t *d = g_fb_virt + ((dst_y + row) * dst_stride) + dst_x;
        memcpy(d, s, w * sizeof(uint32_t));
    }

    virtio_gpu_flush(dst_x, dst_y, w, h);
}

bool virtio_gpu_is_active(void) {
    return g_virtio_gpu_active;
}

void virtio_gpu_get_resolution(uint32_t *w, uint32_t *h) {
    if (w) *w = g_screen_width;
    if (h) *h = g_screen_height;
}

void *virtio_gpu_get_framebuffer(void) {
    return (void *)g_fb_virt;
}

uint32_t virtio_gpu_get_pitch(void) {
    return g_screen_pitch;
}

bool virtio_gpu_has_virgl(void) {
    return g_has_virgl_3d;
}

uint32_t virtio_gpu_get_supported_capsets(void) {
    uint32_t mask = 0;
    if (g_capset_virgl.valid)
        mask |= (1 << VIRTIO_GPU_CAPSET_VIRGL);
    if (g_capset_virgl2.valid)
        mask |= (1 << VIRTIO_GPU_CAPSET_VIRGL2);
    return mask;
}

int virtio_gpu_get_caps(uint32_t cap_set_id, uint32_t cap_set_ver, void *out_caps, size_t max_size) {
    if (!out_caps || max_size == 0)
        return -1;

    virtio_gpu_capset_cache_t *cached = NULL;
    if (cap_set_id == VIRTIO_GPU_CAPSET_VIRGL && g_capset_virgl.valid) {
        cached = &g_capset_virgl;
    } else if (cap_set_id == VIRTIO_GPU_CAPSET_VIRGL2 && g_capset_virgl2.valid) {
        cached = &g_capset_virgl2;
    }

    if (!cached && (cap_set_id == VIRTIO_GPU_CAPSET_VIRGL || cap_set_id == VIRTIO_GPU_CAPSET_VIRGL2)) {
        virtio_gpu_get_capset_t get_cmd;
        memset(&get_cmd, 0, sizeof(get_cmd));
        get_cmd.hdr.type = VIRTIO_GPU_CMD_GET_CAPSET;
        get_cmd.capset_id = cap_set_id;
        get_cmd.capset_version = cap_set_ver;

        size_t req_resp_size = sizeof(virtio_gpu_ctrl_hdr_t) + max_size;
        if (req_resp_size > VGPU_SCRATCH_SIZE)
            req_resp_size = VGPU_SCRATCH_SIZE;

        uint8_t *resp_buf = (uint8_t *)kmalloc(req_resp_size);
        if (resp_buf) {
            memset(resp_buf, 0, req_resp_size);
            spinlock_acquire(&g_virtio_gpu_lock);
            bool ok = virtio_gpu_send_command(&get_cmd, sizeof(get_cmd), NULL, 0, resp_buf, req_resp_size);
            spinlock_release(&g_virtio_gpu_lock);

            if (ok) {
                virtio_gpu_ctrl_hdr_t *rh = (virtio_gpu_ctrl_hdr_t *)resp_buf;
                if (rh->type == VIRTIO_GPU_RESP_OK_CAPSET) {
                    virtio_gpu_capset_cache_t *cache = (cap_set_id == VIRTIO_GPU_CAPSET_VIRGL) ? &g_capset_virgl : &g_capset_virgl2;
                    cache->id = cap_set_id;
                    cache->version = cap_set_ver;
                    size_t data_len = req_resp_size - sizeof(virtio_gpu_ctrl_hdr_t);
                    cache->size = (data_len < sizeof(cache->data)) ? data_len : sizeof(cache->data);
                    memcpy(cache->data, resp_buf + sizeof(virtio_gpu_ctrl_hdr_t), cache->size);
                    cache->valid = true;
                    cached = cache;
                }
            }
            kfree(resp_buf);
        }
    }

    if (!cached)
        return -1;

    size_t copy_size = (max_size < cached->size) ? max_size : cached->size;
    memcpy(out_caps, cached->data, copy_size);
    return (int)copy_size;
}

uint32_t virtio_gpu_alloc_resource_id(void) {
    spinlock_acquire(&g_virtio_gpu_lock);
    uint32_t id = g_next_resource_id++;
    spinlock_release(&g_virtio_gpu_lock);
    return id;
}

uint64_t virtio_gpu_alloc_fence_id(void) {
    spinlock_acquire(&g_virtio_gpu_lock);
    uint64_t id = g_next_fence_id++;
    spinlock_release(&g_virtio_gpu_lock);
    return id;
}

static void virtio_gpu_init_capsets(uint32_t num_capsets) {
    memset(&g_capset_virgl, 0, sizeof(g_capset_virgl));
    memset(&g_capset_virgl2, 0, sizeof(g_capset_virgl2));

    for (uint32_t idx = 0; idx < num_capsets; idx++) {
        virtio_gpu_get_capset_info_t info_cmd;
        memset(&info_cmd, 0, sizeof(info_cmd));
        info_cmd.hdr.type = VIRTIO_GPU_CMD_GET_CAPSET_INFO;
        info_cmd.capset_index = idx;

        virtio_gpu_resp_capset_info_t info_resp;
        memset(&info_resp, 0, sizeof(info_resp));

        spinlock_acquire(&g_virtio_gpu_lock);
        bool ok = virtio_gpu_send_command(&info_cmd, sizeof(info_cmd), NULL, 0, &info_resp, sizeof(info_resp));
        spinlock_release(&g_virtio_gpu_lock);

        if (!ok || info_resp.hdr.type != VIRTIO_GPU_RESP_OK_CAPSET_INFO)
            continue;

        uint32_t cid = info_resp.capset_id;
        uint32_t cver = info_resp.capset_max_version;
        uint32_t csz = info_resp.capset_max_size;

        klog_info("VirtIO-GPU: Capset index %u -> id=%u, max_ver=%u, max_size=%u",
                  idx, cid, cver, csz);

        if (cid == VIRTIO_GPU_CAPSET_VIRGL || cid == VIRTIO_GPU_CAPSET_VIRGL2) {
            virtio_gpu_get_capset_t get_cmd;
            memset(&get_cmd, 0, sizeof(get_cmd));
            get_cmd.hdr.type = VIRTIO_GPU_CMD_GET_CAPSET;
            get_cmd.capset_id = cid;
            get_cmd.capset_version = cver;

            size_t req_resp_size = sizeof(virtio_gpu_ctrl_hdr_t) + csz;
            if (req_resp_size > VGPU_SCRATCH_SIZE)
                req_resp_size = VGPU_SCRATCH_SIZE;

            uint8_t *resp_buf = (uint8_t *)kmalloc(req_resp_size);
            if (!resp_buf)
                continue;

            memset(resp_buf, 0, req_resp_size);
            spinlock_acquire(&g_virtio_gpu_lock);
            bool get_ok = virtio_gpu_send_command(&get_cmd, sizeof(get_cmd), NULL, 0, resp_buf, req_resp_size);
            spinlock_release(&g_virtio_gpu_lock);

            if (get_ok) {
                virtio_gpu_ctrl_hdr_t *rh = (virtio_gpu_ctrl_hdr_t *)resp_buf;
                if (rh->type == VIRTIO_GPU_RESP_OK_CAPSET) {
                    virtio_gpu_capset_cache_t *cache = (cid == VIRTIO_GPU_CAPSET_VIRGL) ? &g_capset_virgl : &g_capset_virgl2;
                    cache->id = cid;
                    cache->version = cver;
                    cache->size = (csz < sizeof(cache->data)) ? csz : sizeof(cache->data);
                    memcpy(cache->data, resp_buf + sizeof(virtio_gpu_ctrl_hdr_t), cache->size);
                    cache->valid = true;
                    klog_info("VirtIO-GPU: Successfully cached Capset ID %u (%u bytes)", cid, (uint32_t)cache->size);
                }
            }
            kfree(resp_buf);
        }
    }
}

int virtio_gpu_create_context(uint32_t ctx_id, const char *debug_name) {
    if (!g_virtio_gpu_active && !g_common_cfg)
        return -1;

    virtio_gpu_ctx_create_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.hdr.type = VIRTIO_GPU_CMD_CTX_CREATE;
    cmd.hdr.ctx_id = ctx_id;
    if (debug_name) {
        size_t len = strlen(debug_name);
        if (len >= sizeof(cmd.debug_name)) len = sizeof(cmd.debug_name) - 1;
        memcpy(cmd.debug_name, debug_name, len);
        cmd.nlen = (uint32_t)len;
    }

    virtio_gpu_ctrl_hdr_t resp;
    spinlock_acquire(&g_virtio_gpu_lock);
    bool ok = virtio_gpu_send_command(&cmd, sizeof(cmd), NULL, 0, &resp, sizeof(resp));
    spinlock_release(&g_virtio_gpu_lock);
    return ok ? 0 : -1;
}

int virtio_gpu_destroy_context(uint32_t ctx_id) {
    if (!g_virtio_gpu_active || !g_has_virgl_3d)
        return -1;

    virtio_gpu_ctx_destroy_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.hdr.type = VIRTIO_GPU_CMD_CTX_DESTROY;
    cmd.hdr.ctx_id = ctx_id;

    virtio_gpu_ctrl_hdr_t resp;
    spinlock_acquire(&g_virtio_gpu_lock);
    bool ok = virtio_gpu_send_command(&cmd, sizeof(cmd), NULL, 0, &resp, sizeof(resp));
    spinlock_release(&g_virtio_gpu_lock);
    return ok ? 0 : -1;
}

int virtio_gpu_attach_resource_to_ctx(uint32_t ctx_id, uint32_t res_id) {
    if (!g_virtio_gpu_active || !g_has_virgl_3d)
        return -1;

    virtio_gpu_ctx_resource_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.hdr.type = VIRTIO_GPU_CMD_CTX_ATTACH_RESOURCE;
    cmd.hdr.ctx_id = (ctx_id > 0) ? ctx_id : 1;
    cmd.resource_id = res_id;

    virtio_gpu_ctrl_hdr_t resp;
    spinlock_acquire(&g_virtio_gpu_lock);
    bool ok = virtio_gpu_send_command(&cmd, sizeof(cmd), NULL, 0, &resp, sizeof(resp));
    spinlock_release(&g_virtio_gpu_lock);
    return ok ? 0 : -1;
}

int virtio_gpu_create_3d_resource(uint32_t res_id, uint32_t target, uint32_t format,
                                  uint32_t bind, uint32_t width, uint32_t height,
                                  uint32_t depth, uint32_t array_size, uint32_t last_level,
                                  uint32_t nr_samples, uint32_t flags, uintptr_t phys_addr, size_t size) {
    if (!g_virtio_gpu_active || !g_has_virgl_3d)
        return -1;

    virtio_gpu_resource_create_3d_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.hdr.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_3D;
    cmd.hdr.ctx_id = 1;
    cmd.resource_id = res_id;
    cmd.target = target;
    cmd.format = format;
    cmd.bind = bind;
    cmd.width = width;
    cmd.height = height;
    cmd.depth = depth ? depth : 1;
    cmd.array_size = array_size ? array_size : 1;
    cmd.last_level = last_level;
    cmd.nr_samples = nr_samples;
    cmd.flags = flags;

    virtio_gpu_ctrl_hdr_t resp;
    spinlock_acquire(&g_virtio_gpu_lock);
    if (!virtio_gpu_send_command(&cmd, sizeof(cmd), NULL, 0, &resp, sizeof(resp))) {
        spinlock_release(&g_virtio_gpu_lock);
        klog_warn("VirtIO-GPU: Failed to create 3D resource %u", res_id);
        return -1;
    }

    if (phys_addr && size > 0) {
        virtio_gpu_resource_attach_backing_t attach;
        memset(&attach, 0, sizeof(attach));
        attach.hdr.type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
        attach.resource_id = res_id;
        attach.nr_entries = 1;

        virtio_gpu_mem_entry_t mem;
        memset(&mem, 0, sizeof(mem));
        mem.addr = phys_addr;
        mem.length = (uint32_t)size;

        if (!virtio_gpu_send_command(&attach, sizeof(attach), &mem, sizeof(mem), &resp, sizeof(resp))) {
            spinlock_release(&g_virtio_gpu_lock);
            klog_warn("VirtIO-GPU: Failed to attach backing memory to 3D resource %u", res_id);
            return -1;
        }

        virtio_gpu_ctx_resource_t ctx_res;
        memset(&ctx_res, 0, sizeof(ctx_res));
        ctx_res.hdr.type = VIRTIO_GPU_CMD_CTX_ATTACH_RESOURCE;
        ctx_res.hdr.ctx_id = 1;
        ctx_res.resource_id = res_id;
        virtio_gpu_send_command(&ctx_res, sizeof(ctx_res), NULL, 0, &resp, sizeof(resp));
    }

    spinlock_release(&g_virtio_gpu_lock);
    return 0;
}

int virtio_gpu_submit_3d(uint32_t ctx_id, const void *cmd_buf, size_t size, uint64_t fence_id) {
    if (!g_virtio_gpu_active || !g_has_virgl_3d || !cmd_buf || size == 0)
        return -1;

    virtio_gpu_cmd_submit_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.hdr.type = VIRTIO_GPU_CMD_SUBMIT_3D;
    cmd.hdr.ctx_id = (ctx_id > 0) ? ctx_id : 1;
    if (fence_id > 0) {
        cmd.hdr.flags = VIRTIO_GPU_FLAG_FENCE;
        cmd.hdr.fence_id = fence_id;
    }
    cmd.size = (uint32_t)size;

    virtio_gpu_ctrl_hdr_t resp;
    spinlock_acquire(&g_virtio_gpu_lock);
    bool ok = virtio_gpu_send_command(&cmd, sizeof(cmd), cmd_buf, size, &resp, sizeof(resp));
    spinlock_release(&g_virtio_gpu_lock);

    return ok ? 0 : -1;
}

int virtio_gpu_transfer_to_host_3d(uint32_t ctx_id, uint32_t res_id, uint64_t offset,
                                   uint32_t level, uint32_t stride, uint32_t layer_stride,
                                   const virtio_gpu_box_t *box) {
    if (!g_virtio_gpu_active || !g_has_virgl_3d || !box)
        return -1;

    virtio_gpu_transfer_host_3d_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.hdr.type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_3D;
    cmd.hdr.ctx_id = (ctx_id > 0) ? ctx_id : 1;
    cmd.resource_id = res_id;
    cmd.offset = offset;
    cmd.level = level;
    cmd.stride = stride;
    cmd.layer_stride = layer_stride;
    memcpy(&cmd.box, box, sizeof(virtio_gpu_box_t));

    virtio_gpu_ctrl_hdr_t resp;
    spinlock_acquire(&g_virtio_gpu_lock);
    bool ok = virtio_gpu_send_command(&cmd, sizeof(cmd), NULL, 0, &resp, sizeof(resp));
    spinlock_release(&g_virtio_gpu_lock);
    return ok ? 0 : -1;
}

int virtio_gpu_transfer_from_host_3d(uint32_t ctx_id, uint32_t res_id, uint64_t offset,
                                     uint32_t level, uint32_t stride, uint32_t layer_stride,
                                     const virtio_gpu_box_t *box) {
    if (!g_virtio_gpu_active || !g_has_virgl_3d || !box)
        return -1;

    virtio_gpu_transfer_host_3d_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.hdr.type = VIRTIO_GPU_CMD_TRANSFER_FROM_HOST_3D;
    cmd.hdr.ctx_id = (ctx_id > 0) ? ctx_id : 1;
    cmd.resource_id = res_id;
    cmd.offset = offset;
    cmd.level = level;
    cmd.stride = stride;
    cmd.layer_stride = layer_stride;
    memcpy(&cmd.box, box, sizeof(virtio_gpu_box_t));

    virtio_gpu_ctrl_hdr_t resp;
    spinlock_acquire(&g_virtio_gpu_lock);
    bool ok = virtio_gpu_send_command(&cmd, sizeof(cmd), NULL, 0, &resp, sizeof(resp));
    spinlock_release(&g_virtio_gpu_lock);
    return ok ? 0 : -1;
}

bool virtio_gpu_init(pci_device_t *pci_dev) {
    if (!pci_dev)
        return false;

    /* Verify device IDs */
    if (pci_dev->vendor_id != VIRTIO_GPU_VENDOR_ID ||
        (pci_dev->device_id != VIRTIO_GPU_DEVICE_ID_MODERN && pci_dev->device_id != VIRTIO_GPU_DEVICE_ID_LEGACY)) {
        return false;
    }

    klog_info("VirtIO-GPU: Probing PCI device %02x:%02x.%d [0x%04x:0x%04x]...",
              pci_dev->bus, pci_dev->slot, pci_dev->func, pci_dev->vendor_id, pci_dev->device_id);

    g_vgpu_pci = pci_dev;
    pci_enable_bus_mastering(pci_dev);

    /* Enable Memory & I/O space access in PCI Command register */
    uint16_t pci_cmd = pci_read16(pci_dev->bus, pci_dev->slot, pci_dev->func, PCI_COMMAND);
    pci_write16(pci_dev->bus, pci_dev->slot, pci_dev->func, PCI_COMMAND,
                pci_cmd | PCI_COMMAND_IO | PCI_COMMAND_MMIO | PCI_COMMAND_MASTER);

    /* Parse VirtIO Modern PCI Capabilities */
    uint16_t status = pci_read16(pci_dev->bus, pci_dev->slot, pci_dev->func, PCI_STATUS);
    if (!(status & 0x10)) {
        klog_warn("VirtIO-GPU: Device lacks PCI capabilities list (legacy-only), bypassing VirtIO-GPU");
        return false;
    }

    uint8_t cap_ptr = pci_read8(pci_dev->bus, pci_dev->slot, pci_dev->func, 0x34) & ~3;
    uintptr_t bar_phys[6] = {0};
    uintptr_t bar_virt[6] = {0};

    for (int i = 0; i < 6; i++) {
        if (pci_dev->bar[i] && !pci_dev->bar_is_io[i]) {
            bar_phys[i] = pci_dev->bar[i] & ~0xFULL;
            bar_virt[i] = (uintptr_t)PHYS_TO_VIRT(bar_phys[i]);

            /* Map MMIO pages into kernel pagemap */
            extern pagemap_t g_kernel_pagemap;
            size_t map_pages = (pci_dev->bar_size[i] > 0) ? ((pci_dev->bar_size[i] + PAGE_SIZE - 1) / PAGE_SIZE) : 64;
            if (map_pages > 256) map_pages = 256;
            for (size_t p = 0; p < map_pages; p++) {
                vmm_map_page(&g_kernel_pagemap, bar_virt[i] + (p * PAGE_SIZE),
                             bar_phys[i] + (p * PAGE_SIZE),
                             VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | VMM_FLAG_CACHE_DISABLE);
            }
        }
    }

    while (cap_ptr) {
        uint8_t cap_id = pci_read8(pci_dev->bus, pci_dev->slot, pci_dev->func, cap_ptr);
        uint8_t cap_next = pci_read8(pci_dev->bus, pci_dev->slot, pci_dev->func, cap_ptr + 1);

        if (cap_id == 0x09) { /* PCI_CAP_ID_VNDR */
            uint8_t cfg_type = pci_read8(pci_dev->bus, pci_dev->slot, pci_dev->func, cap_ptr + 3);
            uint8_t bar = pci_read8(pci_dev->bus, pci_dev->slot, pci_dev->func, cap_ptr + 4);
            uint32_t offset = pci_read32(pci_dev->bus, pci_dev->slot, pci_dev->func, cap_ptr + 8);

            if (bar < 6 && bar_virt[bar]) {
                uintptr_t addr = bar_virt[bar] + offset;
                switch (cfg_type) {
                case VIRTIO_PCI_CAP_COMMON_CFG:
                    g_common_cfg = (volatile virtio_pci_common_cfg_t *)addr;
                    break;
                case VIRTIO_PCI_CAP_NOTIFY_CFG:
                    g_notify_base = (volatile uint8_t *)addr;
                    g_notify_mult = pci_read32(pci_dev->bus, pci_dev->slot, pci_dev->func, cap_ptr + 16);
                    break;
                case VIRTIO_PCI_CAP_DEVICE_CFG:
                    g_device_cfg = (volatile uint8_t *)addr;
                    break;
                default:
                    break;
                }
            }
        }
        cap_ptr = cap_next & ~3;
    }

    if (!g_common_cfg || !g_notify_base) {
        klog_warn("VirtIO-GPU: Failed to find required modern VirtIO PCI capabilities, falling back to Framebuffer");
        return false;
    }

    /* 1. Reset device */
    g_common_cfg->device_status = 0;
    __asm__ volatile ("" ::: "memory");

    /* 2. Acknowledge and Driver status */
    g_common_cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;
    __asm__ volatile ("" ::: "memory");

    /* 3. Feature negotiation */
    g_common_cfg->device_feature_select = 0;
    __asm__ volatile ("" ::: "memory");
    uint32_t feat_lo = g_common_cfg->device_feature;
    g_common_cfg->device_feature_select = 1;
    __asm__ volatile ("" ::: "memory");
    uint32_t feat_hi = g_common_cfg->device_feature;

    if (feat_lo & VIRTIO_GPU_F_VIRGL) {
        g_has_virgl_3d = true;
        klog_info("VirtIO-GPU: Virgl 3D hardware acceleration supported by host!");
    } else {
        g_has_virgl_3d = false;
        klog_info("VirtIO-GPU: Virgl 3D not supported by host (operating in 2D scanout mode)");
    }

    g_common_cfg->guest_feature_select = 0;
    g_common_cfg->guest_feature = feat_lo;
    g_common_cfg->guest_feature_select = 1;
    g_common_cfg->guest_feature = feat_hi;
    __asm__ volatile ("" ::: "memory");

    g_common_cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __asm__ volatile ("" ::: "memory");

    /* 4. Setup Control Queue (Queue 0) */
    g_common_cfg->queue_select = 0;
    __asm__ volatile ("" ::: "memory");
    g_ctrl_qsize = g_common_cfg->queue_size;

    if (g_ctrl_qsize == 0) {
        klog_warn("VirtIO-GPU: Queue 0 has size 0, falling back to Framebuffer");
        return false;
    }

    /* Allocate Virtqueue DMA buffers */
    uintptr_t ring_phys = 0;
    size_t ring_size = (sizeof(vring_desc_t) * g_ctrl_qsize) +
                       (sizeof(uint16_t) * (3 + g_ctrl_qsize)) + 4096 +
                       (sizeof(vring_used_elem_t) * g_ctrl_qsize) + 64;

    void *ring_virt = alloc_dma_zero(ring_size, &ring_phys);
    if (!ring_virt) {
        klog_warn("VirtIO-GPU: Failed to allocate virtqueue ring memory, falling back to Framebuffer");
        return false;
    }

    g_ctrl_desc = (vring_desc_t *)ring_virt;
    g_ctrl_avail = (vring_avail_t *)((uintptr_t)g_ctrl_desc + (sizeof(vring_desc_t) * g_ctrl_qsize));
    uintptr_t used_offset = ((uintptr_t)g_ctrl_avail + sizeof(uint16_t) * (3 + g_ctrl_qsize) + 4095) & ~4095ULL;
    g_ctrl_used = (vring_used_t *)used_offset;
    uintptr_t used_phys = ring_phys + (used_offset - (uintptr_t)ring_virt);
    uintptr_t avail_phys = ring_phys + ((uintptr_t)g_ctrl_avail - (uintptr_t)ring_virt);

    g_common_cfg->queue_desc_lo = (uint32_t)ring_phys;
    g_common_cfg->queue_desc_hi = (uint32_t)(ring_phys >> 32);
    g_common_cfg->queue_avail_lo = (uint32_t)avail_phys;
    g_common_cfg->queue_avail_hi = (uint32_t)(avail_phys >> 32);
    g_common_cfg->queue_used_lo = (uint32_t)used_phys;
    g_common_cfg->queue_used_hi = (uint32_t)(used_phys >> 32);

    uint16_t notify_off = g_common_cfg->queue_notify_off;
    g_ctrl_notify_addr = (volatile uint16_t *)(g_notify_base + (notify_off * g_notify_mult));

    g_common_cfg->queue_enable = 1;
    __asm__ volatile ("" ::: "memory");

    /* Allocate command and response DMA scratch buffers */
    g_cmd_buf = (uint8_t *)alloc_dma_zero(VGPU_SCRATCH_SIZE, &g_cmd_buf_phys);
    g_resp_buf = (uint8_t *)alloc_dma_zero(VGPU_SCRATCH_SIZE, &g_resp_buf_phys);
    g_extra_buf = (uint8_t *)alloc_dma_zero(VGPU_SCRATCH_SIZE, &g_extra_buf_phys);

    if (!g_cmd_buf || !g_resp_buf || !g_extra_buf) {
        klog_warn("VirtIO-GPU: Failed to allocate command DMA buffers, falling back to Framebuffer");
        return false;
    }

    /* 5. Set DRIVER_OK status */
    g_common_cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __asm__ volatile ("" ::: "memory");

    /* Query Host Display Info */
    virtio_gpu_ctrl_hdr_t get_disp_cmd;
    memset(&get_disp_cmd, 0, sizeof(get_disp_cmd));
    get_disp_cmd.type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO;

    virtio_gpu_resp_display_info_t disp_info;
    memset(&disp_info, 0, sizeof(disp_info));

    bool got_disp = virtio_gpu_send_command(&get_disp_cmd, sizeof(get_disp_cmd), NULL, 0, &disp_info, sizeof(disp_info));
    klog_info("VirtIO-GPU: Display query: host %ux%u (enabled=%d), bootloader %lux%lu",
              disp_info.pmodes[0].r.width, disp_info.pmodes[0].r.height, disp_info.pmodes[0].enabled,
              fb_get_width(), fb_get_height());

    /* Determine target display resolution:
     * 1. If host VirtIO-GPU display info provides a resolution, use it (e.g. 2560x1440 configured in QEMU).
     * 2. Otherwise, check Limine Framebuffer mode.
     * 3. Ensure a modern high-resolution widescreen layout of at least 1920x1080 (Full HD).
     */
    if (got_disp && disp_info.pmodes[0].enabled && disp_info.pmodes[0].r.width > 0 && disp_info.pmodes[0].r.height > 0) {
        g_screen_width = disp_info.pmodes[0].r.width;
        g_screen_height = disp_info.pmodes[0].r.height;
    } else if (framebuffer_is_available() && fb_get_width() > 0 && fb_get_height() > 0) {
        g_screen_width = (uint32_t)fb_get_width();
        g_screen_height = (uint32_t)fb_get_height();
    }

    if (g_screen_width < 1920 || g_screen_height < 1080) {
        g_screen_width = 1920;
        g_screen_height = 1080;
    }

    g_screen_pitch = g_screen_width * 4;
    g_fb_size = (size_t)g_screen_pitch * g_screen_height;
    g_fb_num_pages = (g_fb_size + PAGE_SIZE - 1) / PAGE_SIZE;

    /* Allocate Contiguous Scanout Framebuffer */
    g_fb_virt = (uint32_t *)alloc_dma_zero(g_fb_size, &g_fb_phys);
    if (!g_fb_virt) {
        klog_warn("VirtIO-GPU: Failed to allocate scanout framebuffer memory (%lu bytes), falling back to Framebuffer", g_fb_size);
        return false;
    }

    /* 6. Create 2D Resource (resource_id = 1, B8G8R8X8_UNORM) */
    virtio_gpu_resource_create_2d_t create_cmd;
    memset(&create_cmd, 0, sizeof(create_cmd));
    create_cmd.hdr.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
    create_cmd.resource_id = 1;
    create_cmd.format = VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM;
    create_cmd.width = g_screen_width;
    create_cmd.height = g_screen_height;

    virtio_gpu_ctrl_hdr_t resp;
    if (!virtio_gpu_send_command(&create_cmd, sizeof(create_cmd), NULL, 0, &resp, sizeof(resp))) {
        klog_warn("VirtIO-GPU: Failed to create 2D resource, falling back to Framebuffer");
        return false;
    }

    /* 7. Attach Backing Memory */
    virtio_gpu_resource_attach_backing_t attach_cmd;
    memset(&attach_cmd, 0, sizeof(attach_cmd));
    attach_cmd.hdr.type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
    attach_cmd.resource_id = 1;
    attach_cmd.nr_entries = 1;

    virtio_gpu_mem_entry_t mem_entry;
    memset(&mem_entry, 0, sizeof(mem_entry));
    mem_entry.addr = g_fb_phys;
    mem_entry.length = (uint32_t)g_fb_size;

    if (!virtio_gpu_send_command(&attach_cmd, sizeof(attach_cmd), &mem_entry, sizeof(mem_entry), &resp, sizeof(resp))) {
        klog_warn("VirtIO-GPU: Failed to attach backing memory, falling back to Framebuffer");
        return false;
    }

    /* 8. Bind Resource to Scanout 0 */
    virtio_gpu_set_scanout_t scanout_cmd;
    memset(&scanout_cmd, 0, sizeof(scanout_cmd));
    scanout_cmd.hdr.type = VIRTIO_GPU_CMD_SET_SCANOUT;
    scanout_cmd.scanout_id = 0;
    scanout_cmd.resource_id = 1;
    scanout_cmd.r.x = 0;
    scanout_cmd.r.y = 0;
    scanout_cmd.r.width = g_screen_width;
    scanout_cmd.r.height = g_screen_height;

    if (!virtio_gpu_send_command(&scanout_cmd, sizeof(scanout_cmd), NULL, 0, &resp, sizeof(resp))) {
        klog_warn("VirtIO-GPU: Failed to set scanout, falling back to Framebuffer");
        return false;
    }

    /* Copy existing console backbuffer if available to preserve bootsplash/logs */
    if (framebuffer_is_available() && fb_get_backbuffer_ptr()) {
        const uint32_t *bb = fb_get_backbuffer_ptr();
        size_t fb_w = fb_get_width();
        size_t fb_h = fb_get_height();
        size_t copy_w = (fb_w < g_screen_width) ? fb_w : g_screen_width;
        size_t copy_h = (fb_h < g_screen_height) ? fb_h : g_screen_height;
        for (size_t y = 0; y < copy_h; y++) {
            memcpy(&g_fb_virt[y * g_screen_width], &bb[y * fb_w], copy_w * sizeof(uint32_t));
        }
    } else {
        for (size_t i = 0; i < (g_screen_width * g_screen_height); i++) {
            g_fb_virt[i] = FB_COLOR_BG;
        }
    }

    g_virtio_gpu_active = true;
    virtio_gpu_flush(0, 0, g_screen_width, g_screen_height);

    if (g_has_virgl_3d) {
        uint32_t num_capsets = 0;
        if (g_device_cfg) {
            num_capsets = *(volatile uint32_t *)(g_device_cfg + 12);
        }
        if (num_capsets == 0) {
            num_capsets = 2; /* Probe capset 1 and 2 if unspecified */
        }
        klog_info("VirtIO-GPU: Probing %u capsets...", num_capsets);
        virtio_gpu_init_capsets(num_capsets);

        /* Create default 3D context (ctx_id = 1) */
        int ctx_res = virtio_gpu_create_context(1, "SzpontOS-Virgl");
        if (ctx_res == 0) {
            klog_info("VirtIO-GPU: Default 3D context 1 created successfully");
        } else {
            klog_warn("VirtIO-GPU: Failed to create default 3D context 1");
        }
    }

    klog_info("VirtIO-GPU: Hardware %s driver successfully initialized! (%ux%u, %u bpp, Scanout 0)",
              g_has_virgl_3d ? "3D (Virgl)" : "2D", g_screen_width, g_screen_height, 32);

    return true;
}

pci_device_t *virtio_gpu_get_pci_dev(void) {
    return g_vgpu_pci;
}
