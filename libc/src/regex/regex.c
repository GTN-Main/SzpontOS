#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    char *pattern;
    int flags;
} local_regex_t;

int regcomp(regex_t *preg, const char *regex, int cflags) {
    if (!preg || !regex)
        return REG_BADPAT;
    local_regex_t *r = (local_regex_t *)malloc(sizeof(local_regex_t));
    if (!r)
        return REG_ESPACE;
    r->pattern = strdup(regex);
    r->flags = cflags;
    preg->re_g = r;
    preg->re_nsub = 0;
    return 0;
}

int regexec(const regex_t *preg, const char *string, size_t nmatch, regmatch_t pmatch[], int eflags) {
    (void)eflags;
    if (!preg || !preg->re_g || !string)
        return REG_NOMATCH;
    local_regex_t *r = (local_regex_t *)preg->re_g;

    const char *found = NULL;
    if (r->flags & REG_ICASE) {
        found = strcasestr(string, r->pattern);
    } else {
        found = strstr(string, r->pattern);
    }

    if (found) {
        if (nmatch > 0 && pmatch) {
            pmatch[0].rm_so = found - string;
            pmatch[0].rm_eo = pmatch[0].rm_so + strlen(r->pattern);
        }
        return 0;
    }
    return REG_NOMATCH;
}

size_t regerror(int errcode, const regex_t *preg, char *errbuf, size_t errbuf_size) {
    (void)errcode;
    (void)preg;
    if (errbuf && errbuf_size > 0) {
        snprintf(errbuf, errbuf_size, "Regex error");
        return strlen(errbuf);
    }
    return 0;
}

void regfree(regex_t *preg) {
    if (preg && preg->re_g) {
        local_regex_t *r = (local_regex_t *)preg->re_g;
        if (r->pattern)
            free(r->pattern);
        free(r);
        preg->re_g = NULL;
    }
}
