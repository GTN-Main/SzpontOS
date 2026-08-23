/*
 * SzpontOS - Kernel Module Loader & Linker (Szpont Kernel Object - .sko)
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <kernel/module.h>
#include <kernel/kprint.h>
#include <kernel/string.h>
#include <kernel/spinlock.h>
#include <mm/heap.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <fs/elf.h>

#define EPERM        1
#define ENOENT       2
#define ENOEXEC      8
#define ENOMEM      12
#define EBUSY       16
#define EEXIST      17
#define EINVAL      22
#define ENOSYS      38

#define SHT_PROGBITS 1
#define SHT_SYMTAB   2
#define SHT_STRTAB   3
#define SHT_RELA     4
#define SHT_NOBITS   8
#define SHF_WRITE    0x1
#define SHF_ALLOC    0x2
#define SHF_EXECINSTR 0x4
#define SHN_UNDEF    0
#define SHN_ABS      0xFFF1
#define SHN_COMMON   0xFFF2

#define MODULE_VADDR_START 0xFFFFFFFFC0000000ULL
#define MODULE_VADDR_END   0xFFFFFFFFE0000000ULL

static list_node_t g_modules = LIST_HEAD_INIT(g_modules);
static spinlock_t g_module_lock = SPINLOCK_INIT;
static uintptr_t g_module_current_vaddr = MODULE_VADDR_START;

void module_init_subsystem(void) {
    list_init(&g_modules);
    klog_info("Kernel Module Subsystem (.sko) initialized");
}

module_t *module_find(const char *name) {
    if (!name) return NULL;
    spinlock_acquire(&g_module_lock);
    list_node_t *pos;
    list_for_each(pos, &g_modules) {
        module_t *mod = container_of(pos, module_t, list);
        if (strcmp(mod->name, name) == 0) {
            spinlock_release(&g_module_lock);
            return mod;
        }
    }
    spinlock_release(&g_module_lock);
    return NULL;
}

void module_list_for_each(void (*cb)(module_t *mod, void *arg), void *arg) {
    if (!cb) return;
    spinlock_acquire(&g_module_lock);
    list_node_t *pos;
    list_for_each(pos, &g_modules) {
        module_t *mod = container_of(pos, module_t, list);
        cb(mod, arg);
    }
    spinlock_release(&g_module_lock);
}

static void parse_modinfo(module_t *mod, const char *info, size_t len) {
    size_t i = 0;
    while (i < len) {
        const char *entry = info + i;
        size_t entry_len = strlen(entry);
        if (entry_len == 0) { i++; continue; }

        if (strncmp(entry, "name=", 5) == 0 && mod->name[0] == '\0') {
            strncpy(mod->name, entry + 5, sizeof(mod->name) - 1);
        } else if (strncmp(entry, "author=", 7) == 0) {
            strncpy(mod->author, entry + 7, sizeof(mod->author) - 1);
        } else if (strncmp(entry, "description=", 12) == 0) {
            strncpy(mod->description, entry + 12, sizeof(mod->description) - 1);
        } else if (strncmp(entry, "license=", 8) == 0) {
            strncpy(mod->license, entry + 8, sizeof(mod->license) - 1);
        } else if (strncmp(entry, "version=", 8) == 0) {
            strncpy(mod->version, entry + 8, sizeof(mod->version) - 1);
        }
        i += entry_len + 1;
    }
}

int module_load(const void *image, size_t size, const char *args, module_t **out_mod) {
    if (!image || size < sizeof(Elf64_Ehdr)) {
        return -EINVAL;
    }

    const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)image;

    /* Validate ELF-64 relocatable object */
    if (*(const uint32_t *)ehdr->e_ident != ELF_MAGIC ||
        ehdr->e_ident[4] != ELFCLASS64 ||
        ehdr->e_ident[5] != ELFDATA2LSB ||
        ehdr->e_type != ET_REL ||
        ehdr->e_machine != EM_X86_64) {
        klog_err("[MODULE] Invalid ELF64 relocatable object format!");
        return -ENOEXEC;
    }

    const Elf64_Shdr *shdrs = (const Elf64_Shdr *)((uintptr_t)image + ehdr->e_shoff);
    if (ehdr->e_shstrndx >= ehdr->e_shnum) {
        return -ENOEXEC;
    }
    const char *shstrtab = (const char *)((uintptr_t)image + shdrs[ehdr->e_shstrndx].sh_offset);

    /* Locate symtab, strtab, and calculate memory layout */
    const Elf64_Shdr *symtab_shdr = NULL;
    const Elf64_Shdr *strtab_shdr = NULL;
    const char *modinfo_data = NULL;
    size_t modinfo_size = 0;

    size_t total_size = 0;
    uintptr_t *sec_offsets = (uintptr_t *)kzalloc(ehdr->e_shnum * sizeof(uintptr_t));
    if (!sec_offsets) return -ENOMEM;

    for (size_t i = 0; i < ehdr->e_shnum; i++) {
        const Elf64_Shdr *sh = &shdrs[i];
        const char *sec_name = shstrtab + sh->sh_name;

        if (sh->sh_type == SHT_SYMTAB) {
            symtab_shdr = sh;
            if (sh->sh_link < ehdr->e_shnum) {
                strtab_shdr = &shdrs[sh->sh_link];
            }
        } else if (strcmp(sec_name, ".modinfo") == 0) {
            modinfo_data = (const char *)((uintptr_t)image + sh->sh_offset);
            modinfo_size = sh->sh_size;
        }

        if (sh->sh_flags & SHF_ALLOC) {
            size_t align = sh->sh_addralign ? sh->sh_addralign : 8;
            total_size = ALIGN_UP(total_size, align);
            sec_offsets[i] = total_size;
            total_size += sh->sh_size;
        }
    }

    if (!symtab_shdr || !strtab_shdr) {
        kfree(sec_offsets);
        klog_err("[MODULE] Missing symbol or string table in .sko file!");
        return -ENOEXEC;
    }

    const Elf64_Sym *symtab = (const Elf64_Sym *)((uintptr_t)image + symtab_shdr->sh_offset);
    size_t sym_count = symtab_shdr->sh_size / sizeof(Elf64_Sym);
    const char *strtab = (const char *)((uintptr_t)image + strtab_shdr->sh_offset);

    /* Allocate higher-half top-2GB kernel pages for module memory */
    size_t aligned_total = ALIGN_UP(total_size, PAGE_SIZE);
    if (aligned_total == 0) aligned_total = PAGE_SIZE;
    size_t num_pages = aligned_total / PAGE_SIZE;

    spinlock_acquire(&g_module_lock);
    uintptr_t virt = g_module_current_vaddr;
    g_module_current_vaddr += aligned_total;
    spinlock_release(&g_module_lock);

    uintptr_t *phys_pages = (uintptr_t *)kzalloc(num_pages * sizeof(uintptr_t));
    if (!phys_pages) {
        kfree(sec_offsets);
        return -ENOMEM;
    }

    pagemap_t *kmap = vmm_get_kernel_pagemap();
    for (size_t i = 0; i < num_pages; i++) {
        phys_pages[i] = pmm_alloc_page();
        if (!phys_pages[i]) {
            for (size_t j = 0; j < i; j++) {
                vmm_unmap_page(kmap, virt + j * PAGE_SIZE);
                pmm_free_page(phys_pages[j]);
            }
            kfree(phys_pages);
            kfree(sec_offsets);
            return -ENOMEM;
        }
        vmm_map_page(kmap, virt + i * PAGE_SIZE, phys_pages[i], VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE);
    }
    memset((void *)virt, 0, aligned_total);

    /* Compute runtime section addresses and copy data */
    uintptr_t *sec_vaddrs = (uintptr_t *)kzalloc(ehdr->e_shnum * sizeof(uintptr_t));
    if (!sec_vaddrs) {
        for (size_t p = 0; p < num_pages; p++) {
            vmm_unmap_page(kmap, virt + p * PAGE_SIZE);
            pmm_free_page(phys_pages[p]);
        }
        kfree(phys_pages);
        kfree(sec_offsets);
        return -ENOMEM;
    }

    for (size_t i = 0; i < ehdr->e_shnum; i++) {
        const Elf64_Shdr *sh = &shdrs[i];
        if (sh->sh_flags & SHF_ALLOC) {
            sec_vaddrs[i] = virt + sec_offsets[i];
            if (sh->sh_type != SHT_NOBITS && sh->sh_size > 0) {
                memcpy((void *)sec_vaddrs[i], (const void *)((uintptr_t)image + sh->sh_offset), sh->sh_size);
            }
        }
    }

    /* Apply Relocations (SHT_RELA) */
    int reloc_error = 0;
    for (size_t i = 0; i < ehdr->e_shnum; i++) {
        const Elf64_Shdr *sh = &shdrs[i];
        if (sh->sh_type != SHT_RELA) continue;

        uint32_t target_sec = sh->sh_info;
        if (target_sec >= ehdr->e_shnum || !(shdrs[target_sec].sh_flags & SHF_ALLOC)) {
            continue;
        }

        uintptr_t target_base = sec_vaddrs[target_sec];
        const Elf64_Rela *relas = (const Elf64_Rela *)((uintptr_t)image + sh->sh_offset);
        size_t rela_count = sh->sh_size / sizeof(Elf64_Rela);

        for (size_t r = 0; r < rela_count; r++) {
            const Elf64_Rela *rela = &relas[r];
            uint32_t sym_idx = ELF64_R_SYM(rela->r_info);
            uint32_t r_type  = ELF64_R_TYPE(rela->r_info);
            int64_t  addend  = rela->r_addend;

            if (sym_idx >= sym_count) {
                reloc_error = -ENOEXEC;
                break;
            }

            const Elf64_Sym *sym = &symtab[sym_idx];
            uintptr_t S = 0;

            if (sym->st_shndx == SHN_UNDEF) {
                const char *sym_name = strtab + sym->st_name;
                S = ksym_lookup(sym_name);
                if (S == 0) {
                    klog_err("[MODULE] Unresolved external symbol '%s' in .sko module!", sym_name);
                    reloc_error = -ENOENT;
                    break;
                }
            } else if (sym->st_shndx == SHN_ABS) {
                S = sym->st_value;
            } else if (sym->st_shndx < ehdr->e_shnum) {
                S = sec_vaddrs[sym->st_shndx] + sym->st_value;
            }

            uintptr_t P = target_base + rela->r_offset;
            void *loc = (void *)P;

            switch (r_type) {
                case R_X86_64_NONE:
                    break;
                case R_X86_64_64:
                    *(uint64_t *)loc = S + addend;
                    break;
                case R_X86_64_32:
                    *(uint32_t *)loc = (uint32_t)(S + addend);
                    break;
                case R_X86_64_32S:
                    *(int32_t *)loc = (int32_t)(S + addend);
                    break;
                case R_X86_64_PC32:
                case R_X86_64_PLT32:
                    *(int32_t *)loc = (int32_t)(S + addend - P);
                    break;
                default:
                    klog_err("[MODULE] Unsupported relocation type %u at 0x%lx!", r_type, P);
                    reloc_error = -ENOSYS;
                    break;
            }

            if (reloc_error != 0) break;
        }

        if (reloc_error != 0) break;
    }

    if (reloc_error != 0) {
        for (size_t p = 0; p < num_pages; p++) {
            vmm_unmap_page(kmap, virt + p * PAGE_SIZE);
            pmm_free_page(phys_pages[p]);
        }
        kfree(phys_pages);
        kfree(sec_offsets);
        kfree(sec_vaddrs);
        return reloc_error;
    }

    /* Locate init_module and cleanup_module */
    int (*init_fn)(void) = NULL;
    void (*exit_fn)(void) = NULL;

    for (size_t i = 0; i < sym_count; i++) {
        const Elf64_Sym *sym = &symtab[i];
        if (sym->st_shndx == SHN_UNDEF || sym->st_shndx >= ehdr->e_shnum) continue;

        const char *name = strtab + sym->st_name;
        uintptr_t sym_addr = sec_vaddrs[sym->st_shndx] + sym->st_value;

        if (strcmp(name, "init_module") == 0) {
            init_fn = (int (*)(void))sym_addr;
        } else if (strcmp(name, "cleanup_module") == 0) {
            exit_fn = (void (*)(void))sym_addr;
        }
    }

    /* Allocate and fill module_t descriptor */
    module_t *mod = (module_t *)kzalloc(sizeof(module_t));
    if (!mod) {
        for (size_t p = 0; p < num_pages; p++) {
            vmm_unmap_page(kmap, virt + p * PAGE_SIZE);
            pmm_free_page(phys_pages[p]);
        }
        kfree(phys_pages);
        kfree(sec_offsets);
        kfree(sec_vaddrs);
        return -ENOMEM;
    }

    mod->base_addr = virt;
    mod->size = aligned_total;
    mod->phys_pages = phys_pages;
    mod->num_pages = num_pages;
    mod->refcnt = 0;
    mod->state = MODULE_STATE_COMING;
    mod->init = init_fn;
    mod->exit = exit_fn;

    if (modinfo_data && modinfo_size > 0) {
        parse_modinfo(mod, modinfo_data, modinfo_size);
    }

    if (mod->name[0] == '\0') {
        if (args && args[0] != '\0') {
            strncpy(mod->name, args, sizeof(mod->name) - 1);
        } else {
            ksnprintf(mod->name, sizeof(mod->name), "module_%lx", (virt & 0xFFFF));
        }
    }

    /* Check if module with same name already loaded */
    if (module_find(mod->name)) {
        klog_warn("[MODULE] Module '%s' is already loaded!", mod->name);
        for (size_t p = 0; p < num_pages; p++) {
            vmm_unmap_page(kmap, virt + p * PAGE_SIZE);
            pmm_free_page(phys_pages[p]);
        }
        kfree(phys_pages);
        kfree(mod);
        kfree(sec_offsets);
        kfree(sec_vaddrs);
        return -EEXIST;
    }

    /* Register module symbols marked in __ksymtab */
    for (size_t i = 0; i < ehdr->e_shnum; i++) {
        const Elf64_Shdr *sh = &shdrs[i];
        const char *sec_name = shstrtab + sh->sh_name;
        if (strcmp(sec_name, "__ksymtab") == 0 && (sh->sh_flags & SHF_ALLOC)) {
            kernel_symbol_t *syms = (kernel_symbol_t *)sec_vaddrs[i];
            size_t count = sh->sh_size / sizeof(kernel_symbol_t);
            for (size_t k = 0; k < count; k++) {
                if (syms[k].name && syms[k].addr) {
                    ksym_register(syms[k].name, syms[k].addr);
                }
            }
        }
    }

    /* Add to global module list */
    spinlock_acquire(&g_module_lock);
    list_add_tail(&g_modules, &mod->list);
    spinlock_release(&g_module_lock);

    /* Run module initialization function */
    if (mod->init) {
        int init_ret = mod->init();
        if (init_ret != 0) {
            klog_err("[MODULE] Module '%s' init_module() failed with code %d!", mod->name, init_ret);
            spinlock_acquire(&g_module_lock);
            list_remove(&mod->list);
            spinlock_release(&g_module_lock);
            ksym_unregister_range(mod->base_addr, mod->size);
            for (size_t p = 0; p < num_pages; p++) {
                vmm_unmap_page(kmap, virt + p * PAGE_SIZE);
                pmm_free_page(phys_pages[p]);
            }
            kfree(phys_pages);
            kfree(mod);
            kfree(sec_offsets);
            kfree(sec_vaddrs);
            return init_ret;
        }
    }

    mod->state = MODULE_STATE_LIVE;
    klog_info("[MODULE] Loaded module '%s' (%lu bytes) at 0x%lx [LIVE]", mod->name, mod->size, mod->base_addr);

    kfree(sec_offsets);
    kfree(sec_vaddrs);

    if (out_mod) *out_mod = mod;
    return 0;
}

int module_unload(const char *name, unsigned int flags) {
    (void)flags;
    if (!name) return -EINVAL;

    spinlock_acquire(&g_module_lock);
    module_t *mod = NULL;
    list_node_t *pos;
    list_for_each(pos, &g_modules) {
        module_t *m = container_of(pos, module_t, list);
        if (strcmp(m->name, name) == 0) {
            mod = m;
            break;
        }
    }

    if (!mod) {
        spinlock_release(&g_module_lock);
        return -ENOENT;
    }

    if (mod->refcnt > 0) {
        spinlock_release(&g_module_lock);
        klog_warn("[MODULE] Cannot unload module '%s': module is in use (refcnt=%d)", name, mod->refcnt);
        return -EBUSY;
    }

    mod->state = MODULE_STATE_GOING;
    list_remove(&mod->list);
    spinlock_release(&g_module_lock);

    /* Run cleanup function */
    if (mod->exit) {
        mod->exit();
    }

    ksym_unregister_range(mod->base_addr, mod->size);

    pagemap_t *kmap = vmm_get_kernel_pagemap();
    for (size_t p = 0; p < mod->num_pages; p++) {
        vmm_unmap_page(kmap, mod->base_addr + p * PAGE_SIZE);
        pmm_free_page(mod->phys_pages[p]);
    }
    kfree(mod->phys_pages);

    klog_info("[MODULE] Unloaded module '%s' successfully", mod->name);
    kfree(mod);

    return 0;
}
