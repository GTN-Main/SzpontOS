/*
 * SzpontOS - POSIX system() implementation
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int system(const char *command) {
    if (!command)
        return 1;

    pid_t pid = fork();
    if (pid < 0)
        return -1;

    if (pid == 0) {
        const char *argv[] = { "sh", "-c", command, NULL };
        execve("/bin/sh", (char *const *)argv, NULL);
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0)
        return -1;

    return status;
}
