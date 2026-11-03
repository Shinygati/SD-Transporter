/* Virtual Pokéwalker for HeartGold/SoulSilver, shared between the bootloader,
 * the ARM9 walker blob and the ARM7 file flusher. GPL-3.0-or-later.
 *
 * HG/SS reach the Pokéwalker through the IR chip on the game card, with three
 * ARM9 library routines: CARD_SpiWaitReadRange (receive a packet),
 * CARD_SpiWaitWriteRange (send a packet) and CARD_SpiWaitGetStatus (chip
 * check). When the game runs from SD there is no IR chip, so the bootloader
 * replaces those routines with a small stub that calls this walker instead.
 * The walker answers like a real Pokéwalker, using a 64 KiB EEPROM image
 * loaded from a per-save file on SD; the ARM7 writes changed 4 KiB sectors
 * back to that file. Steps, watts, found Pokémon and items are added to the
 * same file by a companion app; the console side only speaks the protocol.
 *
 * The bootloader reserves WALKER_RESERVE bytes at the end of the ROM cache:
 *   +0x00000  EEPROM image (64 KiB)
 *   +0x10000  WalkerControl (one 4 KiB page)
 *   +0x11000  ARM9 walker blob (position independent)
 * The blob only runs with the MPU off (the stub turns it off), so it reads
 * and writes this area and the game's buffers uncached; the stub cleans and
 * invalidates the D-cache first. */
#ifndef WALKER_EMU_H
#define WALKER_EMU_H

#define WALKER_RESERVE      0x14000u
#define WALKER_EEPROM_OFF   0x00000u
#define WALKER_CONTROL_OFF  0x10000u
#define WALKER_BLOB_OFF     0x11000u
#define WALKER_BLOB_MAX     0x03000u

#define WALKER_MAGIC        0x4B4C5750u /* 'PWLK' */
#define WALKER_VERSION      1u
#define WALKER_EEPROM_SIZE  0x10000u
#define WALKER_SECTOR       0x1000u
#define WALKER_SECTORS      16u

/* Stub operations (r1 of the blob entry). */
#define WALKER_OP_READ      0u /* a = buffer; returns bytes received (plain) */
#define WALKER_OP_WRITE     1u /* a = buffer (0xAA-scrambled), b = length */
#define WALKER_OP_INIT      2u /* the game is setting up a new IR session */

#define WALKER_PACKET_MAX   0x88u /* 8-byte header + 128-byte payload */
/* While linked, this many reads in a row with nothing to answer means the
 * game has dropped the link (cancelled, finished, or timed out without a
 * disconnect): go back to advertising so it can connect again. */
#define WALKER_IDLE_READS   120u

/* Like the button on a real walker: the virtual walker only answers the
 * game's connection search after SELECT is pressed, so the game's own
 * "cancel" and the soft-reset combo keep working while it searches.
 * Holding SELECT + LEFT during the search wipes the walker to a new,
 * unregistered one (the same as deleting the .walker file). */
#define WALKER_KEY_REG      0x04000130u /* REG_KEYINPUT, 0 = pressed */
#define WALKER_KEY_SELECT   0x0004u
#define WALKER_KEY_LEFT     0x0020u

/* Offsets used by tests; checked by static asserts. */
#define WALKER_CTL_DIRTY    0x20

#ifndef __ASSEMBLER__
typedef struct WalkerControl {
	/* Written once by the bootloader. */
	unsigned int magic;
	unsigned int version;
	unsigned int cluster;      /* walker file on SD (first cluster) */
	unsigned int eeprom;       /* address of the 64 KiB image */
	/* Write-back bookkeeping, shared with the ARM7. */
	unsigned int writeSeq;     /* incremented after every EEPROM change */
	unsigned int flushedSeq;
	unsigned int flushes;
	unsigned int idleReads;    /* reads in a row with nothing to answer (ARM9) */
	unsigned char dirty[WALKER_SECTORS]; /* ARM9 sets, ARM7 clears */
	/* Link state (ARM9 only). */
	unsigned int state;        /* 0 idle (advertising), 1 linked to the game */
	unsigned char ownSession[4];
	unsigned char session[4];  /* ownSession ^ the game's */
	unsigned int respLen;      /* pending answer, returned by the next read */
	/* Diagnostics, readable from an in-game-menu RAM dump. */
	unsigned int calls, reads, writes, badPackets, unknownCommands;
	unsigned int lastIndex;
	unsigned char lastCommands[16]; /* ring of received command bytes */
	unsigned char peer[0x68];  /* identity the game sent (0x32 and aliases) */
	unsigned char packet[WALKER_PACKET_MAX + 8]; /* descrambled request */
	unsigned char resp[WALKER_PACKET_MAX + 8];   /* plain answer */
	unsigned char scratch[0x300]; /* decompression and copies */
	unsigned int armed;        /* SELECT was pressed: advertise until linked */
	unsigned int resets;       /* walker wiped with SELECT + LEFT */
	unsigned int wipeHeld;     /* SELECT + LEFT still held since the last wipe */
} WalkerControl;

#define WALKER_ASSERT(name, cond) typedef char walker_assert_##name[(cond) ? 1 : -1]
WALKER_ASSERT(dirty, __builtin_offsetof(WalkerControl, dirty) == WALKER_CTL_DIRTY);
WALKER_ASSERT(size, sizeof(WalkerControl) <= WALKER_BLOB_OFF - WALKER_CONTROL_OFF);
#endif

#endif
