/*
 * SzpontOS — env (Print or run in modified environment)
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

extern char **environ;

static void usage(void) {
    fprintf(stderr, "Usage: env [-i] [-u name] [name=value ...] [command [args ...]]\n");
    fprintf(stderr, "Set or display environment variables.\n\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -i, --ignore-environment  Start with an empty environment\n");
    fprintf(stderr, "  -u, --unset=NAME          Remove variable from the environment\n");
    fprintf(stderr, "  -h, --help                Display this help\n");
    exit(1);
}

int main(int argc, char *argv[]) {
    int i = 1;

    /* Parse flags */
    while (i < argc) {
        if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--ignore-environment") == 0 || strcmp(argv[i], "-") == 0) {
            clearenv();
            i++;
        } else if (strcmp(argv[i], "-u") == 0) {
            i++;
            if (i >= argc) {
                usage();
            }
            unsetenv(argv[i]);
            i++;
        } else if (strncmp(argv[i], "--unset=", 8) == 0) {
            unsetenv(argv[i] + 8);
            i++;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage();
        } else {
            break;
        }
    }

    /* Parse NAME=VALUE definitions */
    while (i < argc) {
        char *eq = strchr(argv[i], '=');
        if (eq && eq != argv[i]) {
            *eq = '\0';
            setenv(argv[i], eq + 1, 1);
            *eq = '=';
            i++;
        } else {
            break;
        }
    }

    /* If no command is specified, print environment */
    if (i >= argc) {
        if (environ) {
            for (char **ep = environ; *ep; ep++) {
                printf("%s\n", *ep);
            }
        }
        return 0;
    }

    /* Execute command */
    execvp(argv[i], &argv[i]);
    fprintf(stderr, "env: failed to execute '%s': %s\n", argv[i], strerror(errno));
    return (errno == ENOENT) ? 127 : 126;
}
