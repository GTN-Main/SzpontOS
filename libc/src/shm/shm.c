/*
 * SzpontOS Libc - System V Shared Memory & POSIX Shared Memory Implementation
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>

extern int64_t __syscall1(int64_t num, int64_t a1);
extern int64_t __syscall2(int64_t num, int64_t a1, int64_t a2);
extern int64_t __syscall3(int64_t num, int64_t a1, int64_t a2, int64_t a3);

static inline int64_t __check_syscall(int64_t ret) {
    if (ret < 0) {
        errno = (int)-ret;
        return -1;
    }
    return ret;
}

key_t ftok(const char *pathname, int proj_id) {
    struct stat st;
    if (stat(pathname, &st) < 0) {
        return (key_t)-1;
    }
    return (key_t)((proj_id & 0xFF) << 24 | ((st.st_dev & 0xFF) << 16) | (st.st_ino & 0xFFFF));
}

int shmget(key_t key, size_t size, int shmflg) {
    int64_t ret = __syscall3(SYS_shmget, (int64_t)key, (int64_t)size, (int64_t)shmflg);
    return (int)__check_syscall(ret);
}

void *shmat(int shmid, const void *shmaddr, int shmflg) {
    int64_t ret = __syscall3(SYS_shmat, (int64_t)shmid, (int64_t)shmaddr, (int64_t)shmflg);
    if (ret < 0 && ret > -4096) {
        errno = (int)-ret;
        return (void *)-1;
    }
    return (void *)ret;
}

int shmdt(const void *shmaddr) {
    int64_t ret = __syscall1(SYS_shmdt, (int64_t)shmaddr);
    return (int)__check_syscall(ret);
}

int shmctl(int shmid, int cmd, struct shmid_ds *buf) {
    int64_t ret = __syscall3(SYS_shmctl, (int64_t)shmid, (int64_t)cmd, (int64_t)buf);
    return (int)__check_syscall(ret);
}

int memfd_create(const char *name, unsigned int flags) {
    int64_t ret = __syscall2(SYS_memfd_create, (int64_t)name, (int64_t)flags);
    return (int)__check_syscall(ret);
}
