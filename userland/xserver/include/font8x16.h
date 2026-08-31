/*
 * SzpontOS - SzpontX11 Native X11 Server
 * (C) Copyright by Szpont Industries. All rights reserved.
 *
 * 8x16 Bitmap Font Table (CP437 / ISO-8859-1)
 */

#ifndef SZPONT_FONT8X16_H
#define SZPONT_FONT8X16_H

#include <stdint.h>

#define FONT_WIDTH  8
#define FONT_HEIGHT 16
#define FONT_ASCENT 12
#define FONT_DESCENT 4

extern const uint8_t g_szpont_font8x16[256][16];

#endif /* SZPONT_FONT8X16_H */
