/*
 * SzpontOS C Standard Library - POSIX/Linux signalfd definitions
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef _SYS_SIGNALFD_H
#define _SYS_SIGNALFD_H

#include <stdint.h>
#include <stddef.h>
#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _UAPI_LINUX_SIGNALFD_H
#define _UAPI_LINUX_SIGNALFD_H

#define SFD_CLOEXEC  0x80000
#define SFD_NONBLOCK 0x800

struct signalfd_siginfo {
    uint32_t ssi_signo;
    int32_t  ssi_errno;
    int32_t  ssi_code;
    uint32_t ssi_pid;
    uint32_t ssi_uid;
    int32_t  ssi_fd;
    uint32_t ssi_tid;
    uint32_t ssi_band;
    uint32_t ssi_overrun;
    uint32_t ssi_trapno;
    int32_t  ssi_status;
    int32_t  ssi_int;
    uint64_t ssi_ptr;
    uint64_t ssi_utime;
    uint64_t ssi_stime;
    uint64_t ssi_addr;
    uint16_t ssi_addr_lsb;
    uint16_t __pad2;
    int32_t  ssi_syscall;
    uint64_t ssi_call_addr;
    uint32_t ssi_arch;
    uint8_t  __pad[28];
};
#endif /* _UAPI_LINUX_SIGNALFD_H */

int signalfd(int fd, const sigset_t *mask, int flags);
int signalfd4(int fd, const sigset_t *mask, size_t sizemask, int flags);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_SIGNALFD_H */
