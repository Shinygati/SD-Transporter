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
extern bool gen5CheckedIO(void *buffer,aFile *file,u32 offset,u32 length,bool writing);

#define WALKER_IDLE_FRAMES 30 /* ~0.5 s without new writes */

static aFile walkerFile;
static bool walkerOpened = false;
static u32 walkerSeenSeq = 0, walkerIdleFrames = 0;

/* HGSS is waiting for the withheld CONNECT_COMPLETE_ACK (0x68). */
static bool completionPending(const WalkerControl *c) {
	return !c->state && c->respLen >= 8 && c->resp[0] == 0x68;
}

static WalkerControl *control(void) {
	WalkerControl *c = (WalkerControl *)walkerControl;
	if (!c || c->magic != WALKER_MAGIC || c->version != WALKER_VERSION || !c->cluster) return 0;
	return c;
}

bool walkerDue(bool force) {
	WalkerControl *c = control();
	if (!c || c->writeSeq == c->flushedSeq) return false;
	if (force) return true;
    /* A completion response is waiting on durable storage; start draining on
     * this frame instead of waiting for the half-second coalescing delay. */
    if (completionPending(c)) return true;
	if (c->writeSeq != walkerSeenSeq) {
		walkerSeenSeq = c->writeSeq;
		walkerIdleFrames = 0;
		return false;
	}
	return ++walkerIdleFrames >= WALKER_IDLE_FRAMES;
}

void walkerWrite(bool force) {
	WalkerControl *c = control();
	if (!c) return;
	if (!walkerOpened) {
		getFileFromCluster(&walkerFile, c->cluster, false);
		walkerOpened = true;
	}
	const u32 seq = c->writeSeq;
	/* Pacing one sector per VBlank only helps mid-transfer. Once the game is
	 * waiting on the final acknowledgement every frame of delay widens the
	 * window in which HGSS can give up on a return, so drain it all now. */
	const bool drain = force || completionPending(c);
	for (u32 i = 0; i < WALKER_SECTORS; i++) {
		if (!c->dirty[i]) continue;
		/* Clear first: a sector changed again meanwhile is marked again and
		 * rewritten on the next pass. */
		c->dirty[i] = 0;
        /* The shared checked SD backend verifies physical read-back and driver
         * errors. Keep the sector dirty and retry instead of reporting it saved. */
        for(u32 offset=0;offset<WALKER_SECTOR;offset+=512) {
            const u32 at=i*WALKER_SECTOR+offset;
            if(!gen5CheckedIO((void *)(c->eeprom+at),&walkerFile,at,512,true)) {
                c->dirty[i]=1;
                walkerIdleFrames=0;
                return;
            }
        }
        /* A full dirty image can cost 128 checked SD operations. Do not put
         * that entire burst into one VBlank during the transfer animation.
         * Leave the rest due on the next frame, but forced reset/menu flushes
         * still drain everything before their caller proceeds. */
        if(!drain) {
            for(u32 pending=0;pending<WALKER_SECTORS;pending++) {
                if(c->dirty[pending]) { walkerIdleFrames=WALKER_IDLE_FRAMES; return; }
            }
        }
	}
	c->flushedSeq = seq;
	c->flushes++;
	walkerIdleFrames = 0;
}
#else
bool walkerDue(bool force) { (void)force; return false; }
void walkerWrite(bool force) { (void)force; }
#endif
