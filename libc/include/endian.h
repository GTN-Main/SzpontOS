#ifndef _ENDIAN_H
#define _ENDIAN_H

#include <stdint.h>

#define _LITTLE_ENDIAN 1234
#define _BIG_ENDIAN    4321
#define _PDP_ENDIAN    3412

#define LITTLE_ENDIAN  _LITTLE_ENDIAN
#define BIG_ENDIAN     _BIG_ENDIAN
#define PDP_ENDIAN     _PDP_ENDIAN

#if defined(__BYTE_ORDER__)
#define _BYTE_ORDER    __BYTE_ORDER__
#define BYTE_ORDER     __BYTE_ORDER__
#else
#define _BYTE_ORDER    _LITTLE_ENDIAN
#define BYTE_ORDER     _LITTLE_ENDIAN
#endif

#define __LITTLE_ENDIAN _LITTLE_ENDIAN
#define __BIG_ENDIAN    _BIG_ENDIAN
#define __PDP_ENDIAN    _PDP_ENDIAN
#define __BYTE_ORDER    _BYTE_ORDER

#if _BYTE_ORDER == _LITTLE_ENDIAN

#define htobe16(x) __builtin_bswap16((uint16_t)(x))
#define htole16(x) ((uint16_t)(x))
#define be16toh(x) __builtin_bswap16((uint16_t)(x))
#define le16toh(x) ((uint16_t)(x))

#define htobe32(x) __builtin_bswap32((uint32_t)(x))
#define htole32(x) ((uint32_t)(x))
#define be32toh(x) __builtin_bswap32((uint32_t)(x))
#define le32toh(x) ((uint32_t)(x))

#define htobe64(x) __builtin_bswap64((uint64_t)(x))
#define htole64(x) ((uint64_t)(x))
#define be64toh(x) __builtin_bswap64((uint64_t)(x))
#define le64toh(x) ((uint64_t)(x))

#define betoh16(x) be16toh(x)
#define letoh16(x) le16toh(x)
#define betoh32(x) be32toh(x)
#define letoh32(x) le32toh(x)
#define betoh64(x) be64toh(x)
#define letoh64(x) le64toh(x)

#else

#define htobe16(x) ((uint16_t)(x))
#define htole16(x) __builtin_bswap16((uint16_t)(x))
#define be16toh(x) ((uint16_t)(x))
#define le16toh(x) __builtin_bswap16((uint16_t)(x))

#define htobe32(x) ((uint32_t)(x))
#define htole32(x) __builtin_bswap32((uint32_t)(x))
#define be32toh(x) ((uint32_t)(x))
#define le32toh(x) __builtin_bswap32((uint32_t)(x))

#define htobe64(x) ((uint64_t)(x))
#define htole64(x) __builtin_bswap64((uint64_t)(x))
#define be64toh(x) ((uint64_t)(x))
#define le64toh(x) __builtin_bswap64((uint64_t)(x))

#define betoh16(x) be16toh(x)
#define letoh16(x) le16toh(x)
#define betoh32(x) be32toh(x)
#define letoh32(x) le32toh(x)
#define betoh64(x) be64toh(x)
#define letoh64(x) le64toh(x)

#endif

#endif /* _ENDIAN_H */
