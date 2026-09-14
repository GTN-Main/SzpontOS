#ifndef _SYS_EVENTFD_H
#define _SYS_EVENTFD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t eventfd_t;

#define EFD_SEMAPHORE 0x0001
#define EFD_CLOEXEC   0x00080000
#define EFD_NONBLOCK  0x00000800

int eventfd(unsigned int initval, int flags);
int eventfd_read(int fd, eventfd_t *value);
int eventfd_write(int fd, eventfd_t value);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_EVENTFD_H */
