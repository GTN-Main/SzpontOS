#include <spawn.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>

struct spawn_action {
    int type; // 1: dup2, 2: close, 3: open
    int fd;
    int newfd;
    char *path;
    int oflag;
    mode_t mode;
};

int posix_spawn_file_actions_init(posix_spawn_file_actions_t *file_actions) {
    if (!file_actions) return EINVAL;
    file_actions->allocated = 8;
    file_actions->used = 0;
    file_actions->actions = malloc(8 * sizeof(struct spawn_action));
    return 0;
}

int posix_spawn_file_actions_destroy(posix_spawn_file_actions_t *file_actions) {
    if (!file_actions) return EINVAL;
    if (file_actions->actions) {
        free(file_actions->actions);
        file_actions->actions = NULL;
    }
    file_actions->allocated = 0;
    file_actions->used = 0;
    return 0;
}

int posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t *file_actions, int fd, int newfd) {
    if (!file_actions) return EINVAL;
    if (file_actions->used >= file_actions->allocated) {
        file_actions->allocated *= 2;
        file_actions->actions = realloc(file_actions->actions, file_actions->allocated * sizeof(struct spawn_action));
    }
    struct spawn_action *acts = (struct spawn_action *)file_actions->actions;
    acts[file_actions->used].type = 1;
    acts[file_actions->used].fd = fd;
    acts[file_actions->used].newfd = newfd;
    acts[file_actions->used].path = NULL;
    file_actions->used++;
    return 0;
}

int posix_spawn_file_actions_addclose(posix_spawn_file_actions_t *file_actions, int fd) {
    if (!file_actions) return EINVAL;
    if (file_actions->used >= file_actions->allocated) {
        file_actions->allocated *= 2;
        file_actions->actions = realloc(file_actions->actions, file_actions->allocated * sizeof(struct spawn_action));
    }
    struct spawn_action *acts = (struct spawn_action *)file_actions->actions;
    acts[file_actions->used].type = 2;
    acts[file_actions->used].fd = fd;
    acts[file_actions->used].path = NULL;
    file_actions->used++;
    return 0;
}

int posix_spawn_file_actions_addopen(posix_spawn_file_actions_t *file_actions, int fd, const char *path, int oflag, mode_t mode) {
    if (!file_actions) return EINVAL;
    if (file_actions->used >= file_actions->allocated) {
        file_actions->allocated *= 2;
        file_actions->actions = realloc(file_actions->actions, file_actions->allocated * sizeof(struct spawn_action));
    }
    struct spawn_action *acts = (struct spawn_action *)file_actions->actions;
    acts[file_actions->used].type = 3;
    acts[file_actions->used].fd = fd;
    acts[file_actions->used].path = (char *)path;
    acts[file_actions->used].oflag = oflag;
    acts[file_actions->used].mode = mode;
    file_actions->used++;
    return 0;
}

int posix_spawn(pid_t *pid, const char *path,
                const posix_spawn_file_actions_t *file_actions,
                const posix_spawnattr_t *attrp,
                char *const argv[], char *const envp[]) {
    (void)attrp;
    pid_t child = fork();
    if (child < 0) {
        return errno;
    }
    if (child == 0) {
        if (file_actions && file_actions->actions) {
            struct spawn_action *acts = (struct spawn_action *)file_actions->actions;
            for (int i = 0; i < file_actions->used; i++) {
                if (acts[i].type == 1) {
                    dup2(acts[i].fd, acts[i].newfd);
                } else if (acts[i].type == 2) {
                    close(acts[i].fd);
                } else if (acts[i].type == 3) {
                    int fd = open(acts[i].path, acts[i].oflag, acts[i].mode);
                    if (fd >= 0 && fd != acts[i].fd) {
                        dup2(fd, acts[i].fd);
                        close(fd);
                    }
                }
            }
        }
        execve(path, argv, envp ? envp : environ);
        _exit(127);
    }
    if (pid) *pid = child;
    return 0;
}

int posix_spawnp(pid_t *pid, const char *file,
                 const posix_spawn_file_actions_t *file_actions,
                 const posix_spawnattr_t *attrp,
                 char *const argv[], char *const envp[]) {
    (void)attrp;
    pid_t child = fork();
    if (child < 0) {
        return errno;
    }
    if (child == 0) {
        if (file_actions && file_actions->actions) {
            struct spawn_action *acts = (struct spawn_action *)file_actions->actions;
            for (int i = 0; i < file_actions->used; i++) {
                if (acts[i].type == 1) {
                    dup2(acts[i].fd, acts[i].newfd);
                } else if (acts[i].type == 2) {
                    close(acts[i].fd);
                } else if (acts[i].type == 3) {
                    int fd = open(acts[i].path, acts[i].oflag, acts[i].mode);
                    if (fd >= 0 && fd != acts[i].fd) {
                        dup2(fd, acts[i].fd);
                        close(fd);
                    }
                }
            }
        }
        execvp(file, argv);
        _exit(127);
    }
    if (pid) *pid = child;
    return 0;
}
