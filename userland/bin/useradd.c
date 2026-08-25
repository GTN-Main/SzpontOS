#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>

int main(int argc, char *argv[]) {
    if (geteuid() != 0) {
        printf("useradd: Permission denied (must be root)\n");
        return 1;
    }

    if (argc < 2) {
        printf("Usage: useradd [-u UID] [-g GID] [-d HOMEDIR] [-s SHELL] <username>\n");
        return 1;
    }

    const char *username = NULL;
    int uid = -1;
    int gid = -1;
    char homedir[128] = "";
    char shell[64] = "/bin/sh";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-u") == 0 && i + 1 < argc) {
            uid = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-g") == 0 && i + 1 < argc) {
            gid = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            strncpy(homedir, argv[++i], sizeof(homedir) - 1);
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            strncpy(shell, argv[++i], sizeof(shell) - 1);
        } else if (argv[i][0] != '-') {
            username = argv[i];
        }
    }

    if (!username) {
        printf("useradd: No username specified\n");
        return 1;
    }

    /* Check if user already exists */
    if (getpwnam(username) != NULL) {
        printf("useradd: user '%s' already exists\n", username);
        return 1;
    }

    /* Find next available UID if not specified */
    if (uid < 0) {
        uid = 1000;
        setpwent();
        struct passwd *pw;
        while ((pw = getpwent()) != NULL) {
            if (pw->pw_uid >= (uid_t)uid) {
                uid = pw->pw_uid + 1;
            }
        }
        endpwent();
    }

    if (gid < 0) {
        gid = uid;
    }

    if (homedir[0] == '\0') {
        snprintf(homedir, sizeof(homedir), "/home/%s", username);
    }

    /* 1. Append to /etc/passwd */
    int fd_passwd = open("/etc/passwd", O_WRONLY | O_APPEND);
    if (fd_passwd < 0) {
        printf("useradd: failed to open /etc/passwd\n");
        return 1;
    }

    char line[256];
    int len = snprintf(line, sizeof(line), "%s:x:%d:%d:%s:%s:%s\n", username, uid, gid, username, homedir, shell);
    write(fd_passwd, line, len);
    close(fd_passwd);

    /* 2. Append to /etc/group */
    int fd_group = open("/etc/group", O_WRONLY | O_APPEND);
    if (fd_group >= 0) {
        len = snprintf(line, sizeof(line), "%s:x:%d:%s\n", username, gid, username);
        write(fd_group, line, len);
        close(fd_group);
    }

    /* 3. Create Home directory */
    mkdir("/home", 0755);
    mkdir(homedir, 0700);
    chown(homedir, (uid_t)uid, (gid_t)gid);

    printf("useradd: user '%s' (UID %d, GID %d, Home: %s) created successfully.\n", username, uid, gid, homedir);
    return 0;
}
