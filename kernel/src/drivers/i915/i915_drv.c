/*
 * SzpontOS - Intel i915 DRM/KMS Graphics Driver Main Entry Point
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <drivers/i915/i915_drv.h>
#include <kernel/kprint.h>

bool i915_init(pci_device_t *pci_dev) {
    if (!pci_dev)
        return false;

    klog_info("i915: Initializing Intel Graphics DRM/KMS Driver...");

    /* Step 1: Probe PCI device and map BARs */
    if (!i915_pci_probe(pci_dev)) {
        klog_err("i915: PCI probe failed");
        return false;
    }

    i915_device_t *dev = i915_get_device();
    if (!dev) {
        klog_err("i915: Failed to obtain device state");
        return false;
    }

    /* Step 2: Initialize Global GTT (GGTT) */
    if (i915_gtt_init(dev) != 0) {
        klog_err("i915: GGTT initialization failed");
        return false;
    }

    /* Step 3: Initialize Graphics Execution Manager (GEM) */
    if (i915_gem_init(dev) != 0) {
        klog_err("i915: GEM initialization failed");
        return false;
    }

    /* Step 4: Initialize Command Streamers */
    /* Render Command Streamer (3D / Compute) */
    if (i915_ring_init(dev, I915_RING_RCS) != 0) {
        klog_warn("i915: RCS ring initialization failed");
    }

    /* Blitter Command Streamer (2D / Blt) */
    if (i915_ring_init(dev, I915_RING_BCS) != 0) {
        klog_warn("i915: BCS ring initialization failed");
    }

    /* Step 5: Initialize Display Engine */
    if (i915_display_init(dev) != 0) {
        klog_warn("i915: Display engine initialization failed");
    }

    dev->active = true;

    klog_info("i915: Intel Graphics DRM/KMS driver successfully loaded and active!");
    return true;
}
