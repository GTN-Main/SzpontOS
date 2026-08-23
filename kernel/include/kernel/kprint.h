#ifndef SZPONTOS_KERNEL_KPRINT_H
#define SZPONTOS_KERNEL_KPRINT_H

#include <kernel/types.h>
#include <stdarg.h>

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO  = 1,
    LOG_LEVEL_WARN  = 2,
    LOG_LEVEL_ERROR = 3,
    LOG_LEVEL_PANIC = 4,
} log_level_t;

void kprint_init(void);
int kprintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
int kvprintf(const char *fmt, va_list args);
int ksnprintf(char *buf, size_t size, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
int kvsnprintf(char *buf, size_t size, const char *fmt, va_list args);

void klog(log_level_t level, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

size_t klog_read_ring(char *dst, size_t max_len, size_t offset);
size_t klog_get_ring_size(void);

#define klog_debug(fmt, ...) klog(LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define klog_info(fmt, ...)  klog(LOG_LEVEL_INFO,  fmt, ##__VA_ARGS__)
#define klog_warn(fmt, ...)  klog(LOG_LEVEL_WARN,  fmt, ##__VA_ARGS__)
#define klog_error(fmt, ...) klog(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define klog_err(fmt, ...)   klog(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)

#endif /* SZPONTOS_KERNEL_KPRINT_H */
