#include <fs/elf.h>
#include <mm/heap.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <sched/process.h>
#include <sched/sched.h>
#include <kernel/string.h>
#include <kernel/kprint.h>

#define USER_STACK_BASE 0x00007FFFF0000000ULL
#define USER_STACK_SIZE (4 * 1024 * 1024)
#define SO_BASE_START 0x0000700000000000ULL
#define MAX_LOADED_SO 16

extern void arch_enter_user_mode(uintptr_t rip, uintptr_t rsp);

static void user_thread_trampoline(void) {
    thread_t *curr = sched_get_current_thread();
    if (!curr)
        return;
    arch_enter_user_mode(curr->user_entry, curr->user_stack);
}

static void elf_write_user_mem(pagemap_t *map, uintptr_t vaddr, const void *src, size_t len) {
    size_t written = 0;
    while (written < len) {
        uintptr_t cur_vaddr = vaddr + written;
        uintptr_t page_vaddr = ALIGN_DOWN(cur_vaddr, PAGE_SIZE);
        size_t page_off = cur_vaddr - page_vaddr;
        size_t chunk = PAGE_SIZE - page_off;
        if (chunk > (len - written))
            chunk = len - written;

        uintptr_t phys = vmm_virt_to_phys(map, page_vaddr);
        if (!phys) {
            phys = pmm_alloc_page();
            memset(PHYS_TO_VIRT(phys), 0, PAGE_SIZE);
            vmm_map_page(map, page_vaddr, phys, VMM_FLAG_USER | VMM_FLAG_WRITABLE | VMM_FLAG_PRESENT);
        }

        uint8_t *kptr = (uint8_t *)PHYS_TO_VIRT(phys) + page_off;
        memcpy(kptr, (const uint8_t *)src + written, chunk);
        written += chunk;
    }
}

static void elf_read_user_mem(pagemap_t *map, uintptr_t vaddr, void *dst, size_t len) {
    size_t read_bytes = 0;
    while (read_bytes < len) {
        uintptr_t cur_vaddr = vaddr + read_bytes;
        uintptr_t page_vaddr = ALIGN_DOWN(cur_vaddr, PAGE_SIZE);
        size_t page_off = cur_vaddr - page_vaddr;
        size_t chunk = PAGE_SIZE - page_off;
        if (chunk > (len - read_bytes))
            chunk = len - read_bytes;

        uintptr_t phys = vmm_virt_to_phys(map, page_vaddr);
        if (phys) {
            uint8_t *kptr = (uint8_t *)PHYS_TO_VIRT(phys) + page_off;
            memcpy((uint8_t *)dst + read_bytes, kptr, chunk);
        } else {
            memset((uint8_t *)dst + read_bytes, 0, chunk);
        }
        read_bytes += chunk;
    }
}

static int elf_load_segments(vfs_node_t *file, pagemap_t *map, uintptr_t base_vaddr, uintptr_t *out_memsz,
                             Elf64_Phdr **out_phdrs, size_t *out_phnum, uintptr_t *out_dyn_vaddr) {
    Elf64_Ehdr ehdr;
    if (file->ops->read(file, 0, sizeof(Elf64_Ehdr), &ehdr) != sizeof(Elf64_Ehdr)) {
        return -1;
    }

    if (ehdr.e_ident[0] != 0x7F || ehdr.e_ident[1] != 'E' || ehdr.e_ident[2] != 'L' || ehdr.e_ident[3] != 'F') {
        return -1;
    }

    if (ehdr.e_ident[4] != ELFCLASS64 || ehdr.e_machine != EM_X86_64) {
        return -1;
    }

    size_t phdr_size = ehdr.e_phnum * sizeof(Elf64_Phdr);
    Elf64_Phdr *phdrs = (Elf64_Phdr *)kmalloc(phdr_size);
    if (!phdrs || file->ops->read(file, ehdr.e_phoff, phdr_size, phdrs) != (ssize_t)phdr_size) {
        if (phdrs)
            kfree(phdrs);
        return -1;
    }

    uintptr_t max_vaddr = 0;
    uintptr_t dyn_vaddr = 0;

    for (size_t i = 0; i < ehdr.e_phnum; i++) {
        Elf64_Phdr *p = &phdrs[i];
        if (p->p_type == PT_DYNAMIC) {
            dyn_vaddr = base_vaddr + p->p_vaddr;
        }

        if (p->p_type != PT_LOAD)
            continue;

        uintptr_t seg_vaddr = base_vaddr + p->p_vaddr;
        uintptr_t vaddr_start = ALIGN_DOWN(seg_vaddr, PAGE_SIZE);
        uintptr_t vaddr_end = ALIGN_UP(seg_vaddr + p->p_memsz, PAGE_SIZE);
        size_t page_count = (vaddr_end - vaddr_start) / PAGE_SIZE;

        if (vaddr_end > max_vaddr) {
            max_vaddr = vaddr_end;
        }

        uint64_t vmm_flags = VMM_FLAG_USER | VMM_FLAG_PRESENT;
        if (p->p_flags & PF_W)
            vmm_flags |= VMM_FLAG_WRITABLE;
        if (!(p->p_flags & PF_X))
            vmm_flags |= VMM_FLAG_NO_EXECUTE;

        for (size_t pg = 0; pg < page_count; pg++) {
            uintptr_t vpage = vaddr_start + pg * PAGE_SIZE;
            uintptr_t ppage = vmm_virt_to_phys(map, vpage);
            if (!ppage) {
                ppage = pmm_alloc_page();
                memset(PHYS_TO_VIRT(ppage), 0, PAGE_SIZE);
                vmm_map_page(map, vpage, ppage, vmm_flags);
            }

            void *kptr = PHYS_TO_VIRT(ppage);

            uintptr_t seg_file_start = seg_vaddr;
            uintptr_t seg_file_end = seg_vaddr + p->p_filesz;

            uintptr_t page_file_start = MAX(vpage, seg_file_start);
            uintptr_t page_file_end = MIN(vpage + PAGE_SIZE, seg_file_end);

            if (page_file_start < page_file_end) {
                size_t copy_len = page_file_end - page_file_start;
                off_t file_offset = p->p_offset + (page_file_start - seg_vaddr);
                void *dst = (void *)((uintptr_t)kptr + (page_file_start - vpage));
                file->ops->read(file, file_offset, copy_len, dst);
            }
        }
    }

    if (out_memsz)
        *out_memsz = (max_vaddr > base_vaddr) ? (max_vaddr - base_vaddr) : 0;
    if (out_phdrs)
        *out_phdrs = phdrs;
    else
        kfree(phdrs);
    if (out_phnum)
        *out_phnum = ehdr.e_phnum;
    if (out_dyn_vaddr)
        *out_dyn_vaddr = dyn_vaddr;

    return 0;
}

static int elf_parse_dynamic(pagemap_t *map, uintptr_t dyn_vaddr, uintptr_t base_vaddr, elf_loaded_so_t *so) {
    if (!dyn_vaddr)
        return 0;

    Elf64_Dyn dyn;
    uintptr_t cur = dyn_vaddr;

    uintptr_t strtab_vaddr = 0;
    size_t strsz = 0;
    uintptr_t symtab_vaddr = 0;
    size_t syment = sizeof(Elf64_Sym);
    uintptr_t rela_vaddr = 0;
    size_t relasz = 0;
    uintptr_t jmprel_vaddr = 0;
    size_t pltrelsz = 0;

    while (1) {
        elf_read_user_mem(map, cur, &dyn, sizeof(Elf64_Dyn));
        if (dyn.d_tag == DT_NULL)
            break;

        switch (dyn.d_tag) {
        case DT_STRTAB:
            strtab_vaddr = base_vaddr + dyn.d_un.d_ptr;
            break;
        case DT_STRSZ:
            strsz = dyn.d_un.d_val;
            break;
        case DT_SYMTAB:
            symtab_vaddr = base_vaddr + dyn.d_un.d_ptr;
            break;
        case DT_SYMENT:
            syment = dyn.d_un.d_val ? dyn.d_un.d_val : sizeof(Elf64_Sym);
            break;
        case DT_RELA:
            rela_vaddr = base_vaddr + dyn.d_un.d_ptr;
            break;
        case DT_RELASZ:
            relasz = dyn.d_un.d_val;
            break;
        case DT_JMPREL:
            jmprel_vaddr = base_vaddr + dyn.d_un.d_ptr;
            break;
        case DT_PLTRELSZ:
            pltrelsz = dyn.d_un.d_val;
            break;
        case DT_INIT:
            so->init_func = base_vaddr + dyn.d_un.d_ptr;
            break;
        case DT_FINI:
            so->fini_func = base_vaddr + dyn.d_un.d_ptr;
            break;
        }
        cur += sizeof(Elf64_Dyn);
    }

    if (strtab_vaddr && strsz) {
        so->strtab = (char *)kmalloc(strsz);
        so->str_size = strsz;
        elf_read_user_mem(map, strtab_vaddr, so->strtab, strsz);
    }

    if (symtab_vaddr && syment) {
        /* Estimate symbol count by strtab if symtab precedes strtab or using typical size */
        size_t est_syms = (strtab_vaddr > symtab_vaddr) ? (strtab_vaddr - symtab_vaddr) / syment : 1024;
        if (est_syms == 0 || est_syms > 4096)
            est_syms = 1024;
        so->symtab = (Elf64_Sym *)kmalloc(est_syms * sizeof(Elf64_Sym));
        so->sym_count = est_syms;
        elf_read_user_mem(map, symtab_vaddr, so->symtab, est_syms * sizeof(Elf64_Sym));
    }

    if (rela_vaddr && relasz) {
        so->rela_count = relasz / sizeof(Elf64_Rela);
        so->rela = (Elf64_Rela *)kmalloc(relasz);
        elf_read_user_mem(map, rela_vaddr, so->rela, relasz);
    }

    if (jmprel_vaddr && pltrelsz) {
        so->jmprel_count = pltrelsz / sizeof(Elf64_Rela);
        so->jmprel = (Elf64_Rela *)kmalloc(pltrelsz);
        elf_read_user_mem(map, jmprel_vaddr, so->jmprel, pltrelsz);
    }

    return 0;
}

static uintptr_t elf_resolve_symbol(const char *name, elf_loaded_so_t *so_list, size_t so_count) {
    if (!name || !*name)
        return 0;

    for (size_t i = 0; i < so_count; i++) {
        elf_loaded_so_t *so = &so_list[i];
        if (!so->symtab || !so->strtab)
            continue;

        for (size_t s = 0; s < so->sym_count; s++) {
            Elf64_Sym *sym = &so->symtab[s];
            if (sym->st_name >= so->str_size)
                continue;

            const char *sym_name = so->strtab + sym->st_name;
            if (strcmp(sym_name, name) == 0) {
                if (sym->st_shndx != 0) { /* Defined symbol */
                    return so->base_vaddr + sym->st_value;
                }
            }
        }
    }
    return 0;
}

static int elf_apply_relocations(pagemap_t *map, elf_loaded_so_t *target, elf_loaded_so_t *so_list, size_t so_count) {
    if (!target)
        return -1;

    /* Process .rela.dyn */
    if (target->rela && target->rela_count) {
        for (size_t i = 0; i < target->rela_count; i++) {
            Elf64_Rela *rel = &target->rela[i];
            uint32_t type = ELF64_R_TYPE(rel->r_info);
            uint32_t sym_idx = ELF64_R_SYM(rel->r_info);
            uintptr_t dest_vaddr = target->base_vaddr + rel->r_offset;

            uintptr_t sym_val = 0;
            if (sym_idx != 0 && target->symtab && sym_idx < target->sym_count) {
                Elf64_Sym *sym = &target->symtab[sym_idx];
                if (sym->st_name < target->str_size) {
                    const char *sym_name = target->strtab + sym->st_name;
                    sym_val = elf_resolve_symbol(sym_name, so_list, so_count);
                    if (!sym_val && sym->st_shndx != 0) {
                        sym_val = target->base_vaddr + sym->st_value;
                    }
                }
            }

            uint64_t val = 0;
            switch (type) {
            case R_X86_64_NONE:
                break;
            case R_X86_64_RELATIVE:
                val = target->base_vaddr + rel->r_addend;
                elf_write_user_mem(map, dest_vaddr, &val, sizeof(uint64_t));
                break;
            case R_X86_64_GLOB_DAT:
            case R_X86_64_JUMP_SLOT:
                val = sym_val;
                elf_write_user_mem(map, dest_vaddr, &val, sizeof(uint64_t));
                break;
            case R_X86_64_64:
                val = sym_val + rel->r_addend;
                elf_write_user_mem(map, dest_vaddr, &val, sizeof(uint64_t));
                break;
            default:
                break;
            }
        }
    }

    /* Process .rela.plt */
    if (target->jmprel && target->jmprel_count) {
        for (size_t i = 0; i < target->jmprel_count; i++) {
            Elf64_Rela *rel = &target->jmprel[i];
            uint32_t type = ELF64_R_TYPE(rel->r_info);
            uint32_t sym_idx = ELF64_R_SYM(rel->r_info);
            uintptr_t dest_vaddr = target->base_vaddr + rel->r_offset;

            uintptr_t sym_val = 0;
            if (sym_idx != 0 && target->symtab && sym_idx < target->sym_count) {
                Elf64_Sym *sym = &target->symtab[sym_idx];
                if (sym->st_name < target->str_size) {
                    const char *sym_name = target->strtab + sym->st_name;
                    sym_val = elf_resolve_symbol(sym_name, so_list, so_count);
                    if (!sym_val && sym->st_shndx != 0) {
                        sym_val = target->base_vaddr + sym->st_value;
                    }
                }
            }

            uint64_t val = 0;
            switch (type) {
            case R_X86_64_JUMP_SLOT:
            case R_X86_64_GLOB_DAT:
                val = sym_val;
                elf_write_user_mem(map, dest_vaddr, &val, sizeof(uint64_t));
                break;
            case R_X86_64_RELATIVE:
                val = target->base_vaddr + rel->r_addend;
                elf_write_user_mem(map, dest_vaddr, &val, sizeof(uint64_t));
                break;
            default:
                break;
            }
        }
    }

    return 0;
}

int elf_load_binary(vfs_node_t *file, pagemap_t *map, uintptr_t *out_entry, uintptr_t *out_user_stack) {
    if (!file || !file->ops || !file->ops->read || !map)
        return -1;

    Elf64_Ehdr ehdr;
    if (file->ops->read(file, 0, sizeof(Elf64_Ehdr), &ehdr) != sizeof(Elf64_Ehdr)) {
        klog_error("ELF: Failed to read ELF header!");
        return -1;
    }

    uintptr_t exe_base = (ehdr.e_type == ET_DYN) ? 0x0000000000400000ULL : 0;
    uintptr_t dyn_vaddr = 0;
    uintptr_t exe_memsz = 0;
    Elf64_Phdr *exe_phdrs = NULL;
    size_t exe_phnum = 0;

    if (elf_load_segments(file, map, exe_base, &exe_memsz, &exe_phdrs, &exe_phnum, &dyn_vaddr) != 0) {
        klog_error("ELF: Failed to load executable segments!");
        return -1;
    }

    elf_loaded_so_t loaded_sos[MAX_LOADED_SO];
    memset(loaded_sos, 0, sizeof(loaded_sos));
    size_t loaded_so_count = 0;

    /* Register main executable in loaded_sos[0] */
    strncpy(loaded_sos[0].name, "main", sizeof(loaded_sos[0].name) - 1);
    loaded_sos[0].base_vaddr = exe_base;
    loaded_sos[0].mem_size = exe_memsz;
    elf_parse_dynamic(map, dyn_vaddr, exe_base, &loaded_sos[0]);
    loaded_so_count = 1;

    /* Check if main executable needs shared libraries */
    if (dyn_vaddr && loaded_sos[0].strtab) {
        uintptr_t cur_so_base = SO_BASE_START;
        Elf64_Dyn dyn;
        uintptr_t cur = dyn_vaddr;

        while (1) {
            elf_read_user_mem(map, cur, &dyn, sizeof(Elf64_Dyn));
            if (dyn.d_tag == DT_NULL)
                break;

            if (dyn.d_tag == DT_NEEDED && loaded_so_count < MAX_LOADED_SO) {
                const char *so_name = loaded_sos[0].strtab + dyn.d_un.d_val;
                klog_info("ELF: Resolving dynamic dependency '%s'...", so_name);

                char so_path[256];
                ksnprintf(so_path, sizeof(so_path), "/lib/%s", so_name);
                vfs_node_t *so_file = vfs_lookup(so_path);
                if (!so_file) {
                    ksnprintf(so_path, sizeof(so_path), "/usr/lib/%s", so_name);
                    so_file = vfs_lookup(so_path);
                }

                if (so_file) {
                    uintptr_t so_dyn_vaddr = 0;
                    uintptr_t so_memsz = 0;

                    if (elf_load_segments(so_file, map, cur_so_base, &so_memsz, NULL, NULL, &so_dyn_vaddr) == 0) {
                        elf_loaded_so_t *so_entry = &loaded_sos[loaded_so_count++];
                        strncpy(so_entry->name, so_name, sizeof(so_entry->name) - 1);
                        so_entry->base_vaddr = cur_so_base;
                        so_entry->mem_size = so_memsz;

                        elf_parse_dynamic(map, so_dyn_vaddr, cur_so_base, so_entry);
                        klog_info("ELF: Loaded shared library '%s' at 0x%016lx (Size: %lu KiB)", so_name, cur_so_base,
                                  (so_memsz + 1023) / 1024);

                        cur_so_base = ALIGN_UP(cur_so_base + so_memsz + PAGE_SIZE, 0x200000); /* 2MB alignment */
                    } else {
                        klog_error("ELF: Failed to load shared library '%s'!", so_path);
                    }
                } else {
                    klog_warn("ELF: Shared library '%s' not found in /lib or /usr/lib!", so_name);
                }
            }
            cur += sizeof(Elf64_Dyn);
        }
    }

    /* Apply relocations to all loaded libraries and main executable */
    for (size_t i = 0; i < loaded_so_count; i++) {
        elf_apply_relocations(map, &loaded_sos[i], loaded_sos, loaded_so_count);
    }

    /* Clean up temporary kernel heap buffers */
    for (size_t i = 0; i < loaded_so_count; i++) {
        if (loaded_sos[i].symtab)
            kfree(loaded_sos[i].symtab);
        if (loaded_sos[i].strtab)
            kfree(loaded_sos[i].strtab);
        if (loaded_sos[i].rela)
            kfree(loaded_sos[i].rela);
        if (loaded_sos[i].jmprel)
            kfree(loaded_sos[i].jmprel);
    }
    if (exe_phdrs)
        kfree(exe_phdrs);

    /* Allocate and map User Stack */
    size_t stack_pages = USER_STACK_SIZE / PAGE_SIZE;
    for (size_t pg = 0; pg < stack_pages; pg++) {
        uintptr_t vpage = USER_STACK_BASE + pg * PAGE_SIZE;
        uintptr_t ppage = pmm_alloc_page();
        memset(PHYS_TO_VIRT(ppage), 0, PAGE_SIZE);
        vmm_map_page(map, vpage, ppage, VMM_FLAG_USER | VMM_FLAG_WRITABLE | VMM_FLAG_PRESENT);
    }

    *out_entry = exe_base + ehdr.e_entry;
    *out_user_stack = USER_STACK_BASE + USER_STACK_SIZE - 16; /* 16-byte aligned */
    return 0;
}

process_t *elf_spawn(const char *path, const char *name) {
    vfs_node_t *file = vfs_lookup(path);
    if (!file) {
        klog_error("ELF: Binary not found at '%s'", path);
        return NULL;
    }

    process_t *proc = process_create(name ? name : path);
    uintptr_t entry = 0;
    uintptr_t user_stack = 0;

    if (elf_load_binary(file, proc->pagemap, &entry, &user_stack) != 0) {
        klog_error("ELF: Failed to load binary '%s'", path);
        return NULL;
    }

    /* Spawn thread in process with user_thread_trampoline */
    thread_t *t = thread_create(proc, user_thread_trampoline, true);
    t->user_entry = entry;
    t->user_stack = user_stack;

    klog_info("ELF: Spawned process '%s' (PID %d, Entry: 0x%016lx, Stack: 0x%016lx)", proc->name, proc->pid, entry,
              user_stack);
    return proc;
}
