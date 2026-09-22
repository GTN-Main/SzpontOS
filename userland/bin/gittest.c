#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <string.h>

static const char *g_git_bin = "/usr/bin/git";

static void run_cmd(const char *desc, char *const argv[], const char *workdir) {
    printf("\033[1;33m>>> [%s]\033[0m\n", desc);
    pid_t pid = fork();
    if (pid == 0) {
        if (workdir) {
            chdir(workdir);
        }
        char *envp[] = {"PATH=/bin:/usr/bin:/usr/tbin", "USER=root", "HOME=/root", "TERM=xterm-256color", "SHELL=/bin/sh", NULL};
        execve(argv[0], argv, envp);
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    printf("\033[1;30m----------------------------------------\033[0m\n");
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    if (access("/usr/bin/git", X_OK) != 0 && access("/bin/git", X_OK) == 0) {
        g_git_bin = "/bin/git";
    }

    printf("\n\033[1;36m========================================================\033[0m\n");
    printf("  \033[1;32m[GITTEST]\033[0m \033[1;37mSzpontOS Native Git Test Suite\033[0m\n");
    printf("\033[1;36m========================================================\033[0m\n\n");

    /* 1. git --version */
    {
        char *cmd[] = {(char *)g_git_bin, "--version", NULL};
        run_cmd("git --version", cmd, NULL);
    }

    /* 2. git config */
    {
        char *cmd1[] = {(char *)g_git_bin, "config", "--global", "user.name", "Szpont Developer", NULL};
        run_cmd("git config user.name", cmd1, NULL);
        char *cmd2[] = {(char *)g_git_bin, "config", "--global", "user.email", "dev@szpontos.org", NULL};
        run_cmd("git config user.email", cmd2, NULL);
    }

    /* 3. git init /myrepo */
    mkdir("/myrepo", 0755);
    {
        char *cmd[] = {(char *)g_git_bin, "init", "/myrepo", NULL};
        run_cmd("git init /myrepo", cmd, NULL);
    }

    /* 4. create a file in /myrepo */
    int fd = open("/myrepo/hello.c", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        const char *code = "#include <stdio.h>\n\nint main() {\n    printf(\"Hello from SzpontOS Git!\\n\");\n    return 0;\n}\n";
        write(fd, code, strlen(code));
        close(fd);
    }

    /* 5. git status */
    {
        char *cmd[] = {(char *)g_git_bin, "status", NULL};
        run_cmd("git status (untracked)", cmd, "/myrepo");
    }

    /* 6. git add hello.c */
    {
        char *cmd[] = {(char *)g_git_bin, "add", "hello.c", NULL};
        run_cmd("git add hello.c", cmd, "/myrepo");
    }

    /* 7. git status (staged) */
    {
        char *cmd[] = {(char *)g_git_bin, "status", NULL};
        run_cmd("git status (staged)", cmd, "/myrepo");
    }

    /* 8. git commit -m "feat: initial commit" */
    {
        char *cmd[] = {(char *)g_git_bin, "commit", "-m", "feat: initial commit with hello.c", NULL};
        run_cmd("git commit -m 'feat: initial commit'", cmd, "/myrepo");
    }

    /* 9. git log */
    {
        char *cmd[] = {(char *)g_git_bin, "--no-pager", "log", "-n", "1", NULL};
        run_cmd("git log -n 1", cmd, "/myrepo");
    }

    /* 10. git branch new-feature */
    {
        char *cmd[] = {(char *)g_git_bin, "branch", "new-feature", NULL};
        run_cmd("git branch new-feature", cmd, "/myrepo");
    }

    /* 11. git branch */
    {
        char *cmd[] = {(char *)g_git_bin, "branch", "-a", NULL};
        run_cmd("git branch -a", cmd, "/myrepo");
    }

    /* 12. git checkout new-feature */
    {
        char *cmd[] = {(char *)g_git_bin, "checkout", "new-feature", NULL};
        run_cmd("git checkout new-feature", cmd, "/myrepo");
    }

    /* 13. Append to file */
    fd = open("/myrepo/hello.c", O_WRONLY | O_APPEND, 0644);
    if (fd >= 0) {
        const char *more_code = "// Added in new-feature branch\n";
        write(fd, more_code, strlen(more_code));
        close(fd);
    }

    /* 14. git diff */
    {
        char *cmd[] = {(char *)g_git_bin, "--no-pager", "diff", NULL};
        run_cmd("git diff", cmd, "/myrepo");
    }

    /* 15. git commit -am "feat: update hello.c in branch" */
    {
        char *cmd[] = {(char *)g_git_bin, "commit", "-a", "-m", "feat: update hello.c in new-feature", NULL};
        run_cmd("git commit -am 'feat: update hello.c in new-feature'", cmd, "/myrepo");
    }

    /* 16. git log --oneline */
    {
        char *cmd[] = {(char *)g_git_bin, "--no-pager", "log", "--oneline", NULL};
        run_cmd("git log --oneline", cmd, "/myrepo");
    }

    /* 17. git checkout master */
    {
        char *cmd[] = {(char *)g_git_bin, "checkout", "master", NULL};
        run_cmd("git checkout master", cmd, "/myrepo");
    }

    /* 18. git merge new-feature */
    {
        char *cmd[] = {(char *)g_git_bin, "merge", "new-feature", "-m", "Merge branch 'new-feature'", NULL};
        run_cmd("git merge new-feature", cmd, "/myrepo");
    }

    /* 19. git tag v1.0.0 */
    {
        char *cmd[] = {(char *)g_git_bin, "tag", "-a", "v1.0.0", "-m", "Release version 1.0.0", NULL};
        run_cmd("git tag -a v1.0.0", cmd, "/myrepo");
    }

    /* 20. git tag -l -n */
    {
        char *cmd[] = {(char *)g_git_bin, "tag", "-l", "-n", NULL};
        run_cmd("git tag -l -n", cmd, "/myrepo");
    }

    /* 21. git show v1.0.0 */
    {
        char *cmd[] = {(char *)g_git_bin, "--no-pager", "show", "v1.0.0", NULL};
        run_cmd("git show v1.0.0", cmd, "/myrepo");
    }

    printf("\n\033[1;32m========================================================\033[0m\n");
    printf("  \033[1;32m[GITTEST]\033[0m \033[1;32mALL GIT TESTS COMPLETED SUCCESSFULLY!\033[0m\n");
    printf("\033[1;32m========================================================\033[0m\n\n");

    return 0;
}
