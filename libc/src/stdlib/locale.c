/*
 * SzpontOS C Standard Library - Locale and Langinfo
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <locale.h>
#include <langinfo.h>
#include <string.h>

static char g_current_locale[64] = "C.UTF-8";

char *setlocale(int category, const char *locale) {
    (void)category;
    if (!locale) {
        return g_current_locale;
    }
    if (*locale == '\0' || strcmp(locale, "C") == 0 || strcmp(locale, "POSIX") == 0 || strcmp(locale, "C.UTF-8") == 0) {
        strncpy(g_current_locale, locale[0] ? locale : "C.UTF-8", sizeof(g_current_locale) - 1);
        return g_current_locale;
    }
    strncpy(g_current_locale, locale, sizeof(g_current_locale) - 1);
    return g_current_locale;
}

struct lconv *localeconv(void) {
    static struct lconv c_locale = {.decimal_point = ".",
                                    .thousands_sep = "",
                                    .grouping = "",
                                    .int_curr_symbol = "",
                                    .currency_symbol = "",
                                    .mon_decimal_point = "",
                                    .mon_thousands_sep = "",
                                    .mon_grouping = "",
                                    .positive_sign = "",
                                    .negative_sign = "",
                                    .int_frac_digits = 127,
                                    .frac_digits = 127,
                                    .p_cs_precedes = 127,
                                    .p_sep_by_space = 127,
                                    .n_cs_precedes = 127,
                                    .n_sep_by_space = 127,
                                    .p_sign_posn = 127,
                                    .n_sign_posn = 127};
    return &c_locale;
}

char *nl_langinfo(nl_item item) {
    switch (item) {
    case CODESET:
        return "UTF-8";
    case D_T_FMT:
        return "%a %b %e %H:%M:%S %Y";
    case D_FMT:
        return "%m/%d/%y";
    case T_FMT:
        return "%H:%M:%S";
    case RADIXCHAR:
        return ".";
    case THOUSEP:
        return ",";
    case YESEXPR:
        return "^[yY]";
    case NOEXPR:
        return "^[nN]";
    default:
        return "";
    }
}
