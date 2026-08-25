#include <stdio.h>

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("rootfs on / type ramfs (rw,relatime)\n");
    printf("devfs on /dev type devfs (rw,nosuid,noexec)\n");
    printf("/dev/hda on /mnt type ext2 (rw,relatime)\n");

    return 0;
}
