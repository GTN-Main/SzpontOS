/*
 * kill - send signals to processes
 * Inspired by FreeBSD bin/kill/
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

static const struct {
    int num;
    const char *name;
} g_signals[] = {{SIGHUP, "HUP"},   {SIGINT, "INT"},    {SIGQUIT, "QUIT"}, {SIGILL, "ILL"},   {SIGTRAP, "TRAP"},
                 {SIGABRT, "ABRT"}, {SIGBUS, "BUS"},    {SIGFPE, "FPE"},   {SIGKILL, "KILL"}, {SIGUSR1, "USR1"},
                 {SIGSEGV, "SEGV"}, {SIGUSR2, "USR2"},  {SIGPIPE, "PIPE"}, {SIGALRM, "ALRM"}, {SIGTERM, "TERM"},
                 {SIGCHLD, "CHLD"}, {SIGCONT, "CONT"},  {SIGSTOP, "STOP"}, {SIGTSTP, "TSTP"}, {SIGTTIN, "TTIN"},
                 {SIGTTOU, "TTOU"}, {SIGWINCH, "WINCH"}};
#define SIG_COUNT (sizeof(g_signals) / sizeof(g_signals[0]))

static void list_signals(void) {
    for (size_t i = 0; i < SIG_COUNT; i++) {
        printf("%2d) SIG%-6s%c", g_signals[i].num, g_signals[i].name,
               ((i + 1) % 4 == 0 || i + 1 == SIG_COUNT) ? '\n' : ' ');
    }
}

static int parse_signal(const char *str) {
    if (!str || !*str)
        return -1;
    if (strncmp(str, "SIG", 3) == 0)
        str += 3;

    char *end;
    long num = strtol(str, &end, 10);
    if (*end == '\0' && num >= 1 && num < 32) {
        return (int)num;
    }

    for (size_t i = 0; i < SIG_COUNT; i++) {
        if (strcasecmp(str, g_signals[i].name) == 0) {
            return g_signals[i].num;
        }
    }
    return -1;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: kill [-s sigspec | -signum | -sigspec] pid ...\n"
                        "       kill -l\n");
        return 1;
    }

    if (strcmp(argv[1], "-l") == 0) {
        list_signals();
        return 0;
    }

    int sig = SIGTERM;
    int arg_idx = 1;

    if (strcmp(argv[arg_idx], "-s") == 0) {
        if (arg_idx + 1 >= argc) {
            fprintf(stderr, "kill: option requires an argument -- s\n");
            return 1;
        }
        sig = parse_signal(argv[arg_idx + 1]);
        if (sig < 0) {
            fprintf(stderr, "kill: unknown signal %s\n", argv[arg_idx + 1]);
            return 1;
        }
        arg_idx += 2;
    } else if (argv[arg_idx][0] == '-' && argv[arg_idx][1] != '\0' &&
               (argv[arg_idx][1] < '0' || argv[arg_idx][1] > '9' || argv[arg_idx][2] == '\0' || argc > 2)) {
        sig = parse_signal(argv[arg_idx] + 1);
        if (sig >= 0) {
            arg_idx++;
        } else {
            sig = SIGTERM;
        }
    }

    if (arg_idx >= argc) {
        fprintf(stderr, "kill: no process ID specified\n");
        return 1;
    }

    int errors = 0;
    for (int i = arg_idx; i < argc; i++) {
        pid_t pid = (pid_t)atoi(argv[i]);
        if (kill(pid, sig) != 0) {
            fprintf(stderr, "kill: failed to send signal to pid %d\n", pid);
            errors++;
        }
    }

    return errors ? 1 : 0;
}
