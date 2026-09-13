#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

/*
 * Dynamic environment management for SzpontOS.
 * Provides standard POSIX getenv, setenv, unsetenv, putenv, clearenv.
 *
 * `environ` points to a NULL-terminated array of char pointers.
 * It is initially populated by crt0.asm from the initial stack frame,
 * or points to g_default_environ.
 *
 * When environ is resized, we allocate the pointer array dynamically
 * and track it via g_allocated_environ. We also track any strings
 * allocated by setenv in g_allocated_strings so we can free
 * them when replaced/unset without touching read-only or stack memory.
 */

static char **g_allocated_environ = NULL;
static char **g_allocated_strings = NULL;
static size_t g_allocated_strings_count = 0;
static size_t g_allocated_strings_cap = 0;

static void track_allocated_string(char *s) {
    if (!s) return;
    if (g_allocated_strings_count >= g_allocated_strings_cap) {
        size_t new_cap = g_allocated_strings_cap ? g_allocated_strings_cap * 2 : 32;
        char **new_list = (char **)realloc(g_allocated_strings, new_cap * sizeof(char *));
        if (!new_list) return;
        g_allocated_strings = new_list;
        g_allocated_strings_cap = new_cap;
    }
    g_allocated_strings[g_allocated_strings_count++] = s;
}

static void free_if_allocated_string(char *s) {
    if (!s) return;
    for (size_t i = 0; i < g_allocated_strings_count; i++) {
        if (g_allocated_strings[i] == s) {
            g_allocated_strings[i] = g_allocated_strings[--g_allocated_strings_count];
            free(s);
            return;
        }
    }
}

char *getenv(const char *name) {
    if (!name || !*name || strchr(name, '=') || !environ)
        return NULL;

    size_t len = strlen(name);
    for (char **ep = environ; *ep; ep++) {
        if (strncmp(*ep, name, len) == 0 && (*ep)[len] == '=') {
            return *ep + len + 1;
        }
    }
    return NULL;
}

static size_t environ_len(void) {
    if (!environ) return 0;
    size_t count = 0;
    while (environ[count]) count++;
    return count;
}

int setenv(const char *name, const char *value, int overwrite) {
    if (!name || !*name || strchr(name, '=') || !value) {
        errno = EINVAL;
        return -1;
    }

    size_t name_len = strlen(name);
    size_t val_len = strlen(value);

    /* Check if variable already exists */
    if (environ) {
        for (size_t i = 0; environ[i]; i++) {
            if (strncmp(environ[i], name, name_len) == 0 && environ[i][name_len] == '=') {
                if (!overwrite)
                    return 0;

                /* Format "name=value" */
                char *new_str = (char *)malloc(name_len + 1 + val_len + 1);
                if (!new_str) {
                    errno = ENOMEM;
                    return -1;
                }
                memcpy(new_str, name, name_len);
                new_str[name_len] = '=';
                memcpy(new_str + name_len + 1, value, val_len + 1);

                char *old_str = environ[i];
                environ[i] = new_str;
                track_allocated_string(new_str);
                free_if_allocated_string(old_str);
                return 0;
            }
        }
    }

    /* Variable does not exist: append to environ */
    size_t count = environ_len();
    size_t new_size = (count + 2) * sizeof(char *);
    char **new_env = NULL;

    if (environ == g_allocated_environ && g_allocated_environ != NULL) {
        new_env = (char **)realloc(g_allocated_environ, new_size);
        if (!new_env) {
            errno = ENOMEM;
            return -1;
        }
    } else {
        new_env = (char **)malloc(new_size);
        if (!new_env) {
            errno = ENOMEM;
            return -1;
        }
        if (environ) {
            memcpy(new_env, environ, count * sizeof(char *));
        }
    }

    char *new_str = (char *)malloc(name_len + 1 + val_len + 1);
    if (!new_str) {
        if (new_env != g_allocated_environ) free(new_env);
        errno = ENOMEM;
        return -1;
    }
    memcpy(new_str, name, name_len);
    new_str[name_len] = '=';
    memcpy(new_str + name_len + 1, value, val_len + 1);

    new_env[count] = new_str;
    new_env[count + 1] = NULL;

    g_allocated_environ = new_env;
    environ = new_env;
    track_allocated_string(new_str);

    return 0;
}

int unsetenv(const char *name) {
    if (!name || !*name || strchr(name, '=')) {
        errno = EINVAL;
        return -1;
    }
    if (!environ)
        return 0;

    size_t name_len = strlen(name);
    for (size_t i = 0; environ[i]; i++) {
        if (strncmp(environ[i], name, name_len) == 0 && environ[i][name_len] == '=') {
            char *old_str = environ[i];
            /* Shift remaining pointers down */
            for (size_t j = i; environ[j]; j++) {
                environ[j] = environ[j + 1];
            }
            free_if_allocated_string(old_str);
            /* i remains same to check if duplicate entries exist */
            i--;
        }
    }
    return 0;
}

int putenv(char *string) {
    if (!string || !*string) {
        errno = EINVAL;
        return -1;
    }
    char *eq = strchr(string, '=');
    if (!eq) {
        /* POSIX: if no '=', remove name from environment */
        return unsetenv(string);
    }

    size_t name_len = (size_t)(eq - string);

    /* Check if already in environ */
    if (environ) {
        for (size_t i = 0; environ[i]; i++) {
            if (strncmp(environ[i], string, name_len) == 0 && environ[i][name_len] == '=') {
                char *old_str = environ[i];
                environ[i] = string;
                free_if_allocated_string(old_str);
                return 0;
            }
        }
    }

    /* Append string directly (putenv uses pointer directly per POSIX) */
    size_t count = environ_len();
    size_t new_size = (count + 2) * sizeof(char *);
    char **new_env = NULL;

    if (environ == g_allocated_environ && g_allocated_environ != NULL) {
        new_env = (char **)realloc(g_allocated_environ, new_size);
        if (!new_env) {
            errno = ENOMEM;
            return -1;
        }
    } else {
        new_env = (char **)malloc(new_size);
        if (!new_env) {
            errno = ENOMEM;
            return -1;
        }
        if (environ) {
            memcpy(new_env, environ, count * sizeof(char *));
        }
    }

    new_env[count] = string;
    new_env[count + 1] = NULL;

    g_allocated_environ = new_env;
    environ = new_env;
    return 0;
}

int clearenv(void) {
    if (environ) {
        for (size_t i = 0; environ[i]; i++) {
            free_if_allocated_string(environ[i]);
        }
        if (environ == g_allocated_environ) {
            free(g_allocated_environ);
            g_allocated_environ = NULL;
        }
        environ = NULL;
    }
    return 0;
}
