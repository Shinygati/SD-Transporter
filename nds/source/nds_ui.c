/* Native 256x192 integer renderer. GPL-3.0-or-later. */
#include "nds_ui.h"
#include <stdlib.h>
#include <string.h>
#ifndef NDS_UI_PREVIEW
#include <fat.h>
#include <nds.h>
#else
#include <stdio.h>
#endif
#ifdef _MSC_VER
__declspec(align(32)) static uint16_t frames[2][256 * 192];
#else
static uint16_t frames[2][256 * 192] __attribute__((aligned(32)));
#endif
static int current, storage, changed[2];
static const uint8_t *fontPixels;
static unsigned fontOffset, fontCount;
#ifndef NDS_UI_PREVIEW
static uint16_t *vram[2];
#endif
int ndsUiInit(void) {
#ifndef NDS_UI_PREVIEW
  videoSetMode(MODE_5_2D);
  videoSetModeSub(MODE_5_2D);
  vramSetBankA(VRAM_A_MAIN_BG);
  vramSetBankC(VRAM_C_SUB_BG);
  vram[0] = (uint16_t *)bgGetGfxPtr(
      bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0));
  vram[1] = (uint16_t *)bgGetGfxPtr(
      bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0));
  lcdMainOnTop();
  ConsoleFont f = consoleGetDefault()->font;
  fontPixels = (const uint8_t *)f.gfx;
  fontOffset = f.asciiOffset;
  fontCount = f.numChars;
  storage = fatInitDefault();
#else
  static uint8_t pixels[2048];
  FILE *f = fopen(getenv("NDS_PREVIEW_FONT"), "rb");
  if (!f)
    return 0;
  fontCount = fread(pixels, 8, 256, f);
  fclose(f);
  fontPixels = pixels;
  storage = 1;
#endif
  return fontPixels && fontCount;
}
int ndsStorageReady(void) { return storage; }
void ndsUiBegin(int screen, uint16_t background) {
  current = screen;
  changed[screen] = 1;
  uint32_t pair = background | ((uint32_t)background << 16);
  uint32_t *dst = (uint32_t *)frames[screen];
  for (unsigned n = 0; n < 256 * 192 / 2; n++)
    dst[n] = pair;
}
void ndsUiRect(int x, int y, int w, int h, uint16_t color) {
  if (x < 0) {
    w += x;
    x = 0;
  }
  if (y < 0) {
    h += y;
    y = 0;
  }
  if (x + w > 256)
    w = 256 - x;
  if (y + h > 192)
    h = 192 - y;
  if (w <= 0 || h <= 0)
    return;
  for (int row = y; row < y + h; row++) {
    uint16_t *dst = frames[current] + row * 256 + x;
    for (int col = 0; col < w; col++)
      dst[col] = color;
  }
}
static unsigned next(const unsigned char **p) {
  unsigned c = *(*p)++;
  if (c < 128)
    return c;
  if ((c & 0xe0) == 0xc0 && **p) {
    c = (c & 31) << 6;
    c |= *(*p)++ & 63;
  } else {
    while ((**p & 0xc0) == 0x80)
      (*p)++;
    return '?';
  }
  /* Keep labels readable with the native console font. */
  if (c == 0xe9 || c == 0xc9)
    return c == 0xe9 ? 'e' : 'E';
  if (c == 0xb7)
    return '-';
  return c < fontOffset + fontCount ? c : '?';
}
void ndsUiText(int x, int y, int scale, uint16_t color, const char *text,
               int columns) {
  const unsigned char *p = (const unsigned char *)text;
  while (*p && columns-- > 0) {
    unsigned ch = next(&p);
    if (ch < fontOffset || ch >= fontOffset + fontCount)
      ch = '?';
    const uint8_t *glyph = fontPixels + (ch - fontOffset) * 8;
    for (unsigned row = 0; row < 8; row++)
      for (unsigned col = 0; col < 8; col++)
        if (glyph[row] & (1u << col))
          ndsUiRect(x + col * scale, y + row * scale, scale, scale, color);
    x += 8 * scale;
  }
}
void ndsUiEnd(void) {
#ifndef NDS_UI_PREVIEW
  swiWaitForVBlank();
  for (int i = 0; i < 2; i++)
    if (changed[i]) {
      DC_FlushRange(frames[i], sizeof(frames[i]));
      dmaCopy(frames[i], vram[i], sizeof(frames[i]));
    }
#endif
  changed[0] = changed[1] = 0;
}
#ifdef NDS_UI_PREVIEW
void ndsUiWrite(const char *path, int screen) {
  FILE *f = fopen(path, "wb");
  fprintf(f, "P6\n256 192\n255\n");
  for (unsigned n = 0; n < 256 * 192; n++) {
    uint16_t c = frames[screen][n];
    fputc((c & 31) * 255 / 31, f);
    fputc(((c >> 5) & 31) * 255 / 31, f);
    fputc(((c >> 10) & 31) * 255 / 31, f);
  }
  fclose(f);
}
#endif
