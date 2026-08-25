#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

static char g_dlerror_buf[256] = {0};
static int g_has_dlerror = 0;

typedef struct {
    uint8_t e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} local_elf_ehdr_t;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} local_elf_phdr_t;

typedef struct {
    int64_t d_tag;
    union {
        uint64_t d_val;
        uint64_t d_ptr;
    } d_un;
} local_elf_dyn_t;

typedef struct {
    uint32_t st_name;
    uint8_t st_info;
    uint8_t st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
} local_elf_sym_t;

typedef struct dl_handle {
    char name[128];
    uintptr_t base_addr;
    local_elf_sym_t *symtab;
    size_t sym_count;
    char *strtab;
    size_t str_size;
} dl_handle_t;

#define MAX_DL_HANDLES 8
static dl_handle_t g_dl_handles[MAX_DL_HANDLES];
static size_t g_dl_count = 0;

static void set_dlerror(const char *msg) {
    if (msg) {
        strncpy(g_dlerror_buf, msg, sizeof(g_dlerror_buf) - 1);
        g_dlerror_buf[sizeof(g_dlerror_buf) - 1] = '\0';
        g_has_dlerror = 1;
    } else {
        g_has_dlerror = 0;
    }
}

char *dlerror(void) {
    if (!g_has_dlerror)
        return NULL;
    g_has_dlerror = 0;
    return g_dlerror_buf;
}

void *dlopen(const char *filename, int flags) {
    (void)flags;

    if (!filename) {
        /* Return handle to self/global */
        return (void *)&g_dl_handles[0];
    }

    /* Check if already loaded */
    for (size_t i = 0; i < g_dl_count; i++) {
        if (strcmp(g_dl_handles[i].name, filename) == 0) {
            return &g_dl_handles[i];
        }
    }

    if (g_dl_count >= MAX_DL_HANDLES) {
        set_dlerror("dlopen: maximum shared library handles reached");
        return NULL;
    }

    char path[256];
    if (filename[0] == '/') {
        strncpy(path, filename, sizeof(path) - 1);
    } else {
        snprintf(path, sizeof(path), "/lib/%s", filename);
    }

    int fd = open(path, O_RDONLY, 0);
    if (fd < 0) {
        snprintf(path, sizeof(path), "/usr/lib/%s", filename);
        fd = open(path, O_RDONLY, 0);
    }

    if (fd < 0) {
        set_dlerror("dlopen: file not found");
        return NULL;
    }

    local_elf_ehdr_t ehdr;
    if (read(fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr)) {
        close(fd);
        set_dlerror("dlopen: failed to read ELF header");
        return NULL;
    }

    /* Standard SO base in user space for runtime loaded modules */
    uintptr_t base_addr = 0x0000700000000000ULL + (g_dl_count * 0x0000000100000000ULL);

    dl_handle_t *h = &g_dl_handles[g_dl_count++];
    memset(h, 0, sizeof(dl_handle_t));
    strncpy(h->name, filename, sizeof(h->name) - 1);
    h->base_addr = base_addr;

    close(fd);
    set_dlerror(NULL);
    return h;
}

void *dlsym(void *handle, const char *symbol) {
    if (!symbol || !*symbol) {
        set_dlerror("dlsym: empty symbol name");
        return NULL;
    }

    dl_handle_t *h = (dl_handle_t *)handle;
    if (!h) {
        set_dlerror("dlsym: invalid handle");
        return NULL;
    }

    /* If handle has explicit symbol table */
    if (h->symtab && h->strtab) {
        for (size_t i = 0; i < h->sym_count; i++) {
            local_elf_sym_t *sym = &h->symtab[i];
            if (sym->st_name < h->str_size) {
                const char *name = h->strtab + sym->st_name;
                if (strcmp(name, symbol) == 0 && sym->st_shndx != 0) {
                    return (void *)(h->base_addr + sym->st_value);
                }
            }
        }
    }

    set_dlerror("dlsym: symbol not found");
    return NULL;
}

int dlclose(void *handle) {
    (void)handle;
    return 0;
}
