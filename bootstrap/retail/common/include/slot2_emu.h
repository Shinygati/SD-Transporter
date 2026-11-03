/* Virtual GBA Slot-2 cartridge shared between the bootloader, the ARM9
 * abort-handler blob and the ARM7 save flusher. GPL-3.0-or-later.
 *
 * The bootloader reserves SLOT2EMU_RESERVE bytes at the end of the ROM cache:
 *   +0x00000  save mirror (SRAM 32 KiB, Flash 64 KiB or Flash 128 KiB)
 *   +0x20000  Slot2EmuControl (one 4 KiB page)
 *   +0x21000  ARM9 emulator blob (position independent)
 *
 * The ARM9 blob only touches this area with the MPU disabled (uncached), and
 * the ARM7 reads/writes main RAM directly, so no cache maintenance is needed
 * between them. */
#ifndef SLOT2_EMU_H
#define SLOT2_EMU_H

#define SLOT2EMU_RESERVE      0x28000u
#define SLOT2EMU_MIRROR_OFF   0x00000u
#define SLOT2EMU_CONTROL_OFF  0x20000u
#define SLOT2EMU_BLOB_OFF     0x21000u
#define SLOT2EMU_BLOB_MAX     0x07000u

#define SLOT2EMU_MAGIC        0x4D453253u /* 'S2EM' */
#define SLOT2EMU_VERSION      1u
#define SLOT2EMU_HEADER_SIZE  0x200u
#define SLOT2EMU_SECTOR       0x1000u
#define SLOT2EMU_SECTORS      32u
#define SLOT2EMU_HIST         64u
#define SLOT2EMU_BLOB_BUILD   0x20261102u

/* Backup chip presented to the game, picked from the save size. */
#define SLOT2EMU_TYPE_SRAM    1u  /* 32 KiB battery SRAM */
#define SLOT2EMU_TYPE_FLASH64 2u  /* Panasonic MN63F805MNP, ID 0x1B32 */
#define SLOT2EMU_TYPE_FLASH128 3u /* Macronix MX29L010, ID 0x09C2 */

/* NitroSDK boot-time cartridge info (HW_CTRDG_MODULE_INFO_BUF) and the two
 * flags CTRDGi_InitModuleInfo checks before it DMA-reads the GBA header. */
#define SLOT2EMU_MODULE_INFO  0x027FFC30u
#define SLOT2EMU_INFO_DONE    0x027FFF9Au
#define SLOT2EMU_INFO_EXISTED 0x027FFF9Bu

/* MPU region replacing NitroSDK's slot region 3: 0x0C000000, 64 MiB, enabled.
 * Keeps the DSi extra RAM (nds-bootstrap's ROM cache) mapped while the GBA
 * slot itself (0x08000000-0x0BFFFFFF) is left unmapped and so aborts. */
#define SLOT2EMU_EXTRAM_REGION 0x0C000033u

/* Offsets used by the blob's assembly entry; checked by static asserts. */
#define SLOT2EMU_CTL_PREVVECTOR 0x3C
#define SLOT2EMU_CTL_BIOSSTACK  0x40
#define SLOT2EMU_CTL_CP15CTRL   0x44

#ifndef __ASSEMBLER__
typedef struct Slot2EmuControl {
	/* Written once by the bootloader. */
	unsigned int magic;
	unsigned int version;
	unsigned int type;          /* SLOT2EMU_TYPE_* */
	unsigned int flashSize;     /* bytes emulated: 0x8000, 0x10000 or 0x20000 */
	unsigned int saveSize;      /* size of the .sav file on SD (may hold an RTC trailer) */
	unsigned int mirror;        /* address of the save mirror */
	unsigned int gameCode;      /* GBA header 0xAC */
	unsigned short makerCode;   /* GBA header 0xB0 */
	unsigned short moduleId;    /* GBA header 0xBE */
	unsigned char exLsiId[3];   /* GBA header 0xB5..0xB7 */
	unsigned char flashMaker;
	unsigned char flashDevice;
	unsigned char reserved0[3];
	unsigned short moduleImage; /* ROM halfword at 0x1FFFE */
	unsigned short reserved1;
	unsigned int bulkReads;     /* diagnostics: whole-loop fast paths taken */
	unsigned int bulkVerifies;
	unsigned int bulkPrograms;
	unsigned int blobBuild;     /* SLOT2EMU_BLOB_BUILD once the blob has installed */

	/* ARM9 install state (written with the MPU disabled). */
	unsigned int prevVector;    /* debug-vector handler we chain to */
	unsigned int biosStack;     /* BIOS exception save area (vector - 0xC) */
	unsigned int cp15ctrl;      /* CP15 control register while the game runs */
	unsigned int installs;

	/* ARM9 flash chip state (abort handler). */
	unsigned int flashState;    /* 0 ready, 1 got AA, 2 got 55 */
	unsigned int flashMode;     /* bit0 ID, bit1 erase armed, bit2 program armed, bit3 bank armed */
	unsigned int flashBank;
	unsigned int writeSeq;      /* incremented after every change to the mirror */
	unsigned int aborts;        /* emulated accesses, for debugging */
	unsigned int unhandled;     /* faults passed on to the previous handler */
	unsigned int bulk;          /* byte loops run in one go (see bulkLoop) */
	unsigned int reserved3;     /* diagnostics: optional Pal Park bypass applied */

	/* ARM7 flush state. */
	unsigned int flushedSeq;
	unsigned int flushes;
	unsigned int reserved4[2];

	unsigned char dirty[SLOT2EMU_SECTORS]; /* ARM9 sets, ARM7 clears */

	unsigned char pad[0x100 - 0x9C];
	unsigned char header[SLOT2EMU_HEADER_SIZE]; /* ROM 0x000..0x1FF */

	/* Diagnostics, read back from an in-game-menu RAM dump: where the
	 * game's slot accesses come from (faulting PC, count). */
	unsigned int histPc[SLOT2EMU_HIST];
	unsigned int histCount[SLOT2EMU_HIST];
	unsigned int histOther;
} Slot2EmuControl;

#define SLOT2EMU_ASSERT(name, cond) typedef char slot2emu_assert_##name[(cond) ? 1 : -1]
SLOT2EMU_ASSERT(prev, __builtin_offsetof(Slot2EmuControl, prevVector) == SLOT2EMU_CTL_PREVVECTOR);
SLOT2EMU_ASSERT(bios, __builtin_offsetof(Slot2EmuControl, biosStack) == SLOT2EMU_CTL_BIOSSTACK);
SLOT2EMU_ASSERT(ctrl, __builtin_offsetof(Slot2EmuControl, cp15ctrl) == SLOT2EMU_CTL_CP15CTRL);
SLOT2EMU_ASSERT(header, __builtin_offsetof(Slot2EmuControl, header) == 0x100);
SLOT2EMU_ASSERT(size, sizeof(Slot2EmuControl) <= SLOT2EMU_BLOB_OFF - SLOT2EMU_CONTROL_OFF);
#endif

#endif
