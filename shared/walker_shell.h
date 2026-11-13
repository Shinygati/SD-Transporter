/* Console layout for the same Pokéwalker shell used on phones. GPL-3.0-or-later. */
#ifndef WALKER_SHELL_H
#define WALKER_SHELL_H

#include <stdint.h>

enum { WALKER_SHELL_DS_SIZE = 184, WALKER_SHELL_3DS_SIZE = 232 };

typedef struct {
  int x, y, size;
  int lcdX, lcdY, lcdWidth, lcdHeight;
  int pixelsX, pixelsY, pixelsWidth, pixelsHeight;
} WalkerShellLayout;

static inline WalkerShellLayout walkerShellAt(int x, int y, int size) {
  /* LCD window coordinates in the original 1005 x 1003 image. */
  int left = (size * 230 + 502) / 1005;
  int right = (size * 783 + 502) / 1005;
  int top = (size * 261 + 501) / 1003;
  int bottom = (size * 644 + 501) / 1003;
  int lcdWidth = right - left, lcdHeight = bottom - top;
  /* Keep the 96 x 64 LCD at native pixel size on DS. */
  int pixelsWidth = size == WALKER_SHELL_DS_SIZE ? 96 : lcdWidth;
  int pixelsHeight = (pixelsWidth * 64 + 48) / 96;
  return (WalkerShellLayout){
      x, y, size, x + left, y + top, lcdWidth, lcdHeight,
      x + left + (lcdWidth - pixelsWidth) / 2,
      y + top + (lcdHeight - pixelsHeight) / 2, pixelsWidth, pixelsHeight};
}

static inline WalkerShellLayout walkerShellLayout(int width, int height) {
  int size = height - 8;
  return walkerShellAt((width - size) / 2, (height - size) / 2, size);
}

static inline int walkerShellLcdInset(const WalkerShellLayout *layout, int row) {
  int last = layout->lcdHeight - 1;
  int distance = row < last - row ? row : last - row;
  return distance == 0 ? 2 : distance == 1 ? 1 : 0;
}

static inline int walkerShellButton(const WalkerShellLayout *layout, int x, int y) {
  for (int button = 0; button < 3; button++) {
    int dx = x - (layout->x + layout->size / 2 +
                  (button - 1) * layout->size * 225 / 1000);
    int dy = y - (layout->y + layout->size / 2 +
                  layout->size * (button == 1 ? 305 : 275) / 1000);
    int radius = layout->size * (button == 1 ? 80 : 65) / 1000;
    if (radius < 13)
      radius = 13;
    if (dx * dx + dy * dy <= radius * radius)
      return button;
  }
  return -1;
}

static inline const uint32_t *walkerShellPalette(int contrast) {
  static const uint32_t palettes[3][4] = {
      {0xb4b7aa, 0x9a9f91, 0x787e6f, 0x535b4b},
      {0xb4b7aa, 0x8d9383, 0x555e4c, 0x232b1d},
      {0xb4b7aa, 0x7f8675, 0x3e4835, 0x10180d}};
  return palettes[contrast >= 0 && contrast < 3 ? contrast : 1];
}

#endif
