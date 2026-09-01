/*
 * SzpontOS - Kernel Shared Memory Subsystem Header (SYSV IPC SHM)
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef SZPONTOS_MM_SHM_H
#define SZPONTOS_MM_SHM_H

#include <kernel/types.h>
#include <kernel/spinlock.h>

#define IPC_PRIVATE 0
#define IPC_CREAT  01000
#define IPC_EXCL   02000
#define IPC_NOWAIT 04000

#define IPC_RMID 0
#define IPC_SET  1
#define IPC_STAT 2
#define IPC_INFO 3

#define SHM_RDONLY 010000
#define SHM_RND    020000
#define SHM_REMAP  040000
#define SHM_EXEC   0100000

#define SHM_LOCK   11
#define SHM_UNLOCK 12
#define SHM_STAT   13
#define SHM_INFO   14

#define MAX_SHM_SEGMENTS 128
#define MAX_PROC_SHM     32

struct ipc_perm {
    key_t key;
    uid_t uid;
    gid_t gid;
    uid_t cuid;
    gid_t cgid;
    unsigned short mode;
    unsigned short __pad1;
    unsigned short seq;
    unsigned short __pad2;
    unsigned long __unused1;
    unsigned long __unused2;
};

struct shmid_ds {
    struct ipc_perm shm_perm;
    size_t shm_segsz;
    time_t shm_atime;
    time_t shm_dtime;
    time_t shm_ctime;
    pid_t shm_cpid;
    pid_t shm_lpid;
    unsigned long shm_nattch;
    unsigned long __unused4;
    unsigned long __unused5;
};

struct shminfo {
    unsigned long shmmax;
    unsigned long shmmin;
    unsigned long shmmni;
    unsigned long shmseg;
    unsigned long shmall;
    unsigned long __unused1;
    unsigned long __unused2;
    unsigned long __unused3;
    unsigned long __unused4;
};

typedef struct shm_segment {
    int shmid;
    key_t key;
    size_t size;
    size_t num_pages;
    uintptr_t *phys_pages;
    int attach_count;
    pid_t creator_pid;
    pid_t last_pid;
    time_t atime;
    time_t dtime;
    time_t ctime;
    mode_t mode;
    uid_t uid;
    gid_t gid;
    bool allocated;
    bool marked_rmid;
} shm_segment_t;

typedef struct proc_shm_mapping {
    int shmid;
    uintptr_t vaddr;
    size_t size;
    bool active;
} proc_shm_mapping_t;

/* Forward declaration for process_t */
struct process;

void shm_init(void);
int shm_get(key_t key, size_t size, int shmflg, int *out_shmid);
void *shm_at(int shmid, const void *shmaddr, int shmflg, struct process *proc);
int shm_dt(const void *shmaddr, struct process *proc);
int shm_ctl(int shmid, int cmd, struct shmid_ds *buf, struct process *proc);
void shm_process_exit(struct process *proc);

#endif /* SZPONTOS_MM_SHM_H */
