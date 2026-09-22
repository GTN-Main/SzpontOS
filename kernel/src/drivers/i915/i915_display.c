/*
 * SzpontOS - Intel i915 Display Engine & KMS Modesetting Subsystem
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <drivers/i915/i915_drv.h>
#include <drivers/i915/i915_reg.h>
#include <drivers/framebuffer.h>
#include <kernel/kprint.h>
#include <kernel/string.h>

int i915_display_init(i915_device_t *dev) {
    if (!dev)
        return -1;

    /* Get initial resolution from Limine/GOP or fallback */
    dev->display_width = (uint32_t)fb_get_width();
    dev->display_height = (uint32_t)fb_get_height();
    dev->display_bpp = fb_get_bpp() ? fb_get_bpp() : 32;
    dev->display_pitch = (uint32_t)fb_get_pitch();

    if (dev->display_width == 0 || dev->display_height == 0) {
        dev->display_width = 1280;
        dev->display_height = 960;
        dev->display_bpp = 32;
        dev->display_pitch = dev->display_width * 4;
    }

    klog_info("i915: Display engine initialized (%ux%u @ %u bpp, pitch=%u)",
              dev->display_width, dev->display_height, dev->display_bpp, dev->display_pitch);

    return 0;
}

void i915_get_resolution(uint32_t *w, uint32_t *h) {
    i915_device_t *dev = i915_get_device();
    if (dev && dev->display_width && dev->display_height) {
        if (w) *w = dev->display_width;
        if (h) *h = dev->display_height;
    } else {
        if (w) *w = (uint32_t)fb_get_width();
        if (h) *h = (uint32_t)fb_get_height();
    }
}

int i915_display_set_mode(i915_device_t *dev, uint32_t width, uint32_t height, uint32_t gtt_offset, uint32_t pitch) {
    if (!dev)
        return -1;

    dev->display_width = width;
    dev->display_height = height;
    dev->display_pitch = pitch ? pitch : (width * 4);
    dev->current_fb_gtt_offset = gtt_offset;

    /*
     * NOTE: We preserve UEFI GOP display plane registers (DSPSTRIDE / DSPASURF)
     * on bare metal to prevent desynchronizing the panel pipeline or scanning
     * out un-snooped DRAM. Screen updates are flushed synchronously to the
     * GOP scanout framebuffer via fb_blit_from_buffer.
     */
    return 0;
}

int i915_display_page_flip(i915_device_t *dev, uint32_t gtt_offset) {
    if (!dev)
        return -1;

    dev->current_fb_gtt_offset = gtt_offset;
    return 0;
}

void i915_display_blit(const uint32_t *src, size_t pitch_pixels, size_t dst_x, size_t dst_y, size_t w, size_t h) {
    /* Flush damaged rectangle directly to physical UEFI GOP scanout buffer */
    fb_blit_from_buffer(src, pitch_pixels, dst_x, dst_y, w, h);
}

