#ifndef SZPONTOS_DRIVERS_FONT8X16_H
#define SZPONTOS_DRIVERS_FONT8X16_H

#include <stdint.h>

#define FONT_WIDTH 8
#define FONT_HEIGHT 16

/* Standard 8x16 bitmap font (CP437/ASCII) */
extern const uint8_t g_font_8x16[256][16];

#endif /* SZPONTOS_DRIVERS_FONT8X16_H */
