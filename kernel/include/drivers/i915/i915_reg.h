/*
 * SzpontOS - Intel Graphics MMIO Register Definitions & Command Streamer Opcodes
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef SZPONTOS_DRIVERS_I915_REG_H
#define SZPONTOS_DRIVERS_I915_REG_H

#include <kernel/types.h>

/*
 * =========================================================================
 * Command Streamer Ring Buffer Registers (MMIO offsets)
 * =========================================================================
 */
#define RCS_RING_BASE           0x02000   /* Render Command Streamer (3D / Compute) */
#define VCS_RING_BASE           0x12000   /* Video Command Streamer (Media) */
#define BCS_RING_BASE           0x22000   /* Blitter Command Streamer (2D / Blt) */
#define VECS_RING_BASE          0x1A000   /* Video Enhancement Streamer */

#define RING_TAIL(base)         ((base) + 0x30)
#define RING_HEAD(base)         ((base) + 0x34)
#define RING_START(base)        ((base) + 0x38)
#define RING_CTL(base)          ((base) + 0x3C)
#define RING_HWS_PGA(base)      ((base) + 0x80)
#define RING_HWS_PGA_GEN6(base) ((base) + 0x2080)
#define RING_IMR(base)          ((base) + 0xA8)

#define RING_NR_PAGES(count)    (((count) - 1) << 12)
#define RING_REPORT_64K         (1 << 6)
#define RING_VALID              (1 << 0)
#define RING_WAIT               (1 << 11)
#define RING_WAIT_SEMAPHORE     (1 << 10)

/*
 * =========================================================================
 * Hardware Status Page (HWS) & Seqno Tracking
 * =========================================================================
 */
#define I915_GEM_HWS_INDEX      0x20
#define I915_HWS_PGA_SIZE       4096

/*
 * =========================================================================
 * Command Streamer Instructions (MI - Memory Interface Opcodes)
 * =========================================================================
 */
#define MI_NOOP                 0x00000000
#define MI_USER_INTERRUPT       (0x02 << 23)
#define MI_WAIT_FOR_EVENT       (0x03 << 23)
#define MI_FLUSH                (0x04 << 23)
#define MI_ARB_CHECK            (0x05 << 23)
#define MI_REPORT_HEAD          (0x07 << 23)
#define MI_ARB_ON_OFF           (0x08 << 23)
#define MI_BATCH_BUFFER_END     (0x0A << 23)
#define MI_STORE_DWORD_IMM      (0x20 << 23)
#define MI_STORE_DWORD_INDEX    (0x21 << 23)
#define MI_LOAD_REGISTER_IMM    (0x22 << 23)
#define MI_FLUSH_DW             (0x26 << 23)
#define MI_BATCH_BUFFER_START   (0x31 << 23)
#define MI_BATCH_BUFFER_START_GEN8 ((0x31 << 23) | (1 << 8) | 1)

/* MI_FLUSH flags */
#define MI_READ_FLUSH           (1 << 0)
#define MI_EXE_FLUSH            (1 << 1)
#define MI_NO_WRITE_FLUSH       (1 << 2)

/* MI_STORE_DWORD_INDEX flags */
#define MI_STORE_DWORD_INDEX_USE_GGTT (1 << 22)

/*
 * =========================================================================
 * Global GTT (GGTT) Page Table Entries (PTE)
 * =========================================================================
 */
/* Gen6 / Gen7 (32-bit PTEs located at MMIO + 2 MiB) */
#define GEN6_GTT_PTE_OFFSET     0x200000
#define GEN6_PTE_VALID          (1 << 0)
#define GEN6_PTE_CACHE_LLC      (1 << 1)
#define GEN6_PTE_UNCACHED       (1 << 2)

/* Gen8+ (64-bit PTEs located at MMIO + 8 MiB) */
#define GEN8_GTT_PTE_OFFSET     0x800000
#define GEN8_PAGE_PRESENT       (1ULL << 0)
#define GEN8_PAGE_RW            (1ULL << 1)
#define GEN8_PAGE_LLC           (1ULL << 7)

/*
 * =========================================================================
 * Power & Forcewake Registers
 * =========================================================================
 */
#define FORCEWAKE_GEN6          0xA188
#define FORCEWAKE_ACK_GEN6      0x130044
#define FORCEWAKE_MT            0xA188
#define FORCEWAKE_ACK_HSW       0x130044

/*
 * =========================================================================
 * Display Engine Registers (Pipe A / Plane A)
 * =========================================================================
 */
#define HTOTAL_A                0x60000
#define HBLANK_A                0x60004
#define HSYNC_A                 0x60008
#define VTOTAL_A                0x6000C
#define VBLANK_A                0x60010
#define VSYNC_A                 0x60014
#define PIPEASRC                0x6001C

#define DSPACNTR                0x70180
#define DSPALINOFF              0x70184
#define DSPSTRIDE               0x70188
#define DSPAPOS                 0x7018C
#define DSPASURF                0x7019C
#define PLANE_SURF_1_A          0x7019C

/* Display Control Bits */
#define DISPPLANE_ENABLE        (1 << 31)
#define DISPPLANE_PIXFORMAT_BGRX8888 (0x06 << 26)
#define DISPPLANE_PIXFORMAT_RGBX8888 (0x02 << 26)

/* Hardware Cursor */
#define CURACNTR                0x70080
#define CURABASE                0x70084
#define CURAPOS                 0x70088

#endif /* SZPONTOS_DRIVERS_I915_REG_H */
