#ifndef _LIBC_LINUX_INPUT_H
#define _LIBC_LINUX_INPUT_H

#include <stdint.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <linux/input-event-codes.h>

struct input_event {
    struct timeval time;
    uint16_t type;
    uint16_t code;
    int32_t value;
};

struct input_id {
    uint16_t bustype;
    uint16_t vendor;
    uint16_t product;
    uint16_t version;
};

#define EVIOCGVERSION   _IOR('E', 0x01, int)
#define EVIOCGID        _IOR('E', 0x02, struct input_id)
#define EVIOCGREP       _IOR('E', 0x03, unsigned int[2])
#define EVIOCSREP       _IOW('E', 0x03, unsigned int[2])
#define EVIOCGKEYCODE   _IOR('E', 0x04, unsigned int[2])
#define EVIOCSKEYCODE   _IOW('E', 0x04, unsigned int[2])
#define EVIOCGNAME(len) _IOC(_IOC_READ, 'E', 0x06, len)
#define EVIOCGPHYS(len) _IOC(_IOC_READ, 'E', 0x07, len)
#define EVIOCGUNIQ(len) _IOC(_IOC_READ, 'E', 0x08, len)
#define EVIOCGPROP(len) _IOC(_IOC_READ, 'E', 0x09, len)
#define EVIOCGBIT(ev,len) _IOC(_IOC_READ, 'E', 0x20 + (ev), len)

#endif /* _LIBC_LINUX_INPUT_H */
