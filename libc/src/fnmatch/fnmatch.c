#include <fnmatch.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>

int fnmatch(const char *pattern, const char *string, int flags) {
    if (!pattern || !string) return FNM_NOMATCH;

    while (*pattern) {
        if (*pattern == '*') {
            while (*pattern == '*') pattern++;
            if (!*pattern) {
                if ((flags & FNM_PATHNAME) && strchr(string, '/')) return FNM_NOMATCH;
                return 0;
            }
            while (*string) {
                if (fnmatch(pattern, string, flags) == 0) return 0;
                if ((flags & FNM_PATHNAME) && *string == '/') return FNM_NOMATCH;
                string++;
            }
            return FNM_NOMATCH;
        } else if (*pattern == '?') {
            if (!*string) return FNM_NOMATCH;
            if ((flags & FNM_PATHNAME) && *string == '/') return FNM_NOMATCH;
            pattern++;
            string++;
        } else if (*pattern == '[') {
            pattern++;
            bool invert = false;
            if (*pattern == '!' || *pattern == '^') {
                invert = true;
                pattern++;
            }
            bool match = false;
            char c = (flags & FNM_CASEFOLD) ? (char)tolower((unsigned char)*string) : *string;
            while (*pattern && *pattern != ']') {
                char p1 = (flags & FNM_CASEFOLD) ? (char)tolower((unsigned char)*pattern) : *pattern;
                pattern++;
                if (*pattern == '-' && *(pattern + 1) && *(pattern + 1) != ']') {
                    pattern++;
                    char p2 = (flags & FNM_CASEFOLD) ? (char)tolower((unsigned char)*pattern) : *pattern;
                    pattern++;
                    if (c >= p1 && c <= p2) match = true;
                } else {
                    if (c == p1) match = true;
                }
            }
            if (*pattern == ']') pattern++;
            if (match == invert || !*string) return FNM_NOMATCH;
            string++;
        } else {
            char p = (flags & FNM_CASEFOLD) ? (char)tolower((unsigned char)*pattern) : *pattern;
            char s = (flags & FNM_CASEFOLD) ? (char)tolower((unsigned char)*string) : *string;
            if (p != s) return FNM_NOMATCH;
            pattern++;
            string++;
        }
    }

    return (*string == '\0') ? 0 : FNM_NOMATCH;
}
int rpl_fnmatch(const char *pattern, const char *string, int flags) {
    return fnmatch(pattern, string, flags);
}
