#ifndef WALKER_FILE_H
#define WALKER_FILE_H
#include <nds/ndstypes.h>
/* Virtual Pokéwalker EEPROM write-back (see walker_file.c). */
bool walkerDue(bool force);   /* something to write, and (unless forced) the link is idle */
void walkerWrite(void);       /* caller holds the save mutex with SD selected */
#endif
