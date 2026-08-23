#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <grp.h>

int main(int argc, char *argv[]) {
    if (geteuid() != 0) {
        printf("groupadd: Permission denied (must be root)\n");
        return 1;
    }

    if (argc < 2) {
        printf("Usage: groupadd [-g GID] <groupname>\n");
        return 1;
    }

    const char *groupname = NULL;
    int gid = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-g") == 0 && i + 1 < argc) {
            gid = atoi(argv[++i]);
        } else if (argv[i][0] != '-') {
            groupname = argv[i];
        }
    }

    if (!groupname) {
        printf("groupadd: No groupname specified\n");
        return 1;
    }

    if (getgrnam(groupname) != NULL) {
        printf("groupadd: group '%s' already exists\n", groupname);
        return 1;
    }

    if (gid < 0) {
        gid = 1000;
        setgrent();
        struct group *gr;
        while ((gr = getgrent()) != NULL) {
            if (gr->gr_gid >= (gid_t)gid) {
                gid = gr->gr_gid + 1;
            }
        }
        endgrent();
    }

    int fd = open("/etc/group", O_WRONLY | O_APPEND);
    if (fd < 0) {
        printf("groupadd: failed to open /etc/group\n");
        return 1;
    }

    char line[128];
    int len = snprintf(line, sizeof(line), "%s:x:%d:\n", groupname, gid);
    write(fd, line, len);
    close(fd);

    printf("groupadd: group '%s' (GID %d) created successfully.\n", groupname, gid);
    return 0;
}
