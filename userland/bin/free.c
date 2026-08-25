#include <stdio.h>
#include <string.h>
#include <sys/sysinfo.h>

int main(int argc, char *argv[]) {
    int human = 0;
    int megabytes = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--human") == 0) {
            human = 1;
        } else if (strcmp(argv[i], "-m") == 0) {
            megabytes = 1;
        }
    }

    struct sysinfo s;
    if (sysinfo(&s) != 0) {
        printf("free: failed to get system memory info\n");
        return 1;
    }

    uint64_t total = s.totalram;
    uint64_t free_mem = s.freeram;
    uint64_t used = (total >= free_mem) ? (total - free_mem) : 0;
    uint64_t buff = s.bufferram;

    if (human) {
        printf("%-10s %10s %10s %10s %10s\n", "", "total", "used", "free", "buff/cache");
        printf("%-10s %9luM %9luM %9luM %9luK\n", "Mem:", total / (1024 * 1024), used / (1024 * 1024),
               free_mem / (1024 * 1024), buff / 1024);
        printf("%-10s %9luM %9luM %9luM\n", "Swap:", 0UL, 0UL, 0UL);
    } else if (megabytes) {
        printf("%-10s %10s %10s %10s %10s\n", "", "total", "used", "free", "buff/cache");
        printf("%-10s %10lu %10lu %10lu %10lu\n", "Mem:", total / (1024 * 1024), used / (1024 * 1024),
               free_mem / (1024 * 1024), buff / (1024 * 1024));
        printf("%-10s %10lu %10lu %10lu\n", "Swap:", 0UL, 0UL, 0UL);
    } else {
        printf("%-10s %10s %10s %10s %10s\n", "", "total", "used", "free", "buff/cache");
        printf("%-10s %10lu %10lu %10lu %10lu\n", "Mem:", total / 1024, used / 1024, free_mem / 1024, buff / 1024);
        printf("%-10s %10lu %10lu %10lu\n", "Swap:", 0UL, 0UL, 0UL);
    }

    return 0;
}
