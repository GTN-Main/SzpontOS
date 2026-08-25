#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

static void wc_fd(int fd, const char *name, int show_l, int show_w, int show_c) {
    char buf[1024];
    ssize_t n;
    long lines = 0, words = 0, bytes = 0;
    int in_word = 0;

    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        bytes += n;
        for (ssize_t i = 0; i < n; i++) {
            char c = buf[i];
            if (c == '\n')
                lines++;
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                in_word = 0;
            } else if (!in_word) {
                in_word = 1;
                words++;
            }
        }
    }

    if (show_l)
        printf("%7ld ", lines);
    if (show_w)
        printf("%7ld ", words);
    if (show_c)
        printf("%7ld ", bytes);
    if (name)
        printf("%s", name);
    printf("\n");
}

int main(int argc, char *argv[]) {
    int show_l = 0, show_w = 0, show_c = 0;
    int first_file = 1;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            for (size_t j = 1; j < strlen(argv[i]); j++) {
                if (argv[i][j] == 'l')
                    show_l = 1;
                else if (argv[i][j] == 'w')
                    show_w = 1;
                else if (argv[i][j] == 'c')
                    show_c = 1;
            }
            first_file = i + 1;
        } else {
            break;
        }
    }

    if (!show_l && !show_w && !show_c) {
        show_l = show_w = show_c = 1;
    }

    if (first_file >= argc) {
        wc_fd(STDIN_FILENO, NULL, show_l, show_w, show_c);
        return 0;
    }

    for (int i = first_file; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY, 0);
        if (fd < 0) {
            printf("wc: cannot open '%s'\n", argv[i]);
            continue;
        }
        wc_fd(fd, argv[i], show_l, show_w, show_c);
        close(fd);
    }

    return 0;
}
