/* Native DS framebuffer drawing. GPL-3.0-or-later. */
#ifndef NDS_UI_H
#define NDS_UI_H
#include <stdint.h>
int ndsUiInit(void);
void ndsUiBegin(int screen, uint16_t background);
void ndsUiRect(int x, int y, int w, int h, uint16_t color);
void ndsUiWalkerShell(const uint8_t *pixels, int contrast);
void ndsUiText(int x, int y, int scale, uint16_t color, const char *text,
               int columns);
void ndsUiEnd(void);
#endif
