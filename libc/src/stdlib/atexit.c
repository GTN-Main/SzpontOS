#include <stdlib.h>

#define MAX_ATEXIT 32
static void (*g_atexit_funcs[MAX_ATEXIT])(void);
static int g_atexit_count = 0;

int atexit(void (*function)(void)) {
    if (!function || g_atexit_count >= MAX_ATEXIT)
        return -1;
    g_atexit_funcs[g_atexit_count++] = function;
    return 0;
}

void __execute_atexit(void) {
    while (g_atexit_count > 0) {
        g_atexit_count--;
        if (g_atexit_funcs[g_atexit_count]) {
            g_atexit_funcs[g_atexit_count]();
        }
    }
}
