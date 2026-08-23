#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: chown <owner>[:<group>] <file...>\n");
        return 1;
    }

    char owner_spec[128];
    strncpy(owner_spec, argv[1], sizeof(owner_spec) - 1);
    owner_spec[sizeof(owner_spec) - 1] = '\0';

    char *group_name = strchr(owner_spec, ':');
    if (!group_name) {
        group_name = strchr(owner_spec, '.');
    }

    if (group_name) {
        *group_name = '\0';
        group_name++;
    }

    uid_t uid = (uid_t)-1;
    gid_t gid = (gid_t)-1;

    if (owner_spec[0] != '\0') {
        struct passwd *pw = getpwnam(owner_spec);
        if (pw) {
            uid = pw->pw_uid;
        } else {
            uid = (uid_t)atoi(owner_spec);
        }
    }

    if (group_name && *group_name) {
        struct group *gr = getgrnam(group_name);
        if (gr) {
            gid = gr->gr_gid;
        } else {
            gid = (gid_t)atoi(group_name);
        }
    }

    for (int i = 2; i < argc; i++) {
        if (chown(argv[i], uid, gid) != 0) {
            printf("chown: cannot change ownership of '%s': Permission denied or not found\n", argv[i]);
        }
    }

    return 0;
}
