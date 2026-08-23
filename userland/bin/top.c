/*
 * SzpontOS - top (Interactive System & Process Monitor)
 * Inspired by FreeBSD usr.bin/top/
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <time.h>
#include <fcntl.h>
#include <dirent.h>

typedef struct {
    pid_t pid;
    char name[32];
    char state;
    size_t mem_kb;
} proc_entry_t;

static void render_top(void) {
    struct sysinfo si;
    if (sysinfo(&si) != 0) {
        return;
    }

    struct utsname un;
    uname(&un);

    long uptime_secs = si.uptime;
    int hours = (int)(uptime_secs / 3600);
    int mins = (int)((uptime_secs % 3600) / 60);
    int secs = (int)(uptime_secs % 60);

    /* Move cursor to home and clear screen */
    printf("\033[H\033[2J");

    /* Header bar */
    printf("\033[1;37;44m SzpontOS top - %02d:%02d:%02d up %02d:%02d:%02d, %d procs \033[0m\n",
           hours, mins, secs, hours, mins, secs, (int)si.procs);

    /* Memory summary */
    unsigned long total_mb = si.totalram / 1024 / 1024;
    unsigned long free_mb = si.freeram / 1024 / 1024;
    unsigned long used_mb = total_mb > free_mb ? (total_mb - free_mb) : 0;

    printf("\033[1;36mMem:\033[0m \033[1;32m%luM\033[0m total, \033[1;31m%luM\033[0m used, \033[1;32m%luM\033[0m free\n",
           total_mb, used_mb, free_mb);
    printf("\033[1;36mSystem:\033[0m %s %s (%s)\n\n", un.sysname, un.release, un.machine);

    /* Table header */
    printf("\033[1;30;47m  PID  STATE   MEM(KB)   COMMAND                     \033[0m\n");

    /* Read /proc */
    DIR *proc_dir = opendir("/proc");
    if (proc_dir) {
        struct dirent *ent;
        int displayed = 0;
        while ((ent = readdir(proc_dir)) != NULL && displayed < 20) {
            if (ent->d_name[0] >= '0' && ent->d_name[0] <= '9') {
                int pid = atoi(ent->d_name);
                char stat_path[64];
                snprintf(stat_path, sizeof(stat_path), "/proc/%d/stat", pid);
                FILE *f = fopen(stat_path, "r");
                char cmd[32] = "unknown";
                char state = 'R';
                unsigned long mem = 0;

                if (f) {
                    fscanf(f, "%*d (%31[^)]) %c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*u %*u %*d %*d %*d %*d %*d %*d %*u %*u %*d %lu",
                           cmd, &state, &mem);
                    fclose(f);
                }

                printf(" %4d    %c    %7lu   %-28s\n", pid, state, (mem / 1024), cmd);
                displayed++;
            }
        }
        closedir(proc_dir);
    }

    printf("\n\033[2mPress Ctrl+C to exit.\033[0m\n");
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    int iterations = (argc > 1) ? atoi(argv[1]) : -1;
    int count = 0;

    while (iterations < 0 || count < iterations) {
        render_top();
        count++;
        if (iterations > 0 && count >= iterations) break;
        sleep(1);
    }

    return 0;
}
