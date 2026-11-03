/* Virtual GBA Slot-2 cartridge: boot-time setup. GPL-3.0-or-later.
 *
 * hook_arm9.c reserves slot2EmuBase at the end of the ROM cache. Here the
 * bootloader fills it: the GBA save goes into the mirror, the GBA ROM header
 * into the control block, and the ARM9 blob after it. The ARM7 cardengine is
 * told where to write the save back, and the SDK's ARM7-side cartridge check
 * is told the cartridge is present (the ARM7 has no MPU, so it cannot be
 * trapped like the ARM9 and would otherwise see an empty slot).
 */
/* Boot-only setup: optimize for size to preserve the DSi loader stack area. */
#pragma GCC optimize("Os")
#include <nds/ndstypes.h>
#include "my_fat.h"
#include "tonccpy.h"
#include "nds_header.h"
#include "cardengine_header_arm7.h"
#include "cardengine_header_arm9.h"
#include "slot2_emu.h"
#include "slot2_emu_payload.h"
#include "palpark_bypass.h"

extern u32 slot2EmuBase;
extern u32 slot2SaveCluster, slot2SaveSize, slot2GameCode, slot2RomCluster;
extern u32 slot2Reserved0;

static Slot2EmuControl *slot2Control(void) {
	return (Slot2EmuControl *)(slot2EmuBase + SLOT2EMU_CONTROL_OFF);
}

/* A minimal header for saves selected without a ROM: the game code from the
 * launcher, maker "01", the fixed byte and the header checksum. The logo is
 * the one in the DS cartridge header, which uses the same compressed logo. */
static void slot2SynthesizeHeader(u8 *h, const tNDSHeader *ndsHeader) {
	toncset(h, 0, SLOT2EMU_HEADER_SIZE);
	h[0] = 0x2E; h[3] = 0xEA; /* b 0x080000C0 */
	tonccpy(h + 4, (const u8 *)ndsHeader + 0xC0, 0x9C);
	tonccpy(h + 0xA0, "GBA CARTRIDG", 12);
	*(u32 *)(h + 0xAC) = slot2GameCode;
	h[0xB0] = '0'; h[0xB1] = '1';
	h[0xB2] = 0x96;
	u8 sum = 0;
	for (int i = 0xA0; i <= 0xBC; i++) sum += h[i];
	h[0xBD] = (u8)(-(sum + 0x19));
}

/* NitroSDK's ARM7 CTRDG_IsExisting() compares the boot-time record with the
 * header it reads from the real slot. Found by its literal pool: the module-ID
 * image address 0x0801FFFE next to the record at 0x027FFC30. It is made to
 * return TRUE, which also keeps the pull-out poll from reporting a removal. */
static int slot2PatchArm7(const tNDSHeader *ndsHeader) {
	u32 *code = (u32 *)ndsHeader->arm7destination;
	const u32 words = ndsHeader->arm7binarySize / 4;
	int patched = 0;
	for (u32 i = 0; i < words; i++) {
		if (code[i] != 0x0801FFFE) continue;
		bool record = false;
		for (u32 j = (i >= 6 ? i - 6 : 0); j < i + 6 && j < words; j++) {
			if (code[j] == SLOT2EMU_MODULE_INFO) record = true;
		}
		if (!record) continue;
		for (u32 k = 1; k <= 0x80 && k <= i; k++) {
			if ((code[i - k] & 0xFFFF4000) == 0xE92D4000) { /* stmdb sp!, {..., lr} */
				code[i - k] = 0xE3A00001; /* mov r0, #1 */
				code[i - k + 1] = 0xE12FFF1E; /* bx lr */
				patched++;
				break;
			}
		}
	}
	return patched;
}

static void slot2Disable(cardengineArm9 *ce9) {
	slot2EmuBase = 0;
	ce9->slot2EmuEntry = 0;
}

void slot2EmuBoot(cardengineArm7 *ce7, cardengineArm9 *ce9, const tNDSHeader *ndsHeader) {
	ce7->slot2SaveCluster = 0;
	ce7->slot2SaveSize = 0;
	ce7->slot2EmuControl = 0;
	if (!slot2EmuBase) {
		ce9->slot2EmuEntry = 0;
		return;
	}

	Slot2EmuControl *c = slot2Control();
	toncset(c, 0, sizeof(Slot2EmuControl));

	u32 type, flashSize;
	switch (slot2SaveSize) {
		case 0x8000:
			type = SLOT2EMU_TYPE_SRAM; flashSize = 0x8000; break;
		case 0x10000:
			type = SLOT2EMU_TYPE_FLASH64; flashSize = 0x10000; break;
		case 0x20000:
		case 0x20010: /* mGBA RTC trailer, preserved */
			type = SLOT2EMU_TYPE_FLASH128; flashSize = 0x20000; break;
		default:
			slot2Disable(ce9); return;
	}

	aFile save;
	getFileFromCluster(&save, slot2SaveCluster, false);
	if (fileRead((char *)slot2EmuBase, &save, 0, flashSize) != flashSize) {
		slot2Disable(ce9);
		return;
	}

	if (slot2RomCluster) {
		aFile rom;
		getFileFromCluster(&rom, slot2RomCluster, false);
		if (fileRead((char *)c->header, &rom, 0, SLOT2EMU_HEADER_SIZE) != SLOT2EMU_HEADER_SIZE) {
			slot2Disable(ce9);
			return;
		}
		fileRead((char *)&c->moduleImage, &rom, 0x1FFFE, 2);
	} else {
		slot2SynthesizeHeader(c->header, ndsHeader);
	}
	if (c->header[0xB2] != 0x96) {
		slot2Disable(ce9);
		return;
	}

	c->version = SLOT2EMU_VERSION;
	c->type = type;
	c->flashSize = flashSize;
	c->saveSize = slot2SaveSize;
	c->mirror = slot2EmuBase + SLOT2EMU_MIRROR_OFF;
	c->gameCode = *(u32 *)(c->header + 0xAC);
	c->makerCode = c->header[0xB0] | (c->header[0xB1] << 8);
	c->moduleId = c->header[0xBE] | (c->header[0xBF] << 8);
	c->exLsiId[0] = c->header[0xB5];
	c->exLsiId[1] = c->header[0xB6];
	c->exLsiId[2] = c->header[0xB7];
	if (type == SLOT2EMU_TYPE_FLASH128) {
		c->flashMaker = 0xC2; c->flashDevice = 0x09; /* Macronix MX29L010 */
	} else if (type == SLOT2EMU_TYPE_FLASH64) {
		c->flashMaker = 0x32; c->flashDevice = 0x1B; /* Panasonic MN63F805MNP */
	}

	tonccpy((u8 *)slot2EmuBase + SLOT2EMU_BLOB_OFF, slot2EmuBlob, sizeof(slot2EmuBlob));
	c->magic = SLOT2EMU_MAGIC;
	if (slot2Reserved0 & 1) {
		/* Search the whole decompressed ARM9, not just its compressed size. */
		extern u32 iUncompressedSize;
		const u32 arm9Size = iUncompressedSize > ndsHeader->arm9binarySize ? iUncompressedSize : ndsHeader->arm9binarySize;
		c->reserved3 = palParkPatchArm9((void *)ndsHeader->arm9destination, arm9Size);
	}

	slot2PatchArm7(ndsHeader);

	ce7->slot2SaveCluster = slot2SaveCluster;
	ce7->slot2SaveSize = slot2SaveSize;
	ce7->slot2EmuControl = (u32)c;
}

/* Just before the game starts: write the SDK's boot-time cartridge record and
 * mark it as already initialised, so CTRDG_Init() keeps it instead of reading
 * the (empty) slot by DMA, which the ARM9 trap cannot see. */
void slot2EmuPrefill(void) {
	if (!slot2EmuBase) return;
	const Slot2EmuControl *c = slot2Control();
	volatile u8 *info = (volatile u8 *)SLOT2EMU_MODULE_INFO;
	info[0] = (u8)c->moduleId;
	info[1] = (u8)(c->moduleId >> 8);
	info[2] = c->exLsiId[0];
	info[3] = c->exLsiId[1];
	info[4] = c->exLsiId[2];
	info[5] = 1; /* isAgbCartridge */
	info[6] = (u8)c->makerCode;
	info[7] = (u8)(c->makerCode >> 8);
	*(volatile u32 *)(SLOT2EMU_MODULE_INFO + 8) = c->gameCode;
	*(volatile u8 *)SLOT2EMU_INFO_DONE = 1;
	*(volatile u8 *)SLOT2EMU_INFO_EXISTED = 0;
}
