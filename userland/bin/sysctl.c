/*
 * sysctl - get or set kernel state
 * Inspired by FreeBSD sbin/sysctl/
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>

static int sysctl_read_all(void) {
    int fd = open("/proc/sysctl", O_RDONLY);
    if (fd < 0) {
        perror("sysctl: cannot open /proc/sysctl");
        return 1;
    }

    char buf[1024];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        printf("%s", buf);
    }
    close(fd);
    return 0;
}

static int sysctl_get(const char *name) {
    FILE *f = fopen("/proc/sysctl", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, name, strlen(name)) == 0 && line[strlen(name)] == ' ') {
                printf("%s", line);
                fclose(f);
                return 0;
            }
        }
        fclose(f);
    }
    fprintf(stderr, "sysctl: unknown oid '%s'\n", name);
    return 1;
}

static int sysctl_set(const char *name, const char *value) {
    size_t newlen = strlen(value);
    long ret = (long)__syscall5(SYS_sysctl, (int64_t)name, 0, 0, (int64_t)value, (int64_t)newlen);
    if (ret != 0) {
        fprintf(stderr, "sysctl: error setting '%s': Operation not permitted or invalid oid\n", name);
        return 1;
    }
    printf("%s: %s -> %s\n", name, name, value);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        return sysctl_read_all();
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "-A") == 0) {
            return sysctl_read_all();
        }

        if (strcmp(argv[i], "-w") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "sysctl: option requires an argument -- w\n");
                return 1;
            }
            i++;
        }

        char *eq = strchr(argv[i], '=');
        if (eq) {
            *eq = '\0';
            const char *name = argv[i];
            const char *val = eq + 1;
            sysctl_set(name, val);
        } else {
            sysctl_get(argv[i]);
        }
    }

    return 0;
}
