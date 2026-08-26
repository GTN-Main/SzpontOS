/*
 * SzpontOS - ACPI (Advanced Configuration and Power Interface) Subsystem
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <drivers/acpi.h>
#include <limine.h>
#include <mm/vmm.h>
#include <mm/heap.h>
#include <kernel/kprint.h>
#include <kernel/string.h>

__attribute__((used, section(".requests"))) static volatile struct limine_rsdp_request g_rsdp_request = {
    .id = LIMINE_RSDP_REQUEST, .revision = 0};

static acpi_rsdp_t *g_rsdp = NULL;
static acpi_sdt_header_t *g_rsdt_or_xsdt = NULL;
static bool g_use_xsdt = false;
static acpi_fadt_t *g_fadt = NULL;
static acpi_madt_t *g_madt = NULL;

static void acpi_map_region(uintptr_t phys, size_t len) {
    uintptr_t start = phys & ~0xFFFULL;
    uintptr_t end = (phys + len + PAGE_SIZE - 1) & ~0xFFFULL;
    for (uintptr_t p = start; p < end; p += PAGE_SIZE) {
        vmm_map_page(&g_kernel_pagemap, (uintptr_t)PHYS_TO_VIRT(p), p, VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE);
    }
}

acpi_sdt_header_t *acpi_find_table(const char *signature) {
    if (!g_rsdt_or_xsdt || !signature)
        return NULL;

    size_t sig_len = strlen(signature);
    if (sig_len != 4)
        return NULL;

    if (g_use_xsdt) {
        acpi_xsdt_t *xsdt = (acpi_xsdt_t *)g_rsdt_or_xsdt;
        size_t count = (xsdt->header.length - sizeof(acpi_sdt_header_t)) / sizeof(uint64_t);
        for (size_t i = 0; i < count; i++) {
            uintptr_t phys = (uintptr_t)xsdt->tables[i];
            if (!phys)
                continue;

            acpi_map_region(phys, sizeof(acpi_sdt_header_t));
            acpi_sdt_header_t *tbl = (acpi_sdt_header_t *)PHYS_TO_VIRT(phys);
            if (memcmp(tbl->signature, signature, 4) == 0) {
                acpi_map_region(phys, tbl->length);
                return tbl;
            }
        }
    } else {
        acpi_rsdt_t *rsdt = (acpi_rsdt_t *)g_rsdt_or_xsdt;
        size_t count = (rsdt->header.length - sizeof(acpi_sdt_header_t)) / sizeof(uint32_t);
        for (size_t i = 0; i < count; i++) {
            uintptr_t phys = (uintptr_t)rsdt->tables[i];
            if (!phys)
                continue;

            acpi_map_region(phys, sizeof(acpi_sdt_header_t));
            acpi_sdt_header_t *tbl = (acpi_sdt_header_t *)PHYS_TO_VIRT(phys);
            if (memcmp(tbl->signature, signature, 4) == 0) {
                acpi_map_region(phys, tbl->length);
                return tbl;
            }
        }
    }

    return NULL;
}

bool acpi_has_8042_controller(void) {
    if (!g_fadt)
        return true; /* Default to true if FADT absent */

    /*
     * In ACPI 1.0 (rev 1) and ACPI 2.0 (rev 2), the 8042 bit in IA-PC Boot Architecture Flags
     * does not exist (reserved/zero). All legacy PCs and VMs (e.g. QEMU) are assumed to have 8042.
     * Only in ACPI 3.0+ (Revision >= 3 and table length >= 244) is Bit 1 defined as 8042 presence.
     */
    if (g_fadt->header.revision >= 3 && g_fadt->header.length >= 244) {
        return (g_fadt->ia_pc_boot_arch & (1 << 1)) != 0;
    }

    return true;
}

uint32_t acpi_get_lapic_address(void) {
    if (g_madt) {
        return g_madt->lapic_addr;
    }
    return 0xFEE00000;
}

uint32_t acpi_get_smi_cmd_port(void) {
    return g_fadt ? g_fadt->smi_cmd : 0;
}

uint8_t acpi_get_enable_cmd(void) {
    return g_fadt ? g_fadt->acpi_enable : 0;
}

uint32_t acpi_get_pm1a_cnt(void) {
    return g_fadt ? g_fadt->pm1a_cnt_blk : 0;
}

void acpi_init(void) {
    if (!g_rsdp_request.response || !g_rsdp_request.response->address) {
        klog_warn("ACPI: Bootloader did not provide RSDP address");
        return;
    }

    uintptr_t rsdp_phys = (uintptr_t)g_rsdp_request.response->address;
    if (rsdp_phys >= g_hhdm_base) {
        rsdp_phys -= g_hhdm_base;
    }

    /* Map RSDP page */
    acpi_map_region(rsdp_phys, sizeof(acpi_xsdp_t));
    g_rsdp = (acpi_rsdp_t *)PHYS_TO_VIRT(rsdp_phys);

    /* Verify RSDP signature ("RSD PTR ") */
    if (memcmp(g_rsdp->signature, "RSD PTR ", 8) != 0) {
        klog_error("ACPI: Invalid RSDP signature!");
        return;
    }

    /* Check Revision: 0 = ACPI 1.0 (RSDT), 2 = ACPI 2.0+ (XSDT) */
    if (g_rsdp->revision >= 2) {
        acpi_xsdp_t *xsdp = (acpi_xsdp_t *)g_rsdp;
        if (xsdp->xsdt_addr) {
            uintptr_t xsdt_phys = (uintptr_t)xsdp->xsdt_addr;
            acpi_map_region(xsdt_phys, sizeof(acpi_sdt_header_t));
            acpi_sdt_header_t *hdr = (acpi_sdt_header_t *)PHYS_TO_VIRT(xsdt_phys);
            acpi_map_region(xsdt_phys, hdr->length);
            g_rsdt_or_xsdt = hdr;
            g_use_xsdt = true;
            klog_info("ACPI: ACPI 2.0+ XSDT found at phys 0x%016lx", (unsigned long)xsdt_phys);
        }
    }

    if (!g_rsdt_or_xsdt && g_rsdp->rsdt_addr) {
        uintptr_t rsdt_phys = (uintptr_t)g_rsdp->rsdt_addr;
        acpi_map_region(rsdt_phys, sizeof(acpi_sdt_header_t));
        acpi_sdt_header_t *hdr = (acpi_sdt_header_t *)PHYS_TO_VIRT(rsdt_phys);
        acpi_map_region(rsdt_phys, hdr->length);
        g_rsdt_or_xsdt = hdr;
        g_use_xsdt = false;
        klog_info("ACPI: ACPI 1.0 RSDT found at phys 0x%08x", g_rsdp->rsdt_addr);
    }

    if (!g_rsdt_or_xsdt) {
        klog_error("ACPI: Neither XSDT nor RSDT could be resolved!");
        return;
    }

    /* Find and cache FADT */
    g_fadt = (acpi_fadt_t *)acpi_find_table("FACP");
    if (g_fadt) {
        char oem[7] = {0};
        memcpy(oem, g_fadt->header.oem_id, 6);
        bool has_8042 = acpi_has_8042_controller();
        klog_info("ACPI: FADT (FACP) found (OEM: %s, 8042 PS/2 Present: %s)", oem,
                  has_8042 ? "YES" : "NO (USB/xHCI native platform)");
    }

    /* Find and cache MADT (APIC) */
    g_madt = (acpi_madt_t *)acpi_find_table("APIC");
    if (g_madt) {
        klog_info("ACPI: MADT (APIC) found (LAPIC phys: 0x%08x)", g_madt->lapic_addr);
    }

    /* Find HPET */
    acpi_hpet_t *hpet = (acpi_hpet_t *)acpi_find_table("HPET");
    if (hpet) {
        klog_info("ACPI: HPET table found");
    }

    /* Find MCFG (PCIe) */
    acpi_mcfg_t *mcfg = (acpi_mcfg_t *)acpi_find_table("MCFG");
    if (mcfg) {
        klog_info("ACPI: MCFG PCIe MMCONFIG table found");
    }

    klog_info("ACPI: Hardware configuration parsed successfully!");
}
