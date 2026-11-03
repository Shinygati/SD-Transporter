#pragma GCC optimize("Os")
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
#include "cardengine_header_arm9.h"
#include "module_params.h"
#include "find.h"
#include "patch.h"
#include "debug_file.h"
#include <string.h>
#include "walker_emu.h"
#include "walker_ds_context.h"
#include "walker_ds_payload.h"

WALKER_ASSERT(ds_blob_size, WALKER_DS_RUNTIME_SIZE <= WALKER_DS_BLOB_MAX);
WALKER_ASSERT(ds_protocol_size, WALKER_BLOB_OFF+WALKER_PROTOCOL_SIZE <= WALKER_DS_BLOB_OFF);
WALKER_ASSERT(ds_file_size, sizeof(aFile) <= sizeof(((WalkerDsContext *)0)->file));
u32 walkerDsBase;
#define walkerBase walkerDsBase
extern u32 walkerCluster, walkerPayloadOffset, walkerPayloadSize, ndsBootstrapCluster;
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

/* The legacy fileRead overcounts its return value for a multi-sector read
 * with a partial final sector. Keep each DS payload read within one sector
 * so exact-length validation works without changing shared DSi/3DS storage. */
static bool walkerReadPayload(char *dst, aFile *file, u32 offset, u32 length) {
    while (length) {
        u32 part = 512 - (offset & 511);
        if (part > length) part = length;
        if (fileRead(dst, file, offset, part) != part) {
            dbg_printf("Walker DS: payload read failed\n");
            return false;
        }
        dst += part;
        offset += part;
        length -= part;
    }
    return true;
}

static void walkerInstallDs(cardengineArm9 *ce9, const tNDSHeader *ndsHeader) {
	if (!walkerBase || !walkerCluster) return;
	WalkerControl *c = (WalkerControl *)(walkerBase + WALKER_CONTROL_OFF);
	toncset(c, 0, sizeof(WalkerControl));

	u32 *code = (u32 *)ndsHeader->arm9destination;
	const u32 size = iUncompressedSize > ndsHeader->arm9binarySize ? iUncompressedSize : ndsHeader->arm9binarySize;
	u32 *end = code + size / 4;
	u32 *readRange = findReadRange(code, size / 4);
	if (!readRange) { dbg_printf("Walker DS: IR receive signature missing or ambiguous\n"); return; }
	u32 *writeRange = findNext(readRange, end, ARM_PUSH_WRITERANGE, 0x80);
	if (!writeRange || (u32)((u8 *)writeRange - (u8 *)readRange) < WALKER_IR_STUB_SIZE) { dbg_printf("Walker DS: IR send signature or stub space missing\n"); return; }
	u32 *getStatus = findNext(writeRange, end, ARM_PUSH_GETSTATUS, 0x80);
	if (!getStatus) { dbg_printf("Walker DS: IR status signature missing\n"); return; }
	/* CARD_SpiWaitInit, a little before the receive routine:
	 * stmdb sp!, {r3, lr} / ldr r1, =state / mov r0, #0x32 ... */
	u32 *init = 0;
	for (u32 k = 4; k <= 0x80 && readRange - k >= code; k++) {
		u32 *f = readRange - k;
		if (f[0] == 0xE92D4008 && (f[1] & 0xFFFFF000) == 0xE59F1000 && f[2] == 0xE3A00032) { init = f; break; }
	}
	if (!init) { dbg_printf("Walker DS: IR init signature missing\n"); return; }

	aFile walker;
	getFileFromCluster(&walker, walkerCluster);
	if (fileRead((char *)(walkerBase + WALKER_EEPROM_OFF), &walker, 0, WALKER_EEPROM_SIZE) != WALKER_EEPROM_SIZE) { dbg_printf("Walker DS: EEPROM read failed\n"); return; }

    aFile bootstrap;
    getFileFromCluster(&bootstrap, ndsBootstrapCluster);
    if (walkerPayloadSize != WALKER_DS_PAYLOAD_SIZE || !walkerPayloadOffset) return;
    if (!walkerReadPayload((char *)walkerBase + WALKER_BLOB_OFF, &bootstrap, walkerPayloadOffset, WALKER_PROTOCOL_SIZE)) return;
    if (!walkerReadPayload((char *)walkerBase + WALKER_DS_BLOB_OFF, &bootstrap, walkerPayloadOffset + WALKER_PROTOCOL_SIZE + WALKER_IR_STUB_SIZE, WALKER_DS_RUNTIME_SIZE)) return;
    if (!walkerReadPayload((char *)c->scratch, &bootstrap, walkerPayloadOffset + WALKER_PROTOCOL_SIZE, WALKER_IR_STUB_SIZE)) return;
	c->version = WALKER_VERSION;
	c->cluster = walkerCluster;
	c->eeprom = walkerBase + WALKER_EEPROM_OFF;
    WalkerDsContext *ds = (WalkerDsContext *)((u8 *)c + WALKER_DS_CONTEXT_OFF);
    toncset(ds, 0, sizeof(*ds));
    ds->protocolEntry = walkerBase + WALKER_BLOB_OFF;
    tonccpy(&ds->initialize, (const void *)ce9->walkerServices, 6 * sizeof(u32));
	c->magic = WALKER_MAGIC;

	/* Receive: the stub itself, over the original routine. */
	tonccpy(readRange, c->scratch, WALKER_IR_STUB_SIZE);
	*(u32 *)((u8 *)readRange + WALKER_STUB_CONTROL_OFF) = (u32)c;
	*(u32 *)((u8 *)readRange + WALKER_STUB_ENTRY_OFF) = walkerBase + WALKER_DS_BLOB_OFF;
	/* Send: ldr pc, [pc, #-4] / .word stub+write */
	writeRange[0] = 0xE51FF004;
	writeRange[1] = (u32)readRange + WALKER_STUB_WRITE_OFF;
	/* Session start: ldr pc, [pc, #-4] / .word stub+init */
	init[0] = 0xE51FF004;
	init[1] = (u32)readRange + WALKER_STUB_INIT_OFF;
	/* Chip check: mov r0, #0xAA / bx lr */
	getStatus[0] = 0xE3A000AA;
	getStatus[1] = 0xE12FFF1E;

	ce9->walkerControl = (u32)c;
    ce9->walkerFlush = (u32)readRange + WALKER_STUB_INIT_OFF;
}

/* Reserve only ordinary 4 MiB main RAM, below both the game's detected heap
 * ceiling and the loader's cardengine/FAT buffers. No game-address tables. */
void walkerBootDs(cardengineArm9 *ce9, const tNDSHeader *header, const module_params_t *module, u32 dldiOffset) {
    extern bool extendedMemory;
    extern u32 fatTableAddr;
    walkerBase = 0;
    ce9->walkerControl = 0;
    ce9->walkerFlush = 0;
    dbg_printf("Walker DS cluster/services: "); dbg_hexa(walkerCluster); dbg_hexa(ce9->walkerServices); dbg_printf("\n");
    if (!walkerCluster || !ce9->walkerServices || extendedMemory || isSdk5(module) || header->unitCode) return;
    if (memcmp(getRomTid(header), "IPK", 3) && memcmp(getRomTid(header), "IPG", 3)) return;
    u32 codeStart = (u32)header->arm9destination;
    u32 codeEnd = codeStart + (iUncompressedSize > header->arm9binarySize ? iUncompressedSize : header->arm9binarySize);
    /* patchHiHeapPointer has already replaced the 0x023E0000 literal that
     * findHeapPointer2Offset searches for. Reuse its detected location;
     * rescanning the modified binary silently disables the walker. */
    u32 *heap = patchOffsetCache.heapPointer2Offset;
    if (!heap) heap = findHeapPointer2Offset(module, header);
    if ((u32)heap < codeStart || (u32)heap + 4 > codeEnd || ((u32)heap & 3)) { dbg_printf("Walker DS: SDK heap location unavailable\n"); return; }
    u32 ceiling = *heap;
    dbg_printf("Walker DS heap/value: "); dbg_hexa((u32)heap); dbg_hexa(ceiling); dbg_printf("\n");
    if (ceiling >= 0x02700000 && ceiling < 0x02800000) ceiling -= 0x00400000;
    if (ceiling <= 0x02000000 || ceiling > 0x02400000) return;
    u32 occupiedEnd = (u32)module->static_bss_end;
    if (occupiedEnd < codeEnd) occupiedEnd = codeEnd;
    if (occupiedEnd < 0x02000000 || occupiedEnd >= ceiling) return;
    u32 limits[] = {(u32)ce9, fatTableAddr, dldiOffset, ce9->musicBuffer, 0x023DE000u};
    for (unsigned i = 0; i < sizeof(limits)/sizeof(limits[0]); i++) {
        u32 value = limits[i];
        if (value >= 0x02700000 && value < 0x02800000) value -= 0x00400000;
        if (value >= occupiedEnd && value < ceiling) ceiling = value;
    }
    ceiling &= ~0xfffu;
    dbg_printf("Walker DS ceiling/BSS end: "); dbg_hexa(ceiling); dbg_hexa(occupiedEnd); dbg_printf("\n");
    if (ceiling < occupiedEnd + WALKER_RESERVE + 0x4000 || ceiling - WALKER_RESERVE < 0x023C0000) { dbg_printf("Walker DS: insufficient reserved memory\n"); return; }
    u32 before = *heap;
    walkerBase = ceiling - WALKER_RESERVE;
    *heap = walkerBase;
    walkerInstallDs(ce9, header);
    if (!ce9->walkerControl) {
        *heap = before;
        walkerBase = 0;
        dbg_printf("Walker DS: hook not installed\n");
    } else {
        dbg_printf("Walker DS: hook installed; SELECT connects during search\n");
    }
}
