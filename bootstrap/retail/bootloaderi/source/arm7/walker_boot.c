/* Virtual Pokéwalker: boot-time setup. GPL-3.0-or-later.
 *
 * hook_arm9.c reserves walkerBase at the end of the ROM cache when a walker
 * file was selected for this save (walkerCluster). Here the walker file is
 * loaded into the EEPROM image, the control block and blob are set up, and
 * HeartGold/SoulSilver's three IR routines are replaced:
 *   CARD_SpiWaitReadRange  -> the IR stub (receive)
 *   CARD_SpiWaitWriteRange -> jump into the IR stub (send)
 *   CARD_SpiWaitGetStatus  -> "IR chip present" (0xAA)
 *   CARD_SpiWaitInit       -> start a new session
 * They are found by their code, not by address, so every language and
 * revision works; if anything doesn't match, the game is left untouched.
 * The ARM7 cardengine is told where to write the EEPROM back.
 */
#include <nds/ndstypes.h>
#include "my_fat.h"
#include "tonccpy.h"
#include "nds_header.h"
#include "cardengine_header_arm7.h"
#include "walker_emu.h"
#include "walker_payload.h"

extern u32 walkerBase, walkerCluster;
extern u32 iUncompressedSize;

#define ARM_PUSH_READRANGE  0xE92D47F0 /* stmdb sp!, {r4-r10, lr} */
#define ARM_PUSH_WRITERANGE 0xE92D43F8 /* stmdb sp!, {r3-r9, lr} */
#define ARM_PUSH_GETSTATUS  0xE92D4FF8 /* stmdb sp!, {r3-r11, lr} */

/* The receive loop's descrambling: ldrb r0,[r6,r5] / eor r0,r0,#0xAA /
 * strb r0,[r6,r5] / add r5,r5,#1. */
static u32 *findReadRange(u32 *code, u32 words) {
	u32 *found = 0;
	for (u32 i = 0; i + 4 <= words; i++) {
		if (code[i] != 0xE7D60005 || code[i + 1] != 0xE22000AA ||
		    code[i + 2] != 0xE7C60005 || code[i + 3] != 0xE2855001) continue;
		u32 *start = 0;
		for (u32 k = 1; k <= 0x60 && k <= i; k++) {
			if (code[i - k] == ARM_PUSH_READRANGE) { start = &code[i - k]; break; }
		}
		if (!start || found) return 0; /* missing or ambiguous */
		found = start;
	}
	return found;
}

static u32 *findNext(u32 *from, u32 *end, u32 push, u32 maxWords) {
	for (u32 i = 1; i < maxWords && from + i < end; i++) {
		if (from[i] == push) return from + i;
	}
	return 0;
}

void walkerBoot(cardengineArm7 *ce7, const tNDSHeader *ndsHeader) {
	if (!walkerBase || !walkerCluster) return;
	WalkerControl *c = (WalkerControl *)(walkerBase + WALKER_CONTROL_OFF);
	toncset(c, 0, sizeof(WalkerControl));

	u32 *code = (u32 *)ndsHeader->arm9destination;
	const u32 size = iUncompressedSize > ndsHeader->arm9binarySize ? iUncompressedSize : ndsHeader->arm9binarySize;
	u32 *end = code + size / 4;
	u32 *readRange = findReadRange(code, size / 4);
	if (!readRange) return;
	u32 *writeRange = findNext(readRange, end, ARM_PUSH_WRITERANGE, 0x80);
	if (!writeRange || (u32)((u8 *)writeRange - (u8 *)readRange) < sizeof(walkerStub)) return;
	u32 *getStatus = findNext(writeRange, end, ARM_PUSH_GETSTATUS, 0x80);
	if (!getStatus) return;
	/* CARD_SpiWaitInit, a little before the receive routine:
	 * stmdb sp!, {r3, lr} / ldr r1, =state / mov r0, #0x32 ... */
	u32 *init = 0;
	for (u32 k = 4; k <= 0x80 && readRange - k >= code; k++) {
		u32 *f = readRange - k;
		if (f[0] == 0xE92D4008 && (f[1] & 0xFFFFF000) == 0xE59F1000 && f[2] == 0xE3A00032) { init = f; break; }
	}
	if (!init) return;

	aFile walker;
	getFileFromCluster(&walker, walkerCluster, false);
	if (fileRead((char *)(walkerBase + WALKER_EEPROM_OFF), &walker, 0, WALKER_EEPROM_SIZE) != WALKER_EEPROM_SIZE) return;

	tonccpy((u8 *)walkerBase + WALKER_BLOB_OFF, walkerBlob, sizeof(walkerBlob));
	c->version = WALKER_VERSION;
	c->cluster = walkerCluster;
	c->eeprom = walkerBase + WALKER_EEPROM_OFF;
	c->magic = WALKER_MAGIC;

	/* Receive: the stub itself, over the original routine. */
	tonccpy(readRange, walkerStub, sizeof(walkerStub));
	*(u32 *)((u8 *)readRange + WALKER_STUB_CONTROL_OFF) = (u32)c;
	*(u32 *)((u8 *)readRange + WALKER_STUB_ENTRY_OFF) = walkerBase + WALKER_BLOB_OFF;
	/* Send: ldr pc, [pc, #-4] / .word stub+write */
	writeRange[0] = 0xE51FF004;
	writeRange[1] = (u32)readRange + WALKER_STUB_WRITE_OFF;
	/* Session start: ldr pc, [pc, #-4] / .word stub+init */
	init[0] = 0xE51FF004;
	init[1] = (u32)readRange + WALKER_STUB_INIT_OFF;
	/* Chip check: mov r0, #0xAA / bx lr */
	getStatus[0] = 0xE3A000AA;
	getStatus[1] = 0xE12FFF1E;

	ce7->walkerControl = (u32)c;
}
