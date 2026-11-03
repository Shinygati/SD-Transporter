/* Virtual GBA Slot-2 save write-back. GPL-3.0-or-later.
 * The ARM9 emulator (retail/slot2emu) keeps the GBA save in a RAM mirror and
 * marks changed 4 KiB sectors; these are written to the selected .sav file
 * once the game has stopped writing for a moment, and before a soft reset.
 * Only the emulated chip's bytes are written, so an RTC trailer after them
 * (mGBA's 16 bytes) is preserved. */
#include <nds/ndstypes.h>
#include "my_fat.h"
#include "slot2_file.h"
#include "slot2_emu.h"

extern u32 slot2SaveCluster, slot2SaveSize, slot2EmuControl;

#define SLOT2EMU_IDLE_FRAMES 30 /* ~0.5 s without new writes */

static aFile slot2Save;
static bool slot2Opened = false;
static u32 slot2SeenSeq = 0, slot2IdleFrames = 0;

static Slot2EmuControl *control(void) {
	Slot2EmuControl *c = (Slot2EmuControl *)slot2EmuControl;
	if (!slot2SaveCluster || !c || c->magic != SLOT2EMU_MAGIC || c->version != SLOT2EMU_VERSION) return 0;
	return c;
}

bool slot2EmuDue(bool force) {
	Slot2EmuControl *c = control();
	if (!c || c->writeSeq == c->flushedSeq) return false;
	if (force) return true;
	if (c->writeSeq != slot2SeenSeq) {
		slot2SeenSeq = c->writeSeq;
		slot2IdleFrames = 0;
		return false;
	}
	return ++slot2IdleFrames >= SLOT2EMU_IDLE_FRAMES;
}

void slot2EmuWrite(void) {
	Slot2EmuControl *c = control();
	if (!c) return;
	if (!slot2Opened) {
		getFileFromCluster(&slot2Save, slot2SaveCluster, false);
		slot2Opened = true;
	}
	const u32 seq = c->writeSeq;
	const u32 sectors = c->flashSize / SLOT2EMU_SECTOR;
	for (u32 i = 0; i < sectors && i < SLOT2EMU_SECTORS; i++) {
		if (!c->dirty[i]) continue;
		/* Clear first: if the ARM9 changes the sector again meanwhile it is
		 * marked again and rewritten on the next pass. */
		c->dirty[i] = 0;
		fileWrite((char *)(c->mirror + i * SLOT2EMU_SECTOR), &slot2Save, i * SLOT2EMU_SECTOR, SLOT2EMU_SECTOR);
	}
	c->flushedSeq = seq;
	c->flushes++;
	slot2IdleFrames = 0;
}
