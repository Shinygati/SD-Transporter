/* Virtual GBA Slot-2 cartridge for NitroSDK games on DSi/3DS. GPL-3.0-or-later.
 *
 * There is no GBA slot in DSi mode, so the game's own accesses to the slot
 * (0x08000000-0x0BFFFFFF) are made to fault: the MPU region covering the slot
 * is disabled, every access then raises a data abort, and this handler decodes
 * the faulting load/store and performs it against a virtual cartridge:
 *
 *   ROM   0x08000000-0x09FFFFFF  the GBA ROM header (0x200 bytes), the
 *                                module-ID image at 0x1FFFE, open bus elsewhere
 *   SRAM  0x0A000000-0x0A00FFFF  battery SRAM or a flash chip state machine
 *                                (Macronix 128 KiB / Panasonic 64 KiB) backed
 *                                by a RAM mirror of the GBA .sav file
 *   rest  up to 0x0BFFFFFF       open bus (reads 0xFF, writes ignored)
 *
 * The game's normal cartridge library (CTRDG) does all detection, reading and
 * writing itself, so nothing here is specific to a game, revision or language.
 * The ARM7 cardengine writes changed 4 KiB sectors back to the .sav file.
 *
 * This file is linked as a position-independent blob: no global variables,
 * no constant tables and no jump tables; all state lives in Slot2EmuControl.
 * It runs only with the MPU disabled (from the BIOS exception path, or from
 * the cardengine's install trampoline).
 */
#include "slot2_emu.h"

typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef signed char s8;
typedef signed short s16;

#ifdef SLOT2EMU_HOST
/* Host-side tests supply instruction memory and the mirror. */
u32 hostFetch32(u32 address);
u16 hostFetch16(u32 address);
u8 *hostMirror(Slot2EmuControl *c);
#define FETCH32(a) hostFetch32(a)
#define FETCH16(a) hostFetch16(a)
#define MIRROR(c)  hostMirror(c)
#else
#define FETCH32(a) (*(volatile u32 *)(a))
#define FETCH16(a) (*(volatile u16 *)(a))
#define MIRROR(c)  ((volatile u8 *)(c)->mirror)
#endif

#define SLOT2_START 0x08000000u
#define SLOT2_END   0x0C000000u /* ROM 0x08/0x09, SRAM 0x0A, open bus to the DSi extra-RAM mirror */
#define SLOT2_SLOT_END SLOT2_END
#define SRAM_START  0x0A000000u
#define SRAM_END    0x0A010000u

#define FLASH_READY 0u
#define FLASH_AA    1u
#define FLASH_55    2u
#define MODE_ID      1u
#define MODE_ERASE   2u
#define MODE_PROGRAM 4u
#define MODE_BANK    8u

static int inSlot2(u32 address) {
	return address >= SLOT2_START && address < SLOT2_END;
}

static void markDirty(Slot2EmuControl *c, u32 offset) {
	u32 sector = offset / SLOT2EMU_SECTOR;
	if (sector < SLOT2EMU_SECTORS) c->dirty[sector] = 1;
	c->writeSeq++;
}

/* ---- ROM space ---------------------------------------------------------- */

static u8 romByte(Slot2EmuControl *c, u32 offset) {
	if (offset < SLOT2EMU_HEADER_SIZE) return c->header[offset];
	if (offset == 0x1FFFE) return (u8)c->moduleImage;
	if (offset == 0x1FFFF) return (u8)(c->moduleImage >> 8);
	/* GBA open bus: the ROM address lines echo back the halfword address. */
	u32 open = (offset >> 1) & 0xFFFF;
	return (u8)((offset & 1) ? open >> 8 : open);
}

/* ---- Backup chip (SRAM / flash) ---------------------------------------- */

static u8 backupRead(Slot2EmuControl *c, u32 a) {
	a &= 0xFFFF;
	if (c->type == SLOT2EMU_TYPE_SRAM) return MIRROR(c)[a & (c->flashSize - 1)];
	if ((c->flashMode & MODE_ID) && a < 2) return a ? c->flashDevice : c->flashMaker;
	u32 offset = c->flashBank * 0x10000 + a;
	if (offset >= c->flashSize) return 0xFF;
	return MIRROR(c)[offset];
}

static void fillMirror(Slot2EmuControl *c, u32 offset, u32 length) {
	volatile u8 *m = MIRROR(c);
	for (u32 i = 0; i < length && offset + i < c->flashSize; i++) m[offset + i] = 0xFF;
	for (u32 i = 0; i < length; i += SLOT2EMU_SECTOR) markDirty(c, offset + i);
}

static void backupWrite(Slot2EmuControl *c, u32 a, u8 v) {
	a &= 0xFFFF;
	if (c->type == SLOT2EMU_TYPE_SRAM) {
		a &= c->flashSize - 1;
		MIRROR(c)[a] = v;
		markDirty(c, a);
		return;
	}

	if (c->flashMode & MODE_PROGRAM) {
		u32 offset = c->flashBank * 0x10000 + a;
		c->flashMode &= ~MODE_PROGRAM;
		if (offset < c->flashSize) {
			MIRROR(c)[offset] = v;
			markDirty(c, offset);
		}
		return;
	}
	if ((c->flashMode & MODE_BANK) && a == 0) {
		c->flashMode &= ~MODE_BANK;
		if (c->flashSize > 0x10000) c->flashBank = v & 1;
		return;
	}

	switch (c->flashState) {
	case FLASH_READY:
		if (a == 0x5555 && v == 0xAA) c->flashState = FLASH_AA;
		else if (v == 0xF0) c->flashMode &= ~MODE_ID;
		return;
	case FLASH_AA:
		c->flashState = (a == 0x2AAA && v == 0x55) ? FLASH_55 : FLASH_READY;
		return;
	default:
		break;
	}

	c->flashState = FLASH_READY;
	if (c->flashMode & MODE_ERASE) {
		c->flashMode &= ~MODE_ERASE;
		if (a == 0x5555 && v == 0x10) {
			fillMirror(c, 0, c->flashSize);
		} else if (v == 0x30) {
			fillMirror(c, c->flashBank * 0x10000 + (a & 0xF000), SLOT2EMU_SECTOR);
		}
		return;
	}
	if (a != 0x5555) return;
	if (v == 0x90) c->flashMode |= MODE_ID;
	else if (v == 0xF0) c->flashMode &= ~MODE_ID;
	else if (v == 0x80) c->flashMode |= MODE_ERASE;
	else if (v == 0xA0) c->flashMode |= MODE_PROGRAM;
	else if (v == 0xB0) c->flashMode |= MODE_BANK;
}

/* ---- Bus ----------------------------------------------------------------- */

static u32 busRead(Slot2EmuControl *c, u32 address, u32 size) {
	if (address >= SRAM_START) {
		/* The backup bus is 8 bits wide; wider reads see the byte repeated. */
		u32 v = (address < SRAM_END) ? backupRead(c, address) : 0xFF;
		return v * (size == 1 ? 1u : size == 2 ? 0x0101u : 0x01010101u);
	}
	u32 offset = address & 0x01FFFFFF, v = 0;
	for (u32 i = 0; i < size; i++) v |= (u32)romByte(c, offset + i) << (8 * i);
	return v;
}

static void busWrite(Slot2EmuControl *c, u32 address, u32 size, u32 value) {
	if (address < SRAM_START || address >= SRAM_END) return; /* ROM/GPIO writes are ignored */
	/* Like the GBA: a wide store puts the byte selected by the address. */
	backupWrite(c, address, (u8)(value >> (8 * (address & (size - 1)))));
}

static u32 read32Rotated(Slot2EmuControl *c, u32 address) {
	u32 v = busRead(c, address & ~3u, 4), rot = (address & 3) * 8;
	return rot ? (v >> rot) | (v << (32 - rot)) : v;
}

/* ---- ARM instructions --------------------------------------------------- */

static u32 shiftedRegister(u32 i, u32 *r, u32 spsr, int *ok) {
	u32 rm = i & 15, amount = (i >> 7) & 31, v;
	if (rm > 14) { *ok = 0; return 0; }
	v = r[rm];
	switch ((i >> 5) & 3) {
	case 0: return v << amount;
	case 1: return amount ? v >> amount : 0;
	case 2: return amount ? (u32)((int)v >> amount) : (u32)((int)v >> 31);
	default:
		if (amount) return (v >> amount) | (v << (32 - amount));
		return (v >> 1) | ((spsr & 0x20000000u) ? 0x80000000u : 0); /* RRX */
	}
}

static u32 emulateArm(Slot2EmuControl *c, u32 *r, u32 pc, u32 spsr) {
	const u32 i = FETCH32(pc);
	const u32 P = (i >> 24) & 1, U = (i >> 23) & 1, W = (i >> 21) & 1, L = (i >> 20) & 1;
	const u32 rn = (i >> 16) & 15, rd = (i >> 12) & 15;

	if ((i & 0x0C000000u) == 0x04000000u) {
		/* LDR/STR/LDRB/STRB */
		u32 offset;
		int ok = 1;
		if (rn > 14 || rd > 14) return 0;
		if (i & 0x02000000u) {
			if (i & 0x10) return 0; /* media/undefined space */
			offset = shiftedRegister(i, r, spsr, &ok);
			if (!ok) return 0;
		} else {
			offset = i & 0xFFF;
		}
		const u32 base = r[rn], moved = U ? base + offset : base - offset;
		const u32 address = P ? moved : base;
		const u32 byte = (i >> 22) & 1;
		if (!inSlot2(address)) return 0;
		u32 loaded = 0;
		if (L) loaded = byte ? busRead(c, address, 1) : read32Rotated(c, address);
		else if (byte) busWrite(c, address, 1, r[rd]);
		else busWrite(c, address & ~3u, 4, r[rd]);
		if (!P || W) r[rn] = moved;
		if (L) r[rd] = loaded;
		return pc + 4;
	}

	if ((i & 0x0E000090u) == 0x00000090u && (i & 0x60u)) {
		/* LDRH/STRH/LDRSB/LDRSH/LDRD/STRD */
		const u32 sh = (i >> 5) & 3;
		u32 offset;
		if (rn > 14 || rd > 14) return 0;
		if (i & 0x00400000u) {
			offset = ((i >> 4) & 0xF0) | (i & 0xF);
		} else {
			if ((i & 15) > 14) return 0;
			offset = r[i & 15];
		}
		const u32 base = r[rn], moved = U ? base + offset : base - offset;
		const u32 address = P ? moved : base;
		if (!inSlot2(address)) return 0;
		if (!L && sh != 1) {
			/* LDRD (sh 2) / STRD (sh 3) */
			if ((rd & 1) || rd + 1 > 14) return 0;
			if (sh == 2) {
				u32 lo = busRead(c, address & ~3u, 4), hi = busRead(c, (address & ~3u) + 4, 4);
				if (!P || W) r[rn] = moved;
				r[rd] = lo;
				r[rd + 1] = hi;
			} else {
				busWrite(c, address & ~3u, 4, r[rd]);
				busWrite(c, (address & ~3u) + 4, 4, r[rd + 1]);
				if (!P || W) r[rn] = moved;
			}
			return pc + 4;
		}
		u32 loaded = 0;
		if (!L) busWrite(c, address & ~1u, 2, r[rd]);
		else if (sh == 1) loaded = busRead(c, address & ~1u, 2);
		else if (sh == 2) loaded = (u32)(int)(s8)busRead(c, address, 1);
		else loaded = (u32)(int)(s16)busRead(c, address & ~1u, 2);
		if (!P || W) r[rn] = moved;
		if (L) r[rd] = loaded;
		return pc + 4;
	}

	if ((i & 0x0E000000u) == 0x08000000u) {
		/* LDM/STM without user-bank or PC transfers */
		const u32 list = i & 0xFFFF;
		u32 count = 0;
		if ((i & 0x00400000u) || rn > 14 || !list || (list & 0x8000)) return 0;
		for (u32 k = 0; k < 15; k++) count += (list >> k) & 1;
		const u32 base = r[rn];
		u32 address = U ? (P ? base + 4 : base) : (P ? base - 4 * count : base - 4 * count + 4);
		const u32 newBase = U ? base + 4 * count : base - 4 * count;
		if (!inSlot2(address)) return 0;
		address &= ~3u;
		for (u32 k = 0; k < 15; k++) {
			if (!((list >> k) & 1)) continue;
			if (L) r[k] = busRead(c, address, 4);
			else busWrite(c, address, 4, k == rn ? base : r[k]);
			address += 4;
		}
		if (W && !(L && ((list >> rn) & 1))) r[rn] = newBase;
		return pc + 4;
	}

	return 0;
}

/* ---- Thumb instructions ------------------------------------------------- */

static u32 emulateThumb(Slot2EmuControl *c, u32 *r, u32 pc) {
	const u32 i = FETCH16(pc);
	const u32 rd = i & 7, rn = (i >> 3) & 7;
	u32 address;

	if ((i >> 12) == 0x5) {
		/* register offset: STR STRH STRB LDRSB LDR LDRH LDRB LDRSH */
		const u32 op = (i >> 9) & 7;
		address = r[rn] + r[(i >> 6) & 7];
		if (!inSlot2(address)) return 0;
		switch (op) {
		case 0: busWrite(c, address & ~3u, 4, r[rd]); break;
		case 1: busWrite(c, address & ~1u, 2, r[rd]); break;
		case 2: busWrite(c, address, 1, r[rd]); break;
		case 3: r[rd] = (u32)(int)(s8)busRead(c, address, 1); break;
		case 4: r[rd] = read32Rotated(c, address); break;
		case 5: r[rd] = busRead(c, address & ~1u, 2); break;
		case 6: r[rd] = busRead(c, address, 1); break;
		default: r[rd] = (u32)(int)(s16)busRead(c, address & ~1u, 2); break;
		}
		return pc + 2;
	}

	const u32 imm5 = (i >> 6) & 31;
	switch (i >> 11) {
	case 0x0C: /* STR  [rn, #imm5*4] */
	case 0x0D: /* LDR  [rn, #imm5*4] */
		address = r[rn] + imm5 * 4;
		if (!inSlot2(address)) return 0;
		if (i & 0x0800) r[rd] = read32Rotated(c, address);
		else busWrite(c, address & ~3u, 4, r[rd]);
		return pc + 2;
	case 0x0E: /* STRB [rn, #imm5] */
	case 0x0F: /* LDRB [rn, #imm5] */
		address = r[rn] + imm5;
		if (!inSlot2(address)) return 0;
		if (i & 0x0800) r[rd] = busRead(c, address, 1);
		else busWrite(c, address, 1, r[rd]);
		return pc + 2;
	case 0x10: /* STRH [rn, #imm5*2] */
	case 0x11: /* LDRH [rn, #imm5*2] */
		address = r[rn] + imm5 * 2;
		if (!inSlot2(address)) return 0;
		if (i & 0x0800) r[rd] = busRead(c, address & ~1u, 2);
		else busWrite(c, address & ~1u, 2, r[rd]);
		return pc + 2;
	case 0x18: /* STMIA rb!, {list} */
	case 0x19: { /* LDMIA rb!, {list} */
		const u32 rb = (i >> 8) & 7, list = i & 0xFF, load = (i >> 11) & 1;
		u32 count = 0;
		if (!list) return 0;
		for (u32 k = 0; k < 8; k++) count += (list >> k) & 1;
		const u32 base = r[rb];
		address = base & ~3u;
		if (!inSlot2(base)) return 0;
		for (u32 k = 0; k < 8; k++) {
			if (!((list >> k) & 1)) continue;
			if (load) r[k] = busRead(c, address, 4);
			else busWrite(c, address, 4, k == rb ? base : r[k]);
			address += 4;
		}
		if (!(load && ((list >> rb) & 1))) r[rb] = base + 4 * count;
		return pc + 2;
	}
	default:
		return 0;
	}
}

/* ---- Bulk byte loops ----------------------------------------------------
 * The backup bus is 8 bits wide, so CTRDG copies and verifies a save one
 * byte at a time: a 4 KiB sector is 4096 aborts. When the faulting LDRB is
 * the head of one of the SDK's byte loops, run the loop here instead, then
 * resume at the LDRB so the game does the final iteration (and sets its own
 * flags and return value) itself. Anything that doesn't match exactly falls
 * through to single-instruction emulation. */
#define BULK_MAX 0x1000u /* bytes per abort: bounds the time spent with IRQs off */

static int ramRange(u32 a, u32 n) {
	return a >= 0x02000000u && a + n <= 0x03000000u && a + n > a;
}

/* With the MPU off, accesses bypass the D-cache, so write back (and, when
 * we're about to overwrite the memory, drop) the game's cached lines first. */
static void dcacheRange(u32 a, u32 n, int invalidate) {
#ifndef SLOT2EMU_HOST
	for (u32 line = a & ~31u; line < a + n; line += 32) {
		if (invalidate) __asm__ volatile("mcr p15, 0, %0, c7, c14, 1" :: "r"(line) : "memory");
		else __asm__ volatile("mcr p15, 0, %0, c7, c10, 1" :: "r"(line) : "memory");
	}
	__asm__ volatile("mcr p15, 0, %0, c7, c10, 4" :: "r"(0) : "memory");
#else
	(void)a; (void)n; (void)invalidate;
#endif
}

static int distinct(u32 a, u32 b, u32 c, u32 d, u32 e) {
	if (a > 14 || b > 14 || c > 14 || d > 14 || e > 14) return 0;
	return a != b && a != c && a != d && a != e && b != c && b != d && b != e &&
	       c != d && c != e && d != e;
}

/* CTRDGi_ReadAgbFlashCore:
 *   L: ldrb rd, [rn], #1 / cmp rc, #0 / sub rc, rc, #1 / strb rd, [rm], #1 / bne L */
static u32 bulkRead(Slot2EmuControl *c, u32 *r, u32 pc, u32 i0) {
	const u32 rn = (i0 >> 16) & 15, rd = (i0 >> 12) & 15;
	const u32 i1 = FETCH32(pc + 4), rc = (i1 >> 16) & 15;
	const u32 i3 = FETCH32(pc + 12), rm = (i3 >> 16) & 15;
	if ((i1 & 0xFFF0FFFFu) != 0xE3500000u) return 0;
	if (FETCH32(pc + 8) != (0xE2400001u | rc << 16 | rc << 12)) return 0;
	if ((i3 & 0xFFF0FFFFu) != (0xE4C00001u | rd << 12)) return 0;
	if (FETCH32(pc + 16) != 0x1AFFFFFAu) return 0;
	if (rn > 14 || rd > 14 || rc > 14 || rm > 14) return 0;
	if (rn == rd || rn == rc || rn == rm || rd == rc || rd == rm || rc == rm) return 0;
	/* Each pass tests rc before decrementing it, so rc more passes keep looping. */
	u32 n = r[rc];
	if (n > BULK_MAX) n = BULK_MAX;
	const u32 src = r[rn], dst = r[rm];
	if (!n || src < SRAM_START || src + n > SRAM_END || !ramRange(dst, n)) return 0;
	dcacheRange(dst, n, 1);
	volatile u8 *out = (volatile u8 *)dst;
	u32 v = 0;
	for (u32 k = 0; k < n; k++) {
		v = backupRead(c, src + k);
		out[k] = (u8)v;
	}
	r[rn] = src + n;
	r[rm] = dst + n;
	r[rc] -= n;
	r[rd] = v;
	c->bulkReads++;
	return pc;
}

/* CTRDGi_VerifyFlashSectorCore (flash pointer first):
 *   L: ldrb ra, [rf], #1 / ldrb rb, [rr], #1 / cmp ra, rb / subne x, rf, #1 /
 *      bxne lr / cmp rc, #0 / sub rc, rc, #1 / bne L */
static u32 bulkVerify(Slot2EmuControl *c, u32 *r, u32 pc, u32 i0) {
	const u32 rf = (i0 >> 16) & 15, ra = (i0 >> 12) & 15;
	const u32 i1 = FETCH32(pc + 4), rr = (i1 >> 16) & 15, rb = (i1 >> 12) & 15;
	const u32 i5 = FETCH32(pc + 20), rc = (i5 >> 16) & 15;
	if ((i1 & 0xFFF00FFFu) != 0xE4D00001u) return 0;
	if (FETCH32(pc + 8) != (0xE1500000u | ra << 16 | rb)) return 0;
	if ((FETCH32(pc + 12) & 0xFFFF0FFFu) != (0x12400001u | rf << 16)) return 0;
	if ((FETCH32(pc + 16) & 0xFFFFFFF0u) != 0x112FFF10u) return 0;
	if ((i5 & 0xFFF0FFFFu) != 0xE3500000u) return 0;
	if (FETCH32(pc + 24) != (0xE2400001u | rc << 16 | rc << 12)) return 0;
	if (FETCH32(pc + 28) != 0x1AFFFFF7u) return 0;
	if (!distinct(rf, ra, rr, rb, rc)) return 0;
	u32 limit = r[rc];
	if (limit > BULK_MAX) limit = BULK_MAX;
	const u32 flash = r[rf], ram = r[rr];
	if (!limit || flash < SRAM_START || flash + limit > SRAM_END || !ramRange(ram, limit)) return 0;
	dcacheRange(ram, limit, 0);
	volatile const u8 *in = (volatile const u8 *)ram;
	u32 n = 0;
	while (n < limit && backupRead(c, flash + n) == in[n]) n++;
	if (!n) return 0; /* first byte differs: let the game see the mismatch */
	r[rf] = flash + n;
	r[rr] = ram + n;
	r[rc] -= n;
	c->bulkVerifies++;
	return pc;
}

/* Programming one flash byte is AA/55/A0 command writes, the data write and
 * status polling (CTRDG_IsExisting included): about six aborts a byte, ~25,000
 * for a 4 KiB sector. Emulate the SDK's Macronix byte routine as a whole, and
 * when it was called from the sector loop, the rest of the sector as well.
 *
 * CTRDGi_ProgramFlashByteMX(src, dst), faulting at its first command write:
 *   stmdb sp!, {lr} / sub sp, sp, #4 / ldr ip, =0x0A005555 / mov r3, #0xAA /
 *   ldr r2, =0x0A002AAA / strb r3, [ip]   <- fault
 *   mov r3, #0x55 / mov lr, r0 / strb r3, [r2] / mov r0, #0xA0 / strb r0, [ip] /
 *   ldrb r2, [lr] / ldr r3, =CTRDGi_PollingSR / mov r0, #1 / strb r2, [r1] / ...
 * CTRDGi_WriteFlashSectorCoreMX, around the call (L = return address):
 *   L-12: mov r0, r4 / mov r1, r5 / bl Program
 *   L:    movs r6, r0 / bne done / ldrh r0, [r8] / add r4, r4, #1 /
 *         add r5, r5, #1 / sub r0, r0, #1 / strh r0, [r8] / ldrh r0, [r8] /
 *         cmp r0, #0 / bne L-12
 *   done: (L+0x28) */
static int programByte(Slot2EmuControl *c, u32 address, u8 v) {
	const u32 offset = c->flashBank * 0x10000 + (address & 0xFFFF);
	if (offset >= c->flashSize) return 0;
	MIRROR(c)[offset] = v;
	markDirty(c, offset);
	return 1;
}

static u32 programLoop(Slot2EmuControl *c, u32 *r, u32 pc, u32 i0) {
	if (c->type != SLOT2EMU_TYPE_FLASH128 || i0 != 0xE5CC3000u) return 0;
	/* Newer SDK builds use push {r3,lr} / pop {r3,pc} instead of
	 * push {lr} / sub sp,#4. Both put the caller's LR at sp+4. */
	const int modern = FETCH32(pc - 0x10) == 0xE92D4008u;
	if (!modern && (FETCH32(pc - 0x14) != 0xE92D4000u || FETCH32(pc - 0x10) != 0xE24DD004u)) return 0;
	if (FETCH32(pc + 0x04) != 0xE3A03055u || FETCH32(pc + 0x08) != 0xE1A0E000u ||
	    FETCH32(pc + 0x0C) != 0xE5C23000u || FETCH32(pc + 0x10) != 0xE3A000A0u ||
	    FETCH32(pc + 0x14) != 0xE5CC0000u || FETCH32(pc + 0x18) != 0xE5DE2000u ||
	    (FETCH32(pc + 0x1C) & 0xFFFFF000u) != 0xE59F3000u ||
	    FETCH32(pc + 0x20) != 0xE3A00001u || FETCH32(pc + 0x24) != 0xE5C12000u) return 0;
	if (FETCH32(pc + 0x28) != 0xE5DE2000u || FETCH32(pc + 0x2C) != 0xE5933000u ||
	    FETCH32(pc + 0x30) != 0xE12FFF33u) return 0;
	if (modern) {
		if (FETCH32(pc + 0x34) != 0xE8BD8008u) return 0;
	} else if (FETCH32(pc + 0x34) != 0xE28DD004u || FETCH32(pc + 0x38) != 0xE8BD4000u ||
	           FETCH32(pc + 0x3C) != 0xE12FFF1Eu) return 0;
	if (r[12] != 0x0A005555u || r[2] != 0x0A002AAAu || r[3] != 0xAAu) return 0;
	if (c->flashState != FLASH_READY || (c->flashMode & (MODE_ERASE | MODE_PROGRAM | MODE_BANK))) return 0;

	const u32 sp = r[13], ret = r[14], src = r[0], dst = r[1];
	if (!ramRange(sp, 8) || (sp & 3) || !ramRange(src, 1) || dst < SRAM_START || dst >= SRAM_END) return 0;
	dcacheRange(sp, 8, 0);
	if (*(volatile u32 *)(sp + 4) != ret) return 0; /* the LR it pushed */

	/* Match the loop's instructions, allowing the SDK's different allocations
	 * of callee-saved registers for source, destination, result and count. */
	u32 count = 1, rs = 0, rd = 0, remaining = 0;
	if (ramRange(ret - 12, 0x34) && !(ret & 3)) {
		rs = FETCH32(ret - 12) & 15;
		rd = FETCH32(ret - 8) & 15;
		const u32 result = (FETCH32(ret) >> 12) & 15;
		remaining = (FETCH32(ret + 8) >> 16) & 15;
		if (rs >= 4 && rs <= 11 && rd >= 4 && rd <= 11 && result >= 4 && result <= 11 &&
		    remaining >= 4 && remaining <= 11 && distinct(rs, rd, result, remaining, 0) &&
		    FETCH32(ret - 12) == (0xE1A00000u | rs) && FETCH32(ret - 8) == (0xE1A01000u | rd) &&
		    (FETCH32(ret - 4) & 0xFF000000u) == 0xEB000000u &&
		    FETCH32(ret) == (0xE1B00000u | result << 12) && FETCH32(ret + 0x04) == 0x1A000007u &&
		    FETCH32(ret + 0x08) == (0xE1D000B0u | remaining << 16) &&
		    FETCH32(ret + 0x0C) == (0xE2800001u | rs << 16 | rs << 12) &&
		    FETCH32(ret + 0x10) == (0xE2800001u | rd << 16 | rd << 12) && FETCH32(ret + 0x14) == 0xE2400001u &&
		    FETCH32(ret + 0x18) == (0xE1C000B0u | remaining << 16) &&
		    FETCH32(ret + 0x1C) == (0xE1D000B0u | remaining << 16) &&
		    FETCH32(ret + 0x20) == 0xE3500000u && FETCH32(ret + 0x24) == 0x1AFFFFF2u &&
		    r[rs] == src && r[rd] == dst && ramRange(r[remaining], 2) && !(r[remaining] & 1)) {
			dcacheRange(r[remaining], 2, 1);
			const u32 left = *(volatile u16 *)r[remaining];
			if (left && left <= BULK_MAX && (dst & 0xFFFF) + left <= 0x10000 && ramRange(src, left)) {
				count = left;
			} else remaining = 0;
		} else remaining = 0;
	}

	dcacheRange(src, count, 0);
	volatile const u8 *in = (volatile const u8 *)src;
	for (u32 k = 0; k < count; k++) {
		if (!programByte(c, dst + k, in[k])) return 0; /* can't happen: range checked */
	}

	r[13] = sp + 8; /* the routine's epilogue: add sp, sp, #4 / ldmia sp!, {lr} */
	r[0] = 0;       /* success */
	if (modern) r[3] = *(volatile u32 *)sp;
	if (remaining) {
		/* Return through the caller's final iteration so it sets its own
		 * result, flags and final pointers, just as the read fast path does. */
		*(volatile u16 *)r[remaining] = 1;
		r[rs] = src + count - 1;
		r[rd] = dst + count - 1;
	}
	c->bulkPrograms++;
	return ret;
}

static u32 bulkLoop(Slot2EmuControl *c, u32 *r, u32 pc) {
#ifdef SLOT2EMU_HOST
	(void)c; (void)r; (void)pc;
	return 0;
#else
	const u32 i0 = FETCH32(pc);
	if (i0 == 0xE5CC3000u) return programLoop(c, r, pc, i0);
	if ((i0 & 0xFFF00FFFu) != 0xE4D00001u) return 0; /* ldrb rX, [rY], #1 */
	u32 next = bulkRead(c, r, pc, i0);
	if (!next) next = bulkVerify(c, r, pc, i0);
	return next;
#endif
}

static void recordPc(Slot2EmuControl *c, u32 pc) {
	for (u32 k = 0; k < SLOT2EMU_HIST; k++) {
		if (c->histPc[k] == pc) { c->histCount[k]++; return; }
		if (!c->histPc[k]) { c->histPc[k] = pc; c->histCount[k] = 1; return; }
	}
	c->histOther++;
}

/* Called from the BIOS debug vector in abort mode with the MPU disabled.
 * r points at the faulting context's r0-r14 (r13/r14 from its own bank). Returns the address to resume
 * at, or 0 to pass the exception on to the previous handler. */
u32 slot2EmuAbort(Slot2EmuControl *c, u32 *r, u32 lrAbort, u32 spsr) {
	if (c->magic != SLOT2EMU_MAGIC) return 0;
	/* A data abort leaves LR = aborted instruction + 8 in ARM and Thumb state. */
	const u32 pc = lrAbort - 8;
	recordPc(c, pc);
	u32 next = (spsr & 0x20) ? 0 : bulkLoop(c, r, pc);
	if (next) {
		c->bulk++;
		return next;
	}
	next = (spsr & 0x20) ? emulateThumb(c, r, pc) : emulateArm(c, r, pc, spsr);
	if (next) c->aborts++;
	else c->unhandled++;
	return next;
}

#ifndef SLOT2EMU_HOST
static u32 readRegion(u32 n) {
	u32 v = 0;
	switch (n) {
	case 0: __asm__ volatile("mrc p15, 0, %0, c6, c0, 0" : "=r"(v)); break;
	case 1: __asm__ volatile("mrc p15, 0, %0, c6, c1, 0" : "=r"(v)); break;
	case 2: __asm__ volatile("mrc p15, 0, %0, c6, c2, 0" : "=r"(v)); break;
	case 3: __asm__ volatile("mrc p15, 0, %0, c6, c3, 0" : "=r"(v)); break;
	case 4: __asm__ volatile("mrc p15, 0, %0, c6, c4, 0" : "=r"(v)); break;
	case 5: __asm__ volatile("mrc p15, 0, %0, c6, c5, 0" : "=r"(v)); break;
	case 6: __asm__ volatile("mrc p15, 0, %0, c6, c6, 0" : "=r"(v)); break;
	default: __asm__ volatile("mrc p15, 0, %0, c6, c7, 0" : "=r"(v)); break;
	}
	return v;
}

static void writeRegion(u32 n, u32 v) {
	switch (n) {
	case 0: __asm__ volatile("mcr p15, 0, %0, c6, c0, 0" :: "r"(v)); break;
	case 1: __asm__ volatile("mcr p15, 0, %0, c6, c1, 0" :: "r"(v)); break;
	case 2: __asm__ volatile("mcr p15, 0, %0, c6, c2, 0" :: "r"(v)); break;
	case 3: __asm__ volatile("mcr p15, 0, %0, c6, c3, 0" :: "r"(v)); break;
	case 4: __asm__ volatile("mcr p15, 0, %0, c6, c4, 0" :: "r"(v)); break;
	case 5: __asm__ volatile("mcr p15, 0, %0, c6, c5, 0" :: "r"(v)); break;
	case 6: __asm__ volatile("mcr p15, 0, %0, c6, c6, 0" :: "r"(v)); break;
	default: __asm__ volatile("mcr p15, 0, %0, c6, c7, 0" :: "r"(v)); break;
	}
}

/* Called by the cardengine's trampoline with the MPU (and so the caches)
 * disabled and IRQs masked; the D-cache has been cleaned beforehand. */
void slot2EmuInstall(u32 valueBits, u32 cp15ctrl, Slot2EmuControl *c, u32 abortEntry) {
	if (c->magic != SLOT2EMU_MAGIC) return;

	/* Same debug-vector choice as the cardengine's own exception handler. */
	const u32 vector = (valueBits & (1u << 11)) ? 0x02FFFD9Cu : 0x027FFD9Cu;
	volatile u32 *v = (volatile u32 *)vector;
	c->cp15ctrl = cp15ctrl;
	c->biosStack = vector - 0xC;
	if (*v != abortEntry) {
		c->prevVector = *v;
		*v = abortEntry;
		c->installs++;
		c->blobBuild = SLOT2EMU_BLOB_BUILD;
	}

	/* Unmap the slot so that every access to it aborts. NitroSDK maps it with
	 * region 3 (0x08000000, 128 MiB), which also covers the DSi's extra RAM at
	 * 0x0C000000 where nds-bootstrap keeps its ROM cache; such a region is
	 * narrowed to 0x0C000000-0x0FFFFFFF (same number, same permissions), and
	 * a region that only covers slot addresses is disabled. CTRDG_Enable()
	 * changes only permissions, never these base/size/enable bits. */
	for (u32 n = 0; n < 8; n++) {
		const u32 region = readRegion(n);
		if (!(region & 1)) continue;
		const u32 size = 2u << ((region >> 1) & 31);
		const u32 base = region & ~(size - 1) & 0xFFFFF000u;
		const unsigned long long end = (unsigned long long)base + size;
		if (end <= SLOT2_START || base >= SLOT2_SLOT_END) continue;
		if (base < SLOT2_START || end > 0x10000000ull) continue; /* a background region: leave it */
		writeRegion(n, end > SLOT2_SLOT_END ? SLOT2EMU_EXTRAM_REGION : (region & ~1u));
	}

	/* Keep the SDK's boot-time cartridge record describing this cartridge.
	 * The ARM7 recomputes the "is AGB" bit by reading the real (empty) slot
	 * during CTRDG_Init, so it is restored here after every such update. */
	volatile u8 *info = (volatile u8 *)SLOT2EMU_MODULE_INFO;
	info[0] = (u8)c->moduleId;
	info[1] = (u8)(c->moduleId >> 8);
	info[2] = c->exLsiId[0];
	info[3] = c->exLsiId[1];
	info[4] = c->exLsiId[2];
	info[5] = (u8)((info[5] & ~3u) | 1u);   /* isAgbCartridge, not pulled out */
	info[6] = (u8)c->makerCode;
	info[7] = (u8)(c->makerCode >> 8);
	*(volatile u32 *)(SLOT2EMU_MODULE_INFO + 8) = c->gameCode;
}
#endif
