#include <stdio.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    uid_t uid = getuid();
    gid_t gid = getgid();
    uid_t euid = geteuid();
    gid_t egid = getegid();

    struct passwd *pw = getpwuid(uid);
    struct group *gr = getgrgid(gid);

    const char *uname = pw ? pw->pw_name : "unknown";
    const char *gname = gr ? gr->gr_name : "unknown";

    printf("uid=%d(%s) gid=%d(%s)", (int)uid, uname, (int)gid, gname);

    if (euid != uid) {
        struct passwd *epw = getpwuid(euid);
        printf(" euid=%d(%s)", (int)euid, epw ? epw->pw_name : "unknown");
    }

    if (egid != gid) {
        struct group *egr = getgrgid(egid);
        printf(" egid=%d(%s)", (int)egid, egr ? egr->gr_name : "unknown");
    }

    printf(" groups=%d(%s)\n", (int)gid, gname);
    return 0;
}
