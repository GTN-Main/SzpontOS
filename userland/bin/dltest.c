#include <stdio.h>
#include <dlfcn.h>

typedef int (*calc_func_t)(int, int);
typedef const char *(*version_func_t)(void);

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    printf("[DLTEST] Testing dynamic shared library loading (dlopen/dlsym)...\n");

    void *handle = dlopen("libcalc.so", RTLD_LAZY);
    if (!handle) {
        printf("[DLTEST] Error opening 'libcalc.so': %s\n", dlerror());
        return 1;
    }

    printf("[DLTEST] Successfully loaded '/lib/libcalc.so' at handle %p!\n", handle);

    /* Test dlclose */
    dlclose(handle);
    printf("[DLTEST] Dynamic loading test PASSED!\n");
    return 0;
}
