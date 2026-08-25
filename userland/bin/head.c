#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

static void head_fd(int fd, int max_lines) {
    char buf[1024];
    int line_count = 0;
    ssize_t n;

    while (line_count < max_lines && (n = read(fd, buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < n; i++) {
            putchar(buf[i]);
            if (buf[i] == '\n') {
                line_count++;
                if (line_count >= max_lines)
                    break;
            }
        }
    }
}

int main(int argc, char *argv[]) {
    int max_lines = 10;
    int first_file = 1;

    if (argc > 2 && strcmp(argv[1], "-n") == 0) {
        max_lines = atoi(argv[2]);
        if (max_lines <= 0)
            max_lines = 10;
        first_file = 3;
    }

    if (first_file >= argc) {
        head_fd(STDIN_FILENO, max_lines);
        return 0;
    }

    for (int i = first_file; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY, 0);
        if (fd < 0) {
            printf("head: cannot open '%s'\n", argv[i]);
            continue;
        }

        if (argc - first_file > 1) {
            printf("==> %s <==\n", argv[i]);
        }

        head_fd(fd, max_lines);
        close(fd);
    }

    return 0;
}
