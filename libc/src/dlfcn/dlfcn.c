/*
 * SzpontOS Libc - Dynamic Linker & Dynamic Loading (dlfcn)
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <poll.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>

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
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
} local_elf_shdr_t;

typedef struct {
    uint32_t st_name;
    uint8_t st_info;
    uint8_t st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
} local_elf_sym_t;

typedef struct {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t  r_addend;
} local_elf_rela_t;

#define ELF64_R_SYM(i)    ((uint64_t)(i) >> 32)
#define ELF64_R_TYPE(i)   ((uint32_t)(i))

#define R_X86_64_NONE      0
#define R_X86_64_64        1
#define R_X86_64_COPY      5
#define R_X86_64_GLOB_DAT  6
#define R_X86_64_JUMP_SLOT 7
#define R_X86_64_RELATIVE  8
#define R_X86_64_DTPMOD64  16
#define R_X86_64_DTPOFF64  17
#define R_X86_64_TPOFF64   18

#define PT_LOAD   1
#define PT_DYNAMIC 2
#define PT_TLS    7
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHT_RELA   4
#define SHT_DYNAMIC 6
#define SHT_DYNSYM 11

#define DT_NULL       0
#define DT_NEEDED     1
#define DT_PLTRELSZ   2
#define DT_PLTGOT     3
#define DT_HASH       4
#define DT_STRTAB     5
#define DT_SYMTAB     6
#define DT_RELA       7
#define DT_RELASZ     8
#define DT_RELAENT    9
#define DT_STRSZ      10
#define DT_SYMENT     11
#define DT_INIT       12
#define DT_FINI       13
#define DT_SONAME     14
#define DT_RPATH      15
#define DT_SYMBOLIC   16
#define DT_REL        17
#define DT_RELSZ      18
#define DT_RELENT     19
#define DT_PLTREL     20
#define DT_DEBUG      21
#define DT_TEXTREL    22
#define DT_JMPREL     23
#define DT_INIT_ARRAY 25
#define DT_FINI_ARRAY 26
#define DT_INIT_ARRAYSZ 27
#define DT_FINI_ARRAYSZ 28

typedef struct {
    int64_t d_tag;
    union {
        uint64_t d_val;
        uint64_t d_ptr;
    } d_un;
} local_elf_dyn_t;

typedef struct dl_handle {
    char name[128];
    uintptr_t base_addr;
    local_elf_sym_t *symtab;
    size_t sym_count;
    char *strtab;
    size_t str_size;
    int inits_done;
    size_t tls_mod_id;
} dl_handle_t;

#define MAX_DL_HANDLES 64
static dl_handle_t g_dl_handles[MAX_DL_HANDLES];
static size_t g_dl_count = 0;
static uintptr_t g_next_dl_base = 0x0000720000000000ULL;
static int g_main_initialized = 0;

static int so_names_match(const char *name1, const char *name2) {
    if (!name1 || !name2) return 0;
    const char *b1 = strrchr(name1, '/'); b1 = b1 ? b1 + 1 : name1;
    const char *b2 = strrchr(name2, '/'); b2 = b2 ? b2 + 1 : name2;
    if (strcmp(b1, b2) == 0) return 1;

    char buf1[128], buf2[128];
    strncpy(buf1, b1, sizeof(buf1) - 1); buf1[sizeof(buf1) - 1] = '\0';
    strncpy(buf2, b2, sizeof(buf2) - 1); buf2[sizeof(buf2) - 1] = '\0';
    char *p1 = strstr(buf1, ".so");
    char *p2 = strstr(buf2, ".so");
    if (p1 && p2) {
        *(p1 + 3) = '\0';
        *(p2 + 3) = '\0';
        if (strcmp(buf1, buf2) == 0) return 1;
    }
    return 0;
}

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

/* Built-in system symbol map for libc and libdrm */
typedef struct {
    const char *name;
    void *addr;
} builtin_sym_t;

static const builtin_sym_t g_builtin_syms[] = {
    /* POSIX I/O & Process */
    {"open", (void *)open},
    {"close", (void *)close},
    {"read", (void *)read},
    {"write", (void *)write},
    {"lseek", (void *)lseek},
    {"ioctl", (void *)ioctl},
    {"poll", (void *)poll},
    {"dup", (void *)dup},
    {"dup2", (void *)dup2},
    {"mmap", (void *)mmap},
    {"munmap", (void *)munmap},
    {"fork", (void *)fork},
    {"execve", (void *)execve},
    {"exit", (void *)exit},
    {"getpid", (void *)getpid},
    {"sleep", (void *)sleep},
    {"nanosleep", (void *)nanosleep},
    {"clock_gettime", (void *)clock_gettime},
    {"gettimeofday", (void *)gettimeofday},
    {"time", (void *)time},
    {"errno", (void *)&errno},
    {"__errno_location", (void *)&errno},

    /* Memory & String */
    {"malloc", (void *)malloc},
    {"calloc", (void *)calloc},
    {"realloc", (void *)realloc},
    {"free", (void *)free},
    {"strdup", (void *)strdup},
    {"asprintf", (void *)asprintf},
    {"snprintf", (void *)snprintf},
    {"sprintf", (void *)sprintf},
    {"printf", (void *)printf},
    {"vsnprintf", (void *)vsnprintf},
    {"fprintf", (void *)fprintf},
    {"puts", (void *)puts},
    {"putchar", (void *)putchar},
    {"getenv", (void *)getenv},
    {"setenv", (void *)setenv},
    {"atoi", (void *)atoi},
    {"strtol", (void *)strtol},
    {"strtoul", (void *)strtoul},
    {"abort", (void *)abort},
    {"qsort", (void *)qsort},
    {"memcpy", (void *)memcpy},
    {"memmove", (void *)memmove},
    {"memset", (void *)memset},
    {"memcmp", (void *)memcmp},
    {"strlen", (void *)strlen},
    {"strcpy", (void *)strcpy},
    {"strncpy", (void *)strncpy},
    {"strcat", (void *)strcat},
    {"strncat", (void *)strncat},
    {"strcmp", (void *)strcmp},
    {"strncmp", (void *)strncmp},
    {"strcasecmp", (void *)strcasecmp},
    {"strncasecmp", (void *)strncasecmp},
    {"strchr", (void *)strchr},
    {"strrchr", (void *)strrchr},
    {"strstr", (void *)strstr},
    {"strtok", (void *)strtok},
    {"strerror", (void *)strerror},

    /* Dynamic loading */
    {"dlopen", (void *)dlopen},
    {"dlsym", (void *)dlsym},
    {"dlclose", (void *)dlclose},
    {"dlerror", (void *)dlerror},

    {NULL, NULL}
};

static void *find_builtin_symbol(const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; g_builtin_syms[i].name != NULL; i++) {
        if (strcmp(g_builtin_syms[i].name, name) == 0) {
            return g_builtin_syms[i].addr;
        }
    }
    return NULL;
}

static void *find_symbol_in_handle(dl_handle_t *h, const char *symbol) {
    if (!h || !h->symtab || !h->strtab)
        return NULL;

    for (size_t i = 0; i < h->sym_count; i++) {
        local_elf_sym_t *sym = &h->symtab[i];
        if (sym->st_name < h->str_size) {
            const char *name = h->strtab + sym->st_name;
            if (strcmp(name, symbol) == 0 && sym->st_shndx != 0) {
                return (void *)(h->base_addr + sym->st_value);
            }
        }
    }
    return NULL;
}

static local_elf_sym_t *find_defining_symbol(const char *sym_name, dl_handle_t **out_handle) {
    if (!sym_name || !*sym_name) return NULL;
    local_elf_sym_t *weak_sym = NULL;
    dl_handle_t *weak_h = NULL;

    for (size_t s = 0; s < g_dl_count; s++) {
        dl_handle_t *dh = &g_dl_handles[s];
        if (!dh->symtab || !dh->strtab) continue;
        for (size_t k = 0; k < dh->sym_count; k++) {
            if (dh->symtab[k].st_shndx != 0 && dh->symtab[k].st_name < dh->str_size) {
                if (strcmp(dh->strtab + dh->symtab[k].st_name, sym_name) == 0) {
                    uint8_t bind = dh->symtab[k].st_info >> 4;
                    if (bind != 2 /* STB_WEAK */) {
                        if (out_handle) *out_handle = dh;
                        return &dh->symtab[k];
                    } else if (!weak_sym) {
                        weak_sym = &dh->symtab[k];
                        weak_h = dh;
                    }
                }
            }
        }
    }
    if (weak_sym) {
        if (out_handle) *out_handle = weak_h;
        return weak_sym;
    }
    return NULL;
}

static void init_main_binary_symbols(void) {
    if (g_main_initialized)
        return;
    g_main_initialized = 1;

    int fd = -1;
    char path[256] = {0};

    /* 1. Try reading executable path from /proc/self/exe */
    int pfd = open("/proc/self/exe", O_RDONLY, 0);
    if (pfd >= 0) {
        ssize_t n = read(pfd, path, sizeof(path) - 1);
        close(pfd);
        if (n > 0) {
            path[n] = '\0';
            while (n > 0 && (path[n-1] == '\n' || path[n-1] == '\r' || path[n-1] == ' ')) {
                path[--n] = '\0';
            }
            if (path[0] == '/') {
                fd = open(path, O_RDONLY, 0);
            } else if (path[0] != '\0') {
                char full[256];
                snprintf(full, sizeof(full), "/usr/bin/%s", path);
                fd = open(full, O_RDONLY, 0);
                if (fd < 0) {
                    snprintf(full, sizeof(full), "/bin/%s", path);
                    fd = open(full, O_RDONLY, 0);
                }
            }
        }
    }

    /* 2. Fallback to common main binaries */
    if (fd < 0) {
        const char *main_paths[] = {"/usr/bin/Xorg", "/bin/Xorg", "/bin/xdemo", "/bin/sh", "/bin/init", NULL};
        for (int i = 0; main_paths[i] != NULL; i++) {
            fd = open(main_paths[i], O_RDONLY, 0);
            if (fd >= 0) {
                strncpy(path, main_paths[i], sizeof(path) - 1);
                break;
            }
        }
    }

    if (fd < 0) return;

    local_elf_ehdr_t ehdr;
    if (read(fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr)) {
        close(fd);
        return;
    }

    if (ehdr.e_ident[0] != 0x7f || ehdr.e_ident[1] != 'E' ||
        ehdr.e_ident[2] != 'L' || ehdr.e_ident[3] != 'F') {
        close(fd);
        return;
    }

    if (ehdr.e_shoff && ehdr.e_shnum) {
        size_t shdr_size = (size_t)ehdr.e_shentsize * ehdr.e_shnum;
        local_elf_shdr_t *shdrs = (local_elf_shdr_t *)malloc(shdr_size);
        if (shdrs) {
            lseek(fd, ehdr.e_shoff, SEEK_SET);
            if (read(fd, shdrs, shdr_size) == (ssize_t)shdr_size) {
                int sym_idx = -1;
                for (uint16_t i = 0; i < ehdr.e_shnum; i++) {
                    if (shdrs[i].sh_type == SHT_SYMTAB) {
                        sym_idx = i;
                        break;
                    }
                    if (shdrs[i].sh_type == SHT_DYNSYM && sym_idx < 0) {
                        sym_idx = i;
                    }
                }

                if (sym_idx >= 0) {
                    uint32_t str_idx = shdrs[sym_idx].sh_link;
                    size_t sym_count = shdrs[sym_idx].sh_size / sizeof(local_elf_sym_t);
                    local_elf_sym_t *symtab = (local_elf_sym_t *)malloc(shdrs[sym_idx].sh_size);
                    if (symtab) {
                        lseek(fd, shdrs[sym_idx].sh_offset, SEEK_SET);
                        read(fd, symtab, shdrs[sym_idx].sh_size);
                    }

                    char *strtab = NULL;
                    size_t str_size = 0;
                    if (str_idx < ehdr.e_shnum) {
                        str_size = shdrs[str_idx].sh_size;
                        strtab = (char *)malloc(str_size);
                        if (strtab) {
                            lseek(fd, shdrs[str_idx].sh_offset, SEEK_SET);
                            read(fd, strtab, str_size);
                        }
                    }

                    if (g_dl_count == 0) {
                        dl_handle_t *h = &g_dl_handles[g_dl_count++];
                        memset(h, 0, sizeof(dl_handle_t));
                        strncpy(h->name, "main", sizeof(h->name) - 1);
                        h->base_addr = 0; /* Fixed virtual address for executable */
                        h->symtab = symtab;
                        h->sym_count = sym_count;
                        h->strtab = strtab;
                        h->str_size = str_size;
                        h->inits_done = 1;
                        h->tls_mod_id = 1;
                    }
                }
            }
            free(shdrs);
        }
    }
    close(fd);

    /* Preload core system shared libraries so dynamic symbols are available to all modules */
    const char *preload_libs[] = {
        "libdrm.so",
        "libgbm.so",
        "libpixman-1.so",
        "libm.so",
        "libc.so",
        "libepoxy.so",
        "libEGL.so",
        "libGL.so",
        "libGLESv2.so",
        NULL
    };
    for (int i = 0; preload_libs[i]; i++) {
        dlopen(preload_libs[i], RTLD_GLOBAL);
    }
}

void *dlsym(void *handle, const char *symbol) {
    if (!symbol || !*symbol) {
        set_dlerror("dlsym: empty symbol name");
        return NULL;
    }

    init_main_binary_symbols();

    /* 1. Check built-in system symbols (libc / libdrm) */
    void *built_in = find_builtin_symbol(symbol);
    if (built_in) {
        return built_in;
    }

    if (handle == RTLD_DEFAULT || handle == NULL) {
        /* Search all loaded libraries */
        for (size_t i = 0; i < g_dl_count; i++) {
            void *ptr = find_symbol_in_handle(&g_dl_handles[i], symbol);
            if (ptr) return ptr;
        }
        set_dlerror("dlsym: symbol not found in global scope");
        return NULL;
    }

    dl_handle_t *h = (dl_handle_t *)handle;
    void *ptr = find_symbol_in_handle(h, symbol);
    if (ptr) {
        return ptr;
    }

    /* Fallback: search global scope */
    for (size_t i = 0; i < g_dl_count; i++) {
        ptr = find_symbol_in_handle(&g_dl_handles[i], symbol);
        if (ptr) return ptr;
    }

    set_dlerror("dlsym: symbol not found");
    return NULL;
}

void *dlopen(const char *filename, int flags) {
    (void)flags;

    init_main_binary_symbols();

    if (!filename) {
        /* Return global handle (RTLD_DEFAULT) */
        return RTLD_DEFAULT;
    }

    /* Check if already loaded */
    for (size_t i = 0; i < g_dl_count; i++) {
        if (so_names_match(g_dl_handles[i].name, filename)) {
            return &g_dl_handles[i];
        }
    }

    if (flags & RTLD_NOLOAD) {
        set_dlerror("dlopen: library not loaded");
        return NULL;
    }

    if (g_dl_count >= MAX_DL_HANDLES) {
        set_dlerror("dlopen: maximum shared library handles reached");
        return NULL;
    }

    char path[256];
    int fd = -1;

    if (filename[0] == '/') {
        strncpy(path, filename, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
        fd = open(path, O_RDONLY, 0);
        if (fd < 0) {
            char *so_pos = strstr(path, ".so.");
            if (so_pos) {
                *(so_pos + 3) = '\0';
                fd = open(path, O_RDONLY, 0);
            }
        }
        if (fd < 0) {
            /* Fallback: if absolute path does not exist (e.g. host build sysroot leakage),
             * extract basename and search in standard library directories */
            const char *base = strrchr(filename, '/');
            if (base && *(base + 1)) {
                filename = base + 1;
            }
        }
    }

    if (filename[0] != '/') {
        const char *search_dirs[] = {
            "/usr/lib/dri",
            "/lib/dri",
            "/usr/lib/xorg/modules/drivers",
            "/usr/lib/xorg/modules/input",
            "/usr/lib/xorg/modules/xlibre-25/drivers",
            "/usr/lib/xorg/modules/xlibre-25/input",
            "/usr/lib/xorg/modules/xlibre-25",
            "/usr/lib/xorg/modules",
            "/lib",
            "/usr/lib",
            NULL
        };

        char base_fname[128];
        strncpy(base_fname, filename, sizeof(base_fname) - 1);
        base_fname[sizeof(base_fname) - 1] = '\0';
        char *so_pos = strstr(base_fname, ".so.");
        if (so_pos) {
            *(so_pos + 3) = '\0';
        }

        for (int i = 0; search_dirs[i] != NULL; i++) {
            snprintf(path, sizeof(path), "%s/%s", search_dirs[i], filename);
            fd = open(path, O_RDONLY, 0);
            if (fd >= 0) break;

            if (so_pos) {
                snprintf(path, sizeof(path), "%s/%s", search_dirs[i], base_fname);
                fd = open(path, O_RDONLY, 0);
                if (fd >= 0) break;
            }

            snprintf(path, sizeof(path), "%s/%s.so", search_dirs[i], filename);
            fd = open(path, O_RDONLY, 0);
            if (fd >= 0) break;

            snprintf(path, sizeof(path), "%s/lib%s.so", search_dirs[i], filename);
            fd = open(path, O_RDONLY, 0);
            if (fd >= 0) break;

            snprintf(path, sizeof(path), "%s/%s_drv.so", search_dirs[i], filename);
            fd = open(path, O_RDONLY, 0);
            if (fd >= 0) break;
        }
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

    if (ehdr.e_ident[0] != 0x7f || ehdr.e_ident[1] != 'E' ||
        ehdr.e_ident[2] != 'L' || ehdr.e_ident[3] != 'F') {
        close(fd);
        set_dlerror("dlopen: invalid ELF magic");
        return NULL;
    }

    uintptr_t base_addr = g_next_dl_base;
    g_next_dl_base += 0x0000000010000000ULL; /* 256MB per module */

typedef struct {
    uintptr_t image;
    size_t filesz;
    size_t memsz;
    size_t align;
} szpont_tls_module_t;

extern szpont_tls_module_t __szpont_tls_modules[64];
extern size_t __szpont_tls_mod_count;

    size_t dl_mod_id = 0;
    uintptr_t dyn_vaddr = 0;
    size_t dyn_memsz = 0;
    (void)dyn_memsz;

    /* 1. Map PT_LOAD segments */
    if (ehdr.e_phoff && ehdr.e_phnum) {
        size_t phdr_size = (size_t)ehdr.e_phentsize * ehdr.e_phnum;
        local_elf_phdr_t *phdrs = (local_elf_phdr_t *)malloc(phdr_size);
        if (phdrs) {
            lseek(fd, ehdr.e_phoff, SEEK_SET);
            if (read(fd, phdrs, phdr_size) == (ssize_t)phdr_size) {
                for (uint16_t i = 0; i < ehdr.e_phnum; i++) {
                    if (phdrs[i].p_type == PT_LOAD) {
                        uintptr_t vaddr = base_addr + phdrs[i].p_vaddr;
                        uintptr_t page_vaddr = vaddr & ~0xFFFULL;
                        uintptr_t page_offset = vaddr - page_vaddr;
                        size_t page_len = (phdrs[i].p_memsz + page_offset + 0xFFFULL) & ~0xFFFULL;

                        mmap((void *)page_vaddr, page_len,
                             PROT_READ | PROT_WRITE | PROT_EXEC,
                             MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);

                        lseek(fd, phdrs[i].p_offset, SEEK_SET);
                        read(fd, (void *)vaddr, phdrs[i].p_filesz);
                        if (phdrs[i].p_memsz > phdrs[i].p_filesz) {
                            memset((void *)(vaddr + phdrs[i].p_filesz), 0,
                                   phdrs[i].p_memsz - phdrs[i].p_filesz);
                        }
                    } else if (phdrs[i].p_type == PT_DYNAMIC) {
                        dyn_vaddr = base_addr + phdrs[i].p_vaddr;
                        dyn_memsz = phdrs[i].p_memsz;
                    } else if (phdrs[i].p_type == PT_TLS) {
                        if (__szpont_tls_mod_count == 0) {
                            __szpont_tls_mod_count = 1;
                        }
                        if (__szpont_tls_mod_count < 64) {
                            dl_mod_id = __szpont_tls_mod_count++;
                            __szpont_tls_modules[dl_mod_id].image = base_addr + phdrs[i].p_vaddr;
                            __szpont_tls_modules[dl_mod_id].filesz = phdrs[i].p_filesz;
                            __szpont_tls_modules[dl_mod_id].memsz = phdrs[i].p_memsz;
                            __szpont_tls_modules[dl_mod_id].align = phdrs[i].p_align;
                        }
                    }
                }
            }
            free(phdrs);
        }
    }

    /* 2. Read Section Headers to extract Symbol & String Tables and Relocations */
    local_elf_sym_t *symtab = NULL;
    size_t sym_count = 0;
    char *strtab = NULL;
    size_t str_size = 0;

    local_elf_rela_t *rela_dyn = NULL;
    size_t rela_dyn_count = 0;
    local_elf_rela_t *rela_plt = NULL;
    size_t rela_plt_count = 0;

    if (ehdr.e_shoff && ehdr.e_shnum) {
        size_t shdr_size = (size_t)ehdr.e_shentsize * ehdr.e_shnum;
        local_elf_shdr_t *shdrs = (local_elf_shdr_t *)malloc(shdr_size);
        if (shdrs) {
            lseek(fd, ehdr.e_shoff, SEEK_SET);
            if (read(fd, shdrs, shdr_size) == (ssize_t)shdr_size) {
                int sym_idx = -1;
                char *shstrtab = NULL;
                if (ehdr.e_shstrndx < ehdr.e_shnum) {
                    shstrtab = (char *)malloc(shdrs[ehdr.e_shstrndx].sh_size);
                    if (shstrtab) {
                        lseek(fd, shdrs[ehdr.e_shstrndx].sh_offset, SEEK_SET);
                        read(fd, shstrtab, shdrs[ehdr.e_shstrndx].sh_size);
                    }
                }

                for (uint16_t i = 0; i < ehdr.e_shnum; i++) {
                    if (shdrs[i].sh_type == SHT_DYNSYM) {
                        sym_idx = i;
                    } else if (shdrs[i].sh_type == SHT_SYMTAB && sym_idx < 0) {
                        sym_idx = i;
                    } else if (shdrs[i].sh_type == SHT_DYNAMIC && dyn_vaddr == 0) {
                        dyn_vaddr = base_addr + shdrs[i].sh_addr;
                        dyn_memsz = shdrs[i].sh_size;
                    } else if (shdrs[i].sh_type == SHT_RELA) {
                        if (shstrtab && strcmp(shstrtab + shdrs[i].sh_name, ".rela.plt") == 0) {
                            rela_plt_count = shdrs[i].sh_size / sizeof(local_elf_rela_t);
                            rela_plt = (local_elf_rela_t *)malloc(shdrs[i].sh_size);
                            if (rela_plt) {
                                lseek(fd, shdrs[i].sh_offset, SEEK_SET);
                                read(fd, rela_plt, shdrs[i].sh_size);
                            }
                        } else {
                            rela_dyn_count = shdrs[i].sh_size / sizeof(local_elf_rela_t);
                            rela_dyn = (local_elf_rela_t *)malloc(shdrs[i].sh_size);
                            if (rela_dyn) {
                                lseek(fd, shdrs[i].sh_offset, SEEK_SET);
                                read(fd, rela_dyn, shdrs[i].sh_size);
                            }
                        }
                    }
                }

                if (sym_idx >= 0) {
                    uint32_t str_idx = shdrs[sym_idx].sh_link;
                    sym_count = shdrs[sym_idx].sh_size / sizeof(local_elf_sym_t);
                    symtab = (local_elf_sym_t *)malloc(shdrs[sym_idx].sh_size);
                    if (symtab) {
                        lseek(fd, shdrs[sym_idx].sh_offset, SEEK_SET);
                        read(fd, symtab, shdrs[sym_idx].sh_size);
                    }

                    if (str_idx < ehdr.e_shnum) {
                        str_size = shdrs[str_idx].sh_size;
                        strtab = (char *)malloc(str_size);
                        if (strtab) {
                            lseek(fd, shdrs[str_idx].sh_offset, SEEK_SET);
                            read(fd, strtab, str_size);
                        }
                    }
                }
                if (shstrtab) free(shstrtab);
            }
            free(shdrs);
        }
    }

    close(fd);

    dl_handle_t *h = &g_dl_handles[g_dl_count++];
    memset(h, 0, sizeof(dl_handle_t));
    strncpy(h->name, filename, sizeof(h->name) - 1);
    h->base_addr = base_addr;
    h->symtab = symtab;
    h->sym_count = sym_count;
    h->strtab = strtab;
    h->str_size = str_size;
    h->inits_done = 0;
    h->tls_mod_id = dl_mod_id;

    /* 3. Load DT_NEEDED dependencies recursively */
    if (dyn_vaddr) {
        local_elf_dyn_t *dyn = (local_elf_dyn_t *)dyn_vaddr;
        uintptr_t dt_strtab_vaddr = 0;
        size_t dt_strsz = 0;

        for (size_t d = 0; dyn[d].d_tag != DT_NULL; d++) {
            if (dyn[d].d_tag == DT_STRTAB) {
                dt_strtab_vaddr = base_addr + dyn[d].d_un.d_ptr;
            } else if (dyn[d].d_tag == DT_STRSZ) {
                dt_strsz = dyn[d].d_un.d_val;
            }
        }

        if (dt_strtab_vaddr == 0 && strtab != NULL) {
            dt_strtab_vaddr = (uintptr_t)strtab;
            dt_strsz = str_size;
        }

        if (dt_strtab_vaddr != 0) {
            for (size_t d = 0; dyn[d].d_tag != DT_NULL; d++) {
                if (dyn[d].d_tag == DT_NEEDED) {
                    uint64_t str_offset = dyn[d].d_un.d_val;
                    if (dt_strsz == 0 || str_offset < dt_strsz) {
                        const char *dep_name = (const char *)(dt_strtab_vaddr + str_offset);
                        if (dep_name && *dep_name) {
                            dlopen(dep_name, RTLD_GLOBAL);
                        }
                    }
                }
            }
        }
    }

    /* 4. Apply Dynamic Relocations (R_X86_64_RELATIVE, R_X86_64_64, R_X86_64_GLOB_DAT, R_X86_64_JUMP_SLOT) */
    for (int pass = 0; pass < 2; pass++) {
        local_elf_rela_t *relas = (pass == 0) ? rela_dyn : rela_plt;
        size_t count = (pass == 0) ? rela_dyn_count : rela_plt_count;
        if (!relas) continue;

        for (size_t j = 0; j < count; j++) {
            uint64_t type = ELF64_R_TYPE(relas[j].r_info);
            uint64_t sym_idx = ELF64_R_SYM(relas[j].r_info);
            uint64_t *target = (uint64_t *)(base_addr + relas[j].r_offset);

            if (type == R_X86_64_RELATIVE) {
                *target = (uint64_t)(base_addr + relas[j].r_addend);
            } else if (type == R_X86_64_64 || type == R_X86_64_GLOB_DAT || type == R_X86_64_JUMP_SLOT) {
                const char *sym_name = "";
                if (sym_idx < sym_count && symtab && strtab) {
                    if (symtab[sym_idx].st_name < str_size) {
                        sym_name = strtab + symtab[sym_idx].st_name;
                    }
                }

                void *sym_val = NULL;
                if (*sym_name) {
                    sym_val = dlsym(RTLD_DEFAULT, sym_name);
                    if (!sym_val && symtab && symtab[sym_idx].st_shndx != 0) {
                        sym_val = (void *)(base_addr + symtab[sym_idx].st_value);
                    }
                }

                if (sym_val) {
                    if (type == R_X86_64_64) {
                        *target = (uint64_t)((uintptr_t)sym_val + relas[j].r_addend);
                    } else {
                        *target = (uint64_t)(uintptr_t)sym_val;
                    }
                } else if (*target < base_addr && *target != 0) {
                    *target += base_addr;
                }
            } else if (type == R_X86_64_DTPMOD64) {
                uint64_t mod_id = dl_mod_id ? dl_mod_id : 1;
                if (sym_idx != 0 && symtab && sym_idx < sym_count) {
                    if (symtab[sym_idx].st_shndx != 0) {
                        mod_id = dl_mod_id ? dl_mod_id : 1;
                    } else if (strtab && symtab[sym_idx].st_name < str_size) {
                        const char *sym_name = strtab + symtab[sym_idx].st_name;
                        dl_handle_t *def_h = NULL;
                        local_elf_sym_t *def_sym = find_defining_symbol(sym_name, &def_h);
                        if (def_sym && def_h) {
                            mod_id = def_h->tls_mod_id ? def_h->tls_mod_id : 1;
                        }
                    }
                }
                *target = mod_id;
            } else if (type == R_X86_64_DTPOFF64) {
                uint64_t offset = relas[j].r_addend;
                if (sym_idx != 0 && symtab && sym_idx < sym_count) {
                    if (symtab[sym_idx].st_shndx != 0) {
                        offset += symtab[sym_idx].st_value;
                    } else if (strtab && symtab[sym_idx].st_name < str_size) {
                        const char *sym_name = strtab + symtab[sym_idx].st_name;
                        local_elf_sym_t *def_sym = find_defining_symbol(sym_name, NULL);
                        if (def_sym) {
                            offset += def_sym->st_value;
                        }
                    }
                }
                *target = offset;
            } else if (type == R_X86_64_TPOFF64) {
                uint64_t offset = relas[j].r_addend;
                if (sym_idx != 0 && symtab && sym_idx < sym_count) {
                    if (symtab[sym_idx].st_shndx != 0) {
                        offset += symtab[sym_idx].st_value;
                    } else if (strtab && symtab[sym_idx].st_name < str_size) {
                        const char *sym_name = strtab + symtab[sym_idx].st_name;
                        local_elf_sym_t *def_sym = find_defining_symbol(sym_name, NULL);
                        if (def_sym) {
                            offset += def_sym->st_value;
                        }
                    }
                }
                *target = offset;
            }
        }
        free(relas);
    }

    /* 5. Execute ELF constructors (DT_INIT and DT_INIT_ARRAY) */
    if (!h->inits_done) {
        h->inits_done = 1;
        uintptr_t init_func = 0;
        uintptr_t init_array = 0;
        size_t init_array_sz = 0;

        if (dyn_vaddr) {
            local_elf_dyn_t *dyn = (local_elf_dyn_t *)dyn_vaddr;
            for (size_t d = 0; dyn[d].d_tag != DT_NULL; d++) {
                if (dyn[d].d_tag == DT_INIT && dyn[d].d_un.d_ptr != 0) {
                    init_func = base_addr + dyn[d].d_un.d_ptr;
                } else if (dyn[d].d_tag == DT_INIT_ARRAY && dyn[d].d_un.d_ptr != 0) {
                    init_array = base_addr + dyn[d].d_un.d_ptr;
                } else if (dyn[d].d_tag == DT_INIT_ARRAYSZ) {
                    init_array_sz = dyn[d].d_un.d_val;
                }
            }
        }

        if (init_func) {
            ((void (*)(void))init_func)();
        }
        if (init_array && init_array_sz >= sizeof(uintptr_t)) {
            size_t count = init_array_sz / sizeof(uintptr_t);
            uintptr_t *arr = (uintptr_t *)init_array;
            for (size_t k = 0; k < count; k++) {
                if (arr[k] != 0 && arr[k] != (uintptr_t)-1) {
                    ((void (*)(void))arr[k])();
                }
            }
        }
    }

    set_dlerror(NULL);
    return h;
}

int dlclose(void *handle) {
    (void)handle;
    return 0;
}

int dladdr(const void *addr, Dl_info *info) {
    if (!info) return 0;
    init_main_binary_symbols();
    uintptr_t uaddr = (uintptr_t)addr;
    for (size_t i = 0; i < g_dl_count; i++) {
        dl_handle_t *h = &g_dl_handles[i];
        if (h->base_addr <= uaddr) {
            info->dli_fname = h->name;
            info->dli_fbase = (void *)h->base_addr;
            info->dli_sname = NULL;
            info->dli_saddr = NULL;
            if (h->symtab && h->strtab) {
                for (size_t s = 0; s < h->sym_count; s++) {
                    local_elf_sym_t *sym = &h->symtab[s];
                    if (sym->st_shndx != 0 && sym->st_name < h->str_size) {
                        uintptr_t sym_addr = h->base_addr + sym->st_value;
                        if (sym_addr == uaddr || (sym_addr <= uaddr && uaddr < sym_addr + sym->st_size)) {
                            info->dli_sname = h->strtab + sym->st_name;
                            info->dli_saddr = (void *)sym_addr;
                            break;
                        }
                    }
                }
            }
            return 1;
        }
    }
    info->dli_fname = "/lib/libc.so";
    info->dli_fbase = (void *)0;
    info->dli_sname = "unknown";
    info->dli_saddr = (void *)addr;
    return 1;
}

