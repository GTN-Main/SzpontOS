#ifndef _LINUX_TYPES_H
#define _LINUX_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <asm/types.h>

typedef uint16_t __le16;
typedef uint32_t __le32;
typedef uint64_t __le64;
typedef uint16_t __be16;
typedef uint32_t __be32;
typedef uint64_t __be64;

typedef size_t __kernel_size_t;

#endif /* _LINUX_TYPES_H */
