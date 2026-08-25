#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

extern int64_t __syscall3(int64_t num, int64_t a1, int64_t a2, int64_t a3);
#define SYS_getdents 78

DIR *opendir(const char *name) {
    int fd = open(name, O_RDONLY);
    if (fd < 0)
        return NULL;

    DIR *dir = (DIR *)malloc(sizeof(DIR));
    if (!dir) {
        close(fd);
        return NULL;
    }
    dir->fd = fd;
    memset(&dir->current, 0, sizeof(struct dirent));
    return dir;
}

DIR *fdopendir(int fd) {
    if (fd < 0)
        return NULL;
    DIR *dir = (DIR *)malloc(sizeof(DIR));
    if (!dir)
        return NULL;
    dir->fd = fd;
    memset(&dir->current, 0, sizeof(struct dirent));
    return dir;
}

int dirfd(DIR *dirp) {
    if (!dirp)
        return -1;
    return dirp->fd;
}

struct dirent *readdir(DIR *dirp) {
    if (!dirp || dirp->fd < 0)
        return NULL;

    ssize_t bytes = __syscall3(SYS_getdents, dirp->fd, (int64_t)&dirp->current, sizeof(struct dirent));
    if (bytes <= 0) {
        return NULL;
    }
    return &dirp->current;
}

void rewinddir(DIR *dirp) {
    if (!dirp || dirp->fd < 0)
        return;
    lseek(dirp->fd, 0, SEEK_SET);
}

int closedir(DIR *dirp) {
    if (!dirp)
        return -1;
    int ret = close(dirp->fd);
    free(dirp);
    return ret;
}
