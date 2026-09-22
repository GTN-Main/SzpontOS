/*
 * SzpontOS - Intel Graphics PCI IDs and Generation Classification
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef SZPONTOS_DRIVERS_I915_PCIIDS_H
#define SZPONTOS_DRIVERS_I915_PCIIDS_H

#include <kernel/types.h>

#define INTEL_VENDOR_ID 0x8086

typedef enum intel_gen {
    INTEL_GEN_UNKNOWN = 0,
    INTEL_GEN3        = 3,   /* GMA 900, 950, 3100 */
    INTEL_GEN4        = 4,   /* GMA X3000, X3100, 4500HD */
    INTEL_GEN5        = 5,   /* Ironlake (Arrandale / Clarkdale) */
    INTEL_GEN6        = 6,   /* Sandy Bridge */
    INTEL_GEN7        = 7,   /* Ivy Bridge, Baytrail */
    INTEL_GEN7_5      = 75,  /* Haswell */
    INTEL_GEN8        = 8,   /* Broadwell, Braswell */
    INTEL_GEN9        = 9,   /* Skylake, Kaby Lake, Coffee Lake */
    INTEL_GEN11       = 11,  /* Ice Lake, Elkhart Lake */
    INTEL_GEN12       = 12,  /* Tiger Lake, Alder Lake, DG1, Arc */
} intel_gen_t;

/* Helper to classify PCI device ID into Intel GPU Generation */
static inline intel_gen_t i915_get_device_gen(uint16_t dev_id) {
    /* Gen 3 */
    if (dev_id == 0x2582 || dev_id == 0x2592 || dev_id == 0x2772 ||
        dev_id == 0x27A2 || dev_id == 0x27AE || dev_id == 0x29B2 ||
        dev_id == 0x29C2 || dev_id == 0x29D2) {
        return INTEL_GEN3;
    }

    /* Gen 4 */
    if (dev_id == 0x29A2 || dev_id == 0x2A02 || dev_id == 0x2A12 ||
        dev_id == 0x2972 || dev_id == 0x2982 || dev_id == 0x2992 ||
        (dev_id >= 0x2E02 && dev_id <= 0x2E92)) {
        return INTEL_GEN4;
    }

    /* Gen 5 (Ironlake) */
    if (dev_id == 0x0042 || dev_id == 0x0046) {
        return INTEL_GEN5;
    }

    /* Gen 6 (Sandy Bridge) */
    if ((dev_id >= 0x0102 && dev_id <= 0x0126) || dev_id == 0x010A) {
        return INTEL_GEN6;
    }

    /* Gen 7 (Ivy Bridge & Baytrail) */
    if ((dev_id >= 0x0152 && dev_id <= 0x016A) ||
        (dev_id >= 0x0F30 && dev_id <= 0x0F33)) {
        return INTEL_GEN7;
    }

    /* Gen 7.5 (Haswell) */
    if ((dev_id >= 0x0402 && dev_id <= 0x042A) ||
        (dev_id >= 0x0A02 && dev_id <= 0x0A2E) ||
        (dev_id >= 0x0D02 && dev_id <= 0x0D2E)) {
        return INTEL_GEN7_5;
    }

    /* Gen 8 (Broadwell & Braswell/Cherryview) */
    if ((dev_id >= 0x1602 && dev_id <= 0x162E) ||
        (dev_id >= 0x22B0 && dev_id <= 0x22B3)) {
        return INTEL_GEN8;
    }

    /* Gen 9 (Skylake, Kaby Lake, Coffee Lake, Apollo Lake, Gemini Lake) */
    if ((dev_id >= 0x1902 && dev_id <= 0x193D) ||
        (dev_id >= 0x5902 && dev_id <= 0x5927) ||
        (dev_id >= 0x3E90 && dev_id <= 0x3EA0) ||
        (dev_id >= 0x9BA0 && dev_id <= 0x9BC8) ||
        (dev_id >= 0x5A84 && dev_id <= 0x5A85) ||
        (dev_id >= 0x3184 && dev_id <= 0x3185)) {
        return INTEL_GEN9;
    }

    /* Gen 11 (Ice Lake) */
    if ((dev_id >= 0x8A50 && dev_id <= 0x8A5D) ||
        dev_id == 0x4500 || dev_id == 0x4571) {
        return INTEL_GEN11;
    }

    /* Gen 12 (Tiger Lake, Alder Lake, DG1, Arc) */
    if ((dev_id >= 0x9A40 && dev_id <= 0x9AF0) ||
        (dev_id >= 0x4680 && dev_id <= 0x4693) ||
        (dev_id >= 0x4905 && dev_id <= 0x4908) ||
        (dev_id >= 0x5690 && dev_id <= 0x56C2)) {
        return INTEL_GEN12;
    }

    /* Default fallback for recognized Intel graphics class: assume Gen7+ Sandy/Ivy/Haswell compatible */
    return INTEL_GEN7;
}

static inline const char *i915_get_device_name(uint16_t dev_id) {
    intel_gen_t gen = i915_get_device_gen(dev_id);
    switch (gen) {
        case INTEL_GEN3:   return "Intel GMA 900/950 (Gen3)";
        case INTEL_GEN4:   return "Intel GMA X3000/4500 (Gen4)";
        case INTEL_GEN5:   return "Intel Ironlake HD Graphics (Gen5)";
        case INTEL_GEN6:   return "Intel HD Graphics 2000/3000 (Sandy Bridge Gen6)";
        case INTEL_GEN7:   return "Intel HD Graphics 2500/4000 (Ivy Bridge Gen7)";
        case INTEL_GEN7_5: return "Intel HD Graphics 4400/4600 (Haswell Gen7.5)";
        case INTEL_GEN8:   return "Intel HD Graphics 5500/6000 (Broadwell Gen8)";
        case INTEL_GEN9:   return "Intel HD/UHD Graphics 500/600 series (Gen9)";
        case INTEL_GEN11:  return "Intel Iris Plus Graphics (Gen11)";
        case INTEL_GEN12:  return "Intel Iris Xe / Arc Graphics (Gen12)";
        default:           return "Intel Integrated Graphics";
    }
}

#endif /* SZPONTOS_DRIVERS_I915_PCIIDS_H */
