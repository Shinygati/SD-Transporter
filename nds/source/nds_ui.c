/* Native 256x192 integer renderer. GPL-3.0-or-later. */
#include "nds_ui.h"
#include "../../shared/walker_shell.h"
#include <stdlib.h>
#include <string.h>
#include <fat.h>
#include <nds.h>
static uint16_t frames[2][256 * 192] __attribute__((aligned(32)));
/* The raw asset stores little-endian RGB555 words. */
static const uint8_t walkerShell[] = {
#embed "../../shared/walker-shell-nds.bin"
};
_Static_assert(sizeof(walkerShell) == WALKER_SHELL_DS_SIZE * WALKER_SHELL_DS_SIZE * 2,
               "Unexpected DS shell dimensions");
static int current, storage, changed[2];
static const uint8_t *fontPixels;
static unsigned fontOffset, fontCount;
static uint16_t *vram[2];
int ndsUiInit(void) {
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
  /* No write-back cache: direct_disc.c supplies the alignment adapter still
   * required by Calico when FatFs reads into unaligned or DTCM buffers. */
  storage = fatInit(0, true);
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
void ndsUiWalkerShell(const uint8_t *pixels, int contrast) {
  WalkerShellLayout layout = walkerShellLayout(256, 192);
  const uint8_t *source = walkerShell;
  for (int row = 0; row < layout.size; row++) {
    uint16_t *dst = frames[current] + (layout.y + row) * 256 + layout.x;
    for (int col = 0; col < layout.size; col++, source += 2)
      if (source[1] & 0x80)
        dst[col] = source[0] | (source[1] << 8);
  }

  const uint32_t *palette = walkerShellPalette(contrast);
  uint16_t shades[4];
  for (int shade = 0; shade < 4; shade++) {
    uint32_t rgb = palette[shade];
    shades[shade] = 0x8000 | ((rgb >> 19) & 31) |
                    (((rgb >> 11) & 31) << 5) | (((rgb >> 3) & 31) << 10);
  }
  for (int row = 0; row < layout.lcdHeight; row++) {
    int inset = walkerShellLcdInset(&layout, row);
    ndsUiRect(layout.lcdX + inset, layout.lcdY + row,
              layout.lcdWidth - 2 * inset, 1, shades[0]);
  }
  for (int row = 0; row < layout.pixelsHeight; row++) {
    uint16_t *dst = frames[current] + (layout.pixelsY + row) * 256 + layout.pixelsX;
    for (int col = 0; col < layout.pixelsWidth; col++)
      dst[col] = shades[*pixels++];
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
  swiWaitForVBlank();
  for (int i = 0; i < 2; i++)
    if (changed[i]) {
      DC_FlushRange(frames[i], sizeof(frames[i]));
      dmaCopy(frames[i], vram[i], sizeof(frames[i]));
    }
  changed[0] = changed[1] = 0;
}
