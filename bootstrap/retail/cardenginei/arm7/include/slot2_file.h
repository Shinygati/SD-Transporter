#ifndef SLOT2_FILE_H
#define SLOT2_FILE_H
#include <nds/ndstypes.h>
/* Virtual GBA Slot-2 save write-back (see slot2_file.c). */
bool slot2EmuDue(bool force);   /* something to write, and (unless forced) the game is idle */
void slot2EmuWrite(void);       /* caller holds the save mutex with SD selected */
#endif
