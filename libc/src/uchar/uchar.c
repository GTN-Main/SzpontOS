#include <uchar.h>
#include <wchar.h>
#include <errno.h>

size_t c16rtomb(char *s, char16_t c16, mbstate_t *ps) {
    (void)ps;
    if (!s) return 1;
    if (c16 < 0x80) {
        *s = (char)c16;
        return 1;
    }
    if (c16 < 0x800) {
        s[0] = (char)(0xC0 | (c16 >> 6));
        s[1] = (char)(0x80 | (c16 & 0x3F));
        return 2;
    }
    s[0] = (char)(0xE0 | (c16 >> 12));
    s[1] = (char)(0x80 | ((c16 >> 6) & 0x3F));
    s[2] = (char)(0x80 | (c16 & 0x3F));
    return 3;
}

size_t mbrtoc16(char16_t *pc16, const char *s, size_t n, mbstate_t *ps) {
    (void)ps;
    if (!s || n == 0) return 0;
    if (pc16) *pc16 = (char16_t)*s;
    return 1;
}

size_t c32rtomb(char *s, char32_t c32, mbstate_t *ps) {
    (void)ps;
    if (!s) return 1;
    if (c32 < 0x80) {
        *s = (char)c32;
        return 1;
    }
    s[0] = (char)(0xF0 | (c32 >> 18));
    s[1] = (char)(0x80 | ((c32 >> 12) & 0x3F));
    s[2] = (char)(0x80 | ((c32 >> 6) & 0x3F));
    s[3] = (char)(0x80 | (c32 & 0x3F));
    return 4;
}

size_t mbrtoc32(char32_t *pc32, const char *s, size_t n, mbstate_t *ps) {
    (void)ps;
    if (!s || n == 0) return 0;
    if (pc32) *pc32 = (char32_t)*s;
    return 1;
}
