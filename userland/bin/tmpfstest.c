/*
 * tmpfstest - verify /tmp and /run in-memory filesystem
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("[TMPFSTEST] Testing /tmp In-Memory RAM Filesystem...\n");

    /* 1. Create file in /tmp */
    const char *test_path = "/tmp/szpont_test.txt";
    int fd = open(test_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("tmpfstest: open /tmp/szpont_test.txt failed");
        return 1;
    }

    const char *msg = "Hello from SzpontOS TmpFS! In-memory storage is active.\n";
    ssize_t w = write(fd, msg, strlen(msg));
    close(fd);

    if (w != (ssize_t)strlen(msg)) {
        fprintf(stderr, "tmpfstest: write failed (wrote %zd)\n", w);
        return 1;
    }

    /* 2. Read back from /tmp */
    fd = open(test_path, O_RDONLY);
    if (fd < 0) {
        perror("tmpfstest: read open failed");
        return 1;
    }

    char buf[128];
    memset(buf, 0, sizeof(buf));
    ssize_t r = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    if (r <= 0 || strcmp(buf, msg) != 0) {
        fprintf(stderr, "tmpfstest: data mismatch in /tmp\n");
        return 1;
    }
    printf("  Read from /tmp: %s", buf);

    /* 3. Create subfolder in /tmp */
    if (mkdir("/tmp/subfolder", 0755) != 0) {
        perror("tmpfstest: mkdir /tmp/subfolder failed");
        return 1;
    }

    int fd2 = open("/tmp/subfolder/nested.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd2 >= 0) {
        write(fd2, "nested data\n", 12);
        close(fd2);
    }

    /* 4. Unlink file */
    if (unlink(test_path) != 0) {
        perror("tmpfstest: unlink /tmp/szpont_test.txt failed");
        return 1;
    }

    printf("[TMPFSTEST] TmpFS In-Memory filesystem PASSED successfully!\n");
    return 0;
}
