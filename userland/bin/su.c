#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>

int main(int argc, char *argv[]) {
    const char *target_user = (argc > 1) ? argv[1] : "root";

    struct passwd *pw = getpwnam(target_user);
    if (!pw) {
        printf("su: user '%s' does not exist\n", target_user);
        return 1;
    }

    /* If not root, check permissions */
    if (geteuid() != 0) {
        printf("su: Permission denied (must be root to switch user without password)\n");
        return 1;
    }

    /* Change credentials */
    if (setgid(pw->pw_gid) != 0) {
        printf("su: setgid failed\n");
        return 1;
    }

    if (setuid(pw->pw_uid) != 0) {
        printf("su: setuid failed\n");
        return 1;
    }

    /* Switch working directory to user home directory */
    if (pw->pw_dir && *pw->pw_dir) {
        chdir(pw->pw_dir);
    }

    const char *shell = (pw->pw_shell && *pw->pw_shell) ? pw->pw_shell : "/bin/sh";
    char *sh_argv[] = { (char *)shell, NULL };
    char env_home[128], env_user[128], env_shell[128];
    snprintf(env_home, sizeof(env_home), "HOME=%s", pw->pw_dir);
    snprintf(env_user, sizeof(env_user), "USER=%s", pw->pw_name);
    snprintf(env_shell, sizeof(env_shell), "SHELL=%s", shell);
    char *envp[] = { env_home, env_user, env_shell, "PATH=/bin", NULL };

    execve(shell, sh_argv, envp);

    printf("su: failed to execute shell '%s'\n", shell);
    return 1;
}
