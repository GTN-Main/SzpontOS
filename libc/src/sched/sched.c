/*
 * SzpontOS - POSIX Process Scheduling Implementation
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <sched.h>
#include <sys/syscall.h>
#include <errno.h>

extern int64_t __syscall0(int64_t sys_no);
extern int64_t __syscall1(int64_t sys_no, int64_t arg1);
extern int64_t __syscall2(int64_t sys_no, int64_t arg1, int64_t arg2);

int sched_yield(void) {
    return (int)__syscall0(SYS_yield);
}

int sched_get_priority_max(int policy) {
    (void)policy;
    return 99;
}

int sched_get_priority_min(int policy) {
    (void)policy;
    return 1;
}
