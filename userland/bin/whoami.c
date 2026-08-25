#include <stdio.h>
#include <unistd.h>
#include <pwd.h>

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    uid_t euid = geteuid();
    struct passwd *pw = getpwuid(euid);

    if (pw && pw->pw_name) {
        printf("%s\n", pw->pw_name);
    } else {
        printf("uid%d\n", (int)euid);
    }
    return 0;
}
