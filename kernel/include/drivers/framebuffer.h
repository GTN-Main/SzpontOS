#ifndef SZPONTOS_DRIVERS_FRAMEBUFFER_H
#define SZPONTOS_DRIVERS_FRAMEBUFFER_H

#include <kernel/types.h>
#include <limine.h>

#define FB_COLOR_BLACK 0x00151828
#define FB_COLOR_DARK_BLUE 0x001e2337
#define FB_COLOR_RED 0x00ff5370
#define FB_COLOR_GREEN 0x00c3e88d
#define FB_COLOR_YELLOW 0x00ffcb6b
#define FB_COLOR_BLUE 0x0082aaff
#define FB_COLOR_MAGENTA 0x00c792ea
#define FB_COLOR_CYAN 0x0089ddff
#define FB_COLOR_WHITE 0x00ffffff
#define FB_COLOR_GRAY 0x00717cb4
#define FB_COLOR_BG 0x000f111a

void framebuffer_init(struct limine_framebuffer *fb);
void framebuffer_init_backbuffer(void);
bool framebuffer_is_available(void);

void fb_clear(uint32_t color);
void fb_put_pixel(size_t x, size_t y, uint32_t color);
void fb_fill_rect(size_t x, size_t y, size_t w, size_t h, uint32_t color);

void fb_console_set_color(uint32_t fg, uint32_t bg);
void fb_console_putc(char c);
void fb_console_puts(const char *str);
void fb_console_write(const char *buf, size_t len);
void fb_console_clear(void);
void fb_flush(void);

size_t fb_get_width(void);
size_t fb_get_height(void);
size_t fb_get_cols(void);
size_t fb_get_rows(void);

#endif /* SZPONTOS_DRIVERS_FRAMEBUFFER_H */
