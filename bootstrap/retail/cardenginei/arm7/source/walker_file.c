/* Virtual Pokéwalker EEPROM write-back. GPL-3.0-or-later.
 * The ARM9 walker (retail/pokewalker) keeps the walker's 64 KiB EEPROM in RAM
 * and marks changed 4 KiB sectors; they are written to the save's .walker
 * file once the game has stopped talking to the walker for a moment, and
 * before the in-game menu or a soft reset. */
#include <nds/ndstypes.h>
#include "my_fat.h"
#include "walker_file.h"
#include "walker_emu.h"

#ifndef TWLSDK
/* HGSS uses the NTR cardengine. The TWL SDK variants cannot install this
 * walker and have no room for an unused EEPROM file handle and its state. */
extern u32 walkerControl;

#define WALKER_IDLE_FRAMES 30 /* ~0.5 s without new writes */

static aFile walkerFile;
static bool walkerOpened = false;
static u32 walkerSeenSeq = 0, walkerIdleFrames = 0;

static WalkerControl *control(void) {
	WalkerControl *c = (WalkerControl *)walkerControl;
	if (!c || c->magic != WALKER_MAGIC || c->version != WALKER_VERSION || !c->cluster) return 0;
	return c;
}

bool walkerDue(bool force) {
	WalkerControl *c = control();
	if (!c || c->writeSeq == c->flushedSeq) return false;
	if (force) return true;
	if (c->writeSeq != walkerSeenSeq) {
		walkerSeenSeq = c->writeSeq;
		walkerIdleFrames = 0;
		return false;
	}
	return ++walkerIdleFrames >= WALKER_IDLE_FRAMES;
}

void walkerWrite(void) {
	WalkerControl *c = control();
	if (!c) return;
	if (!walkerOpened) {
		getFileFromCluster(&walkerFile, c->cluster, false);
		walkerOpened = true;
	}
	const u32 seq = c->writeSeq;
	for (u32 i = 0; i < WALKER_SECTORS; i++) {
		if (!c->dirty[i]) continue;
		/* Clear first: a sector changed again meanwhile is marked again and
		 * rewritten on the next pass. */
		c->dirty[i] = 0;
		fileWrite((char *)(c->eeprom + i * WALKER_SECTOR), &walkerFile, i * WALKER_SECTOR, WALKER_SECTOR);
	}
	c->flushedSeq = seq;
	c->flushes++;
	walkerIdleFrames = 0;
}
#else
bool walkerDue(bool force) { (void)force; return false; }
void walkerWrite(void) {}
#endif
