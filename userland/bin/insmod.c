/*
 * SzpontOS - insmod (Insert Module into Kernel)
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/syscall.h>

extern int64_t __syscall3(int64_t num, int64_t a1, int64_t a2, int64_t a3);

static int init_module(void *image, unsigned long len, const char *param_values) {
    return (int)__syscall3(SYS_init_module, (int64_t)image, (int64_t)len, (int64_t)param_values);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <module.sko> [args...]\n", argv[0]);
        return 1;
    }

    const char *path = argv[1];
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("insmod: open failed");
        return 1;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        perror("insmod: fstat failed");
        close(fd);
        return 1;
    }

    size_t size = st.st_size;
    if (size == 0) {
        fprintf(stderr, "insmod: empty module file '%s'\n", path);
        close(fd);
        return 1;
    }

    void *buf = malloc(size);
    if (!buf) {
        fprintf(stderr, "insmod: out of memory\n");
        close(fd);
        return 1;
    }

    ssize_t rd = read(fd, buf, size);
    close(fd);

    if (rd < (ssize_t)size) {
        fprintf(stderr, "insmod: short read from '%s'\n", path);
        free(buf);
        return 1;
    }

    char args[256] = {0};
    if (argc > 2) {
        for (int i = 2; i < argc; i++) {
            if (i > 2)
                strncat(args, " ", sizeof(args) - strlen(args) - 1);
            strncat(args, argv[i], sizeof(args) - strlen(args) - 1);
        }
    } else {
        const char *base = strrchr(path, '/');
        base = base ? base + 1 : path;
        strncpy(args, base, sizeof(args) - 1);
        char *dot = strstr(args, ".sko");
        if (dot)
            *dot = '\0';
    }

    int ret = init_module(buf, size, args);
    free(buf);

    if (ret != 0) {
        fprintf(stderr, "insmod: ERROR: could not insert module '%s': error %d\n", path, ret);
        return 1;
    }

    printf("insmod: Module '%s' inserted successfully.\n", path);
    return 0;
}
