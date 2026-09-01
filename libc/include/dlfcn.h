#ifndef _DLFCN_H
#define _DLFCN_H

#ifdef __cplusplus
extern "C" {
#endif

#define RTLD_LAZY 0x00001
#define RTLD_NOW 0x00002
#define RTLD_GLOBAL 0x00100
#define RTLD_LOCAL 0x00000

#define RTLD_DEFAULT ((void *)0)
typedef struct {
    const char *dli_fname;
    void       *dli_fbase;
    const char *dli_sname;
    void       *dli_saddr;
} Dl_info;

void *dlopen(const char *filename, int flags);
void *dlsym(void *handle, const char *symbol);
int dlclose(void *handle);
char *dlerror(void);
int dladdr(const void *addr, Dl_info *info);

#ifdef __cplusplus
}
#endif

#endif /* _DLFCN_H */
