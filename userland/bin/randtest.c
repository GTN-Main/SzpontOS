/*
 * randtest - verify CSPRNG /dev/urandom and getrandom()
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("[RANDTEST] Testing CSPRNG & /dev/urandom...\n");

    /* 1. Read from /dev/urandom */
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        perror("randtest: open /dev/urandom failed");
        return 1;
    }

    uint8_t buf1[16];
    ssize_t n1 = read(fd, buf1, sizeof(buf1));
    close(fd);

    if (n1 != sizeof(buf1)) {
        fprintf(stderr, "randtest: failed to read 16 bytes from /dev/urandom (got %zd)\n", n1);
        return 1;
    }

    printf("  /dev/urandom bytes: ");
    for (size_t i = 0; i < sizeof(buf1); i++) {
        printf("%02x ", buf1[i]);
    }
    printf("\n");

    /* 2. Syscall getrandom */
    uint8_t buf2[16];
    long ret = syscall(SYS_getrandom, buf2, sizeof(buf2), 0);
    if (ret != sizeof(buf2)) {
        fprintf(stderr, "randtest: getrandom syscall failed (ret=%ld)\n", ret);
        return 1;
    }

    printf("  getrandom() bytes:  ");
    for (size_t i = 0; i < sizeof(buf2); i++) {
        printf("%02x ", buf2[i]);
    }
    printf("\n");

    if (memcmp(buf1, buf2, 16) == 0) {
        fprintf(stderr, "randtest: warning - subsequent random streams are identical!\n");
        return 1;
    }

    printf("[RANDTEST] CSPRNG test PASSED successfully!\n");
    return 0;
}
