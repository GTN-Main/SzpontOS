/*
 * SzpontOS - lsmod (List Loaded Kernel Modules)
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    int fd = open("/proc/modules", O_RDONLY);
    if (fd < 0) {
        perror("lsmod: cannot open /proc/modules");
        return 1;
    }

    char buf[2048];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    if (n <= 0) {
        printf("Module                  Size  Used by\n");
        return 0;
    }

    buf[n] = '\0';

    printf("%-24s %8s %8s %-10s %s\n", "Module", "Size", "Used by", "Status", "Base Address");

    char *line = buf;
    while (line && *line) {
        char *next = strchr(line, '\n');
        if (next)
            *next = '\0';

        if (strlen(line) > 0) {
            char *p = line;
            while (*p && (*p == ' ' || *p == '\t'))
                p++;
            char *name = p;
            while (*p && *p != ' ' && *p != '\t')
                p++;
            if (*p)
                *p++ = '\0';

            while (*p && (*p == ' ' || *p == '\t'))
                p++;
            char *size = p;
            while (*p && *p != ' ' && *p != '\t')
                p++;
            if (*p)
                *p++ = '\0';

            while (*p && (*p == ' ' || *p == '\t'))
                p++;
            char *ref = p;
            while (*p && *p != ' ' && *p != '\t')
                p++;
            if (*p)
                *p++ = '\0';

            while (*p && (*p == ' ' || *p == '\t'))
                p++;
            char *dash = p;
            while (*p && *p != ' ' && *p != '\t')
                p++;
            if (*p)
                *p++ = '\0';
            (void)dash;

            while (*p && (*p == ' ' || *p == '\t'))
                p++;
            char *state = p;
            while (*p && *p != ' ' && *p != '\t')
                p++;
            if (*p)
                *p++ = '\0';

            while (*p && (*p == ' ' || *p == '\t'))
                p++;
            char *addr = p;
            while (*p && *p != ' ' && *p != '\t')
                p++;
            if (*p)
                *p = '\0';

            printf("%-24s %8s %8s %-10s %s\n", name[0] ? name : "-", size[0] ? size : "-", ref[0] ? ref : "0",
                   state[0] ? state : "Live", addr[0] ? addr : "-");
        }

        if (!next)
            break;
        line = next + 1;
    }

    return 0;
}
