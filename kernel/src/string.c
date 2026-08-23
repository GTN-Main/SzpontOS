#include <kernel/string.h>
#include <mm/heap.h>

void *memcpy(void *dest, const void *src, size_t n) {
    if (!dest || !src || n == 0) return dest;
#if defined(__x86_64__)
    void *d = dest;
    const void *s = src;
    __asm__ volatile (
        "mov %2, %%rcx\n\t"
        "shr $3, %%rcx\n\t"
        "rep movsq\n\t"
        "mov %2, %%rcx\n\t"
        "and $7, %%rcx\n\t"
        "rep movsb"
        : "+D"(d), "+S"(s)
        : "r"(n)
        : "rcx", "memory"
    );
    return dest;
#else
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    size_t qwords = n >> 3;
    uint64_t *d64 = (uint64_t *)d;
    const uint64_t *s64 = (const uint64_t *)s;
    while (qwords--) {
        *d64++ = *s64++;
    }
    d = (uint8_t *)d64;
    s = (const uint8_t *)s64;
    size_t rem = n & 7;
    while (rem--) {
        *d++ = *s++;
    }
    return dest;
#endif
}

void *memset(void *s, int c, size_t n) {
    if (!s || n == 0) return s;
    uint8_t byte = (uint8_t)c;
#if defined(__x86_64__)
    void *d = s;
    uint64_t val64 = 0x0101010101010101ULL * byte;
    __asm__ volatile (
        "mov %2, %%rcx\n\t"
        "shr $3, %%rcx\n\t"
        "rep stosq\n\t"
        "mov %2, %%rcx\n\t"
        "and $7, %%rcx\n\t"
        "rep stosb"
        : "+D"(d)
        : "a"(val64), "r"(n)
        : "rcx", "memory"
    );
    return s;
#else
    uint8_t *p = (uint8_t *)s;
    uint64_t val64 = 0x0101010101010101ULL * byte;
    size_t qwords = n >> 3;
    uint64_t *p64 = (uint64_t *)p;
    while (qwords--) {
        *p64++ = val64;
    }
    p = (uint8_t *)p64;
    size_t rem = n & 7;
    while (rem--) {
        *p++ = byte;
    }
    return s;
#endif
}

void *memmove(void *dest, const void *src, size_t n) {
    if (!dest || !src || n == 0 || dest == src) return dest;

    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;

    if (d < s || d >= (s + n)) {
        return memcpy(dest, src, n);
    }

    /* Backward copy for overlapping regions */
    d += n;
    s += n;
    size_t rem = n & 7;
    while (rem--) {
        *--d = *--s;
    }
    size_t qwords = n >> 3;
    uint64_t *d64 = (uint64_t *)d;
    const uint64_t *s64 = (const uint64_t *)s;
    while (qwords--) {
        *--d64 = *--s64;
    }
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }
    return 0;
}

size_t strlen(const char *s) {
    size_t len = 0;
    while (s && s[len]) {
        len++;
    }
    return len;
}

size_t strnlen(const char *s, size_t maxlen) {
    size_t len = 0;
    while (s && len < maxlen && s[len]) {
        len++;
    }
    return len;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i] || s1[i] == '\0') {
            return (unsigned char)s1[i] - (unsigned char)s2[i];
        }
    }
    return 0;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

char *strcat(char *dest, const char *src) {
    char *d = dest + strlen(dest);
    while ((*d++ = *src++));
    return dest;
}

char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return (*s == (char)c) ? (char *)s : NULL;
}

char *strrchr(const char *s, int c) {
    const char *last = NULL;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    return (last != NULL || *s == (char)c) ? (char *)(last ? last : s) : NULL;
}

char *strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *copy = (char *)kmalloc(len + 1);
    if (!copy) return NULL;
    memcpy(copy, s, len + 1);
    return copy;
}
