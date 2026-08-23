/*
 * killall - kill processes by name
 * Inspired by FreeBSD usr.bin/killall/
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/syscall.h>

typedef struct proc_info {
    pid_t pid;
    pid_t ppid;
    uid_t uid;
    gid_t gid;
    int state;
    char name[64];
} proc_info_t;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: killall [-s signal] process_name ...\n");
        return 1;
    }

    int sig = SIGTERM;
    int arg_idx = 1;

    if (argv[1][0] == '-') {
        if (strcmp(argv[1], "-9") == 0 || strcmp(argv[1], "-KILL") == 0) {
            sig = SIGKILL;
            arg_idx = 2;
        } else if (strcmp(argv[1], "-INT") == 0) {
            sig = SIGINT;
            arg_idx = 2;
        } else if (strcmp(argv[1], "-s") == 0 && argc > 2) {
            sig = atoi(argv[2]);
            if (sig == 0 && strcmp(argv[2], "KILL") == 0) sig = SIGKILL;
            arg_idx = 3;
        }
    }

    if (arg_idx >= argc) {
        fprintf(stderr, "killall: no process name specified\n");
        return 1;
    }

    proc_info_t procs[128];
    long count = (long)__syscall2(SYS_getprocs, (int64_t)procs, 128);
    if (count <= 0) {
        fprintf(stderr, "killall: could not retrieve process list\n");
        return 1;
    }

    int killed = 0;
    for (int a = arg_idx; a < argc; a++) {
        const char *target_name = argv[a];
        for (int i = 0; i < count; i++) {
            const char *pname = procs[i].name;
            const char *slash = strrchr(pname, '/');
            if (slash) pname = slash + 1;

            if (strcmp(pname, target_name) == 0 || strcmp(procs[i].name, target_name) == 0) {
                if (procs[i].pid > 1 && procs[i].pid != getpid()) {
                    if (kill(procs[i].pid, sig) == 0) {
                        printf("Killed %s (PID %d)\n", pname, procs[i].pid);
                        killed++;
                    }
                }
            }
        }
    }

    if (killed == 0) {
        fprintf(stderr, "killall: no matching processes found\n");
        return 1;
    }

    return 0;
}
