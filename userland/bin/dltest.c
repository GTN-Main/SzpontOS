#include <stdio.h>
#include <dlfcn.h>

typedef int (*calc_func_t)(int, int);
typedef const char *(*version_func_t)(void);

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("[DLTEST] Testing dynamic shared library loading (dlopen/dlsym)...\n");

    void *handle = dlopen("libcalc.so", RTLD_LAZY);
    if (!handle) {
        printf("[DLTEST] Error opening 'libcalc.so': %s\n", dlerror());
        return 1;
    }

    printf("[DLTEST] Successfully loaded '/lib/libcalc.so' at handle %p!\n", handle);

    void *m_handle = dlopen("/usr/lib/xorg/modules/drivers/modesetting_drv.so", RTLD_LAZY);
    if (!m_handle) {
        printf("[DLTEST] Error opening modesetting_drv.so: %s\n", dlerror());
    } else {
        printf("[DLTEST] Successfully loaded modesetting_drv.so at %p!\n", m_handle);
        void *sym = dlsym(m_handle, "modesettingModuleData");
        if (sym) {
            printf("[DLTEST] Successfully found 'modesettingModuleData' at %p!\n", sym);
        } else {
            printf("[DLTEST] Failed to find 'modesettingModuleData': %s\n", dlerror());
        }
    }

    /* Test /proc/self/exe */
    FILE *f = fopen("/proc/self/exe", "r");
    if (f) {
        char buf[128] = {0};
        fgets(buf, sizeof(buf), f);
        fclose(f);
        printf("[DLTEST] /proc/self/exe content: '%s'\n", buf);
    } else {
        printf("[DLTEST] Failed to open /proc/self/exe\n");
    }

    void *sym_open = dlsym(RTLD_DEFAULT, "open");
    printf("[DLTEST] dlsym(RTLD_DEFAULT, 'open') = %p\n", sym_open);

    void *sym_busid = dlsym(RTLD_DEFAULT, "drmGetBusid");
    printf("[DLTEST] dlsym(RTLD_DEFAULT, 'drmGetBusid') = %p\n", sym_busid);

    /* Test dlclose */
    dlclose(handle);
    printf("[DLTEST] Dynamic loading test PASSED!\n");
    return 0;
}
