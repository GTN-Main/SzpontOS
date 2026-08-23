#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_TAIL_LINES 128
#define MAX_LINE_LEN   256

static char g_tail_lines[MAX_TAIL_LINES][MAX_LINE_LEN];

static void tail_fd(int fd, int max_lines) {
    if (max_lines > MAX_TAIL_LINES) max_lines = MAX_TAIL_LINES;
    int total_lines = 0;

    char current_line[MAX_LINE_LEN];
    int cur_idx = 0;
    char buf[256];
    ssize_t n;

    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < n; i++) {
            char c = buf[i];
            if (cur_idx < MAX_LINE_LEN - 1) {
                current_line[cur_idx++] = c;
            }
            if (c == '\n') {
                current_line[cur_idx] = '\0';
                int slot = total_lines % max_lines;
                strncpy(g_tail_lines[slot], current_line, MAX_LINE_LEN - 1);
                g_tail_lines[slot][MAX_LINE_LEN - 1] = '\0';
                total_lines++;
                cur_idx = 0;
            }
        }
    }

    if (cur_idx > 0) {
        current_line[cur_idx] = '\0';
        int slot = total_lines % max_lines;
        strncpy(g_tail_lines[slot], current_line, MAX_LINE_LEN - 1);
        g_tail_lines[slot][MAX_LINE_LEN - 1] = '\0';
        total_lines++;
    }

    int print_count = (total_lines < max_lines) ? total_lines : max_lines;
    int start = (total_lines < max_lines) ? 0 : (total_lines % max_lines);

    for (int i = 0; i < print_count; i++) {
        int idx = (start + i) % max_lines;
        printf("%s\n", g_tail_lines[idx]);
    }
}

int main(int argc, char *argv[]) {
    int max_lines = 10;
    int first_file = 1;

    if (argc > 2 && strcmp(argv[1], "-n") == 0) {
        max_lines = atoi(argv[2]);
        if (max_lines <= 0) max_lines = 10;
        first_file = 3;
    }

    if (first_file >= argc) {
        tail_fd(STDIN_FILENO, max_lines);
        return 0;
    }

    for (int i = first_file; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY, 0);
        if (fd < 0) {
            printf("tail: cannot open '%s'\n", argv[i]);
            continue;
        }

        if (argc - first_file > 1) {
            printf("==> %s <==\n", argv[i]);
        }

        tail_fd(fd, max_lines);
        close(fd);
    }

    return 0;
}
