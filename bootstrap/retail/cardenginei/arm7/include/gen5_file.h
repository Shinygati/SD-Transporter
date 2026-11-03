#ifndef GEN5_FILE_H
#define GEN5_FILE_H
#include <nds/ndstypes.h>
extern bool gen5Active;
bool gen5FileRequest(volatile u32 *request);
#endif
