#include <stdio.h>
#include <sys/sysinfo.h>

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    struct sysinfo s;
    if (sysinfo(&s) != 0) {
        printf("uptime: failed to get sysinfo\n");
        return 1;
    }

    long up = s.uptime;
    long hours = up / 3600;
    long mins = (up % 3600) / 60;
    long secs = up % 60;

    printf(" up %ld:%02ld:%02ld,  %u processes,  load average: 0.00, 0.00, 0.00\n", hours, mins, secs, s.procs);

    return 0;
}
