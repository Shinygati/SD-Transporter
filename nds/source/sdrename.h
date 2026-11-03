#ifndef SDRENAME_H
#define SDRENAME_H
#include <stdint.h>
/* POSIX-style result, plus the original 3DS FS Result on native failure. */
int sdRenameReplace(const char *source, const char *destination, uint32_t *nativeError);
#endif
