#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>

static void remove_entry_from_file(const char *filepath, const char *prefix) {
    int fd = open(filepath, O_RDONLY);
    if (fd < 0)
        return;

    char file_buf[4096];
    ssize_t bytes = read(fd, file_buf, sizeof(file_buf) - 1);
    close(fd);
    if (bytes <= 0)
        return;
    file_buf[bytes] = '\0';

    char new_buf[4096];
    new_buf[0] = '\0';

    char *line = file_buf;
    while (*line) {
        char *eol = strchr(line, '\n');
        if (eol)
            *eol = '\0';

        if (strncmp(line, prefix, strlen(prefix)) != 0 && *line) {
            strcat(new_buf, line);
            strcat(new_buf, "\n");
        }

        if (eol) {
            line = eol + 1;
        } else {
            break;
        }
    }

    fd = open(filepath, O_WRONLY | O_TRUNC);
    if (fd >= 0) {
        write(fd, new_buf, strlen(new_buf));
        close(fd);
    }
}

int main(int argc, char *argv[]) {
    if (geteuid() != 0) {
        printf("userdel: Permission denied (must be root)\n");
        return 1;
    }

    if (argc < 2) {
        printf("Usage: userdel <username>\n");
        return 1;
    }

    const char *username = argv[1];
    if (strcmp(username, "root") == 0) {
        printf("userdel: cannot remove root user\n");
        return 1;
    }

    if (getpwnam(username) == NULL) {
        printf("userdel: user '%s' does not exist\n", username);
        return 1;
    }

    char prefix[128];
    snprintf(prefix, sizeof(prefix), "%s:", username);

    remove_entry_from_file("/etc/passwd", prefix);
    remove_entry_from_file("/etc/group", prefix);

    printf("userdel: user '%s' removed successfully.\n", username);
    return 0;
}
