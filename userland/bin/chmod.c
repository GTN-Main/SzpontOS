#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

static mode_t parse_octal_mode(const char *str) {
    mode_t mode = 0;
    while (*str >= '0' && *str <= '7') {
        mode = (mode << 3) | (*str - '0');
        str++;
    }
    return mode;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: chmod <octal_mode> <file...>\n");
        return 1;
    }

    mode_t mode = parse_octal_mode(argv[1]);

    for (int i = 2; i < argc; i++) {
        if (chmod(argv[i], mode) != 0) {
            printf("chmod: cannot change permissions of '%s': Permission denied or not found\n", argv[i]);
        }
    }

    return 0;
}
