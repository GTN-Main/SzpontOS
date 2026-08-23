#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        /* Read from stdin */
        char buf[512];
        ssize_t bytes;
        while ((bytes = read(0, buf, sizeof(buf) - 1)) > 0) {
            buf[bytes] = '\0';
            printf("%s", buf);
        }
        return 0;
    }

    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY);
        if (fd < 0) {
            printf("cat: %s: No such file or directory\n", argv[i]);
            continue;
        }

        char buf[512];
        ssize_t bytes;
        while ((bytes = read(fd, buf, sizeof(buf) - 1)) > 0) {
            buf[bytes] = '\0';
            printf("%s", buf);
        }
        close(fd);
    }

    return 0;
}
