#ifndef SZPONTOS_KERNEL_MODULE_H
#define SZPONTOS_KERNEL_MODULE_H

#include <kernel/types.h>
#include <kernel/list.h>

#define MODULE_NAME_LEN 64

typedef enum { MODULE_STATE_LIVE = 0, MODULE_STATE_COMING = 1, MODULE_STATE_GOING = 2 } module_state_t;

typedef struct kernel_symbol {
    const char *name;
    uintptr_t addr;
} kernel_symbol_t;

typedef struct module {
    char name[MODULE_NAME_LEN];
    module_state_t state;
    uintptr_t base_addr;
    size_t size;
    uintptr_t *phys_pages;
    size_t num_pages;
    int refcnt;

    int (*init)(void);
    void (*exit)(void);

    /* Metadata */
    char author[64];
    char description[128];
    char license[32];
    char version[32];

    list_node_t list;
} module_t;

#define EXPORT_SYMBOL(sym)                                                                                             \
    __attribute__((used, section("__ksymtab"))) static const kernel_symbol_t __ksymtab_##sym = {#sym, (uintptr_t)&sym}

#define MODULE_NAME(str) static const char __mod_name[] __attribute__((used, section(".modinfo"))) = "name=" str
#define MODULE_AUTHOR(str) static const char __mod_author[] __attribute__((used, section(".modinfo"))) = "author=" str
#define MODULE_DESCRIPTION(str)                                                                                        \
    static const char __mod_desc[] __attribute__((used, section(".modinfo"))) = "description=" str
#define MODULE_LICENSE(str)                                                                                            \
    static const char __mod_license[] __attribute__((used, section(".modinfo"))) = "license=" str
#define MODULE_VERSION(str)                                                                                            \
    static const char __mod_version[] __attribute__((used, section(".modinfo"))) = "version=" str

#define module_init(fn) int init_module(void) __attribute__((alias(#fn)))
#define module_exit(fn) void cleanup_module(void) __attribute__((alias(#fn)))

void module_init_subsystem(void);
int module_load(const void *image, size_t size, const char *args, module_t **out_mod);
int module_unload(const char *name, unsigned int flags);
module_t *module_find(const char *name);
void module_list_for_each(void (*cb)(module_t *mod, void *arg), void *arg);

uintptr_t ksym_lookup(const char *name);
int ksym_register(const char *name, uintptr_t addr);
void ksym_unregister_range(uintptr_t base, size_t size);

#endif /* SZPONTOS_KERNEL_MODULE_H */
