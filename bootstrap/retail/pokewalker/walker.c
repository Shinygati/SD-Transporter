/* Virtual Pokéwalker: the walker side of the HeartGold/SoulSilver IR
 * protocol, answering the game from a 64 KiB EEPROM image. GPL-3.0-or-later.
 *
 * Written from the publicly documented protocol (Dmitry Grinberg's
 * Pokéwalker research; command numbers and EEPROM layout as also used by
 * the picowalker project). No code from either is included.
 *
 * Packets: 8-byte header {command, extra, checksum (LE), session (4)} and up
 * to 128 bytes of payload, every byte XORed with 0xAA on the wire. The game
 * is always the master: the walker advertises (a lone 0xFC), the game asserts
 * master (0xFA), the walker acknowledges (0xF8) and both XOR their session
 * IDs together. After that the game sends requests and the walker answers
 * each one.
 *
 * This file is linked as a position-independent blob: no globals, no
 * constant tables; all state lives in WalkerControl. It runs with the MPU off,
 * called from the stub that replaces the game's IR routines.
 */
#include "walker_emu.h"

typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

/* Commands (game -> walker requests and walker answers). */
#define CMD_EEPROM_WRITE_ACK   0x04
#define CMD_EEPROM_READ_REQ    0x0C
#define CMD_EEPROM_READ_RSP    0x0E
#define CMD_IDENTITY_REQ       0x20
#define CMD_IDENTITY_RSP       0x22
#define CMD_PING               0x24
#define CMD_PONG               0x26
#define CMD_RESET_KEEP_ALL     0x2C
#define CMD_RESET_EVENTS       0x2A
#define CMD_RESET_EVERYTHING   0xE0
#define CMD_WALK_START_INIT    0x38
#define CMD_WALK_START         0x5A
#define CMD_WALK_END_REQ       0x4E
#define CMD_WALK_END_ACK       0x50
#define CMD_CONNECT_COMPLETE   0x66
#define CMD_CONNECT_COMPLETE_ACK 0x68
#define CMD_NORX               0x9C
#define CMD_NORX_ACK           0x9E
#define CMD_DISCONNECT         0xF4
#define CMD_SLAVE_ACK          0xF8
#define CMD_ASSERT_MASTER      0xFA
#define CMD_ADVERTISING        0xFC
#define EXTRA_FROM_WALKER      0x01
#define EXTRA_TO_WALKER        0x02

/* EEPROM layout. "Reliable" records are stored twice, each followed by a
 * checksum byte (1 + sum of the bytes). */
#define EE_NINTENDO            0x0000
#define EE_UNIQUE_ID_1         0x0083
#define EE_UNIQUE_ID_2         0x0183
#define EE_UNIQUE_ID_SIZE      0x28
#define EE_IDENTITY_1          0x00ED
#define EE_IDENTITY_2          0x01ED
#define EE_IDENTITY_SIZE       0x68
#define EE_HEALTH_1            0x0156
#define EE_HEALTH_2            0x0256
#define EE_HEALTH_SIZE         0x18
#define EE_COPY_MARKER_1       0x016F
#define EE_COPY_MARKER_2       0x026F
#define EE_ROUTE_INFO          0x8F00
#define EE_RECEIVED_BITFIELD   0xB800
#define EE_TEAM_DATA           0xCC00
#define EE_TEAM_DATA_SIZE      0x224
#define EE_CURRENT_WATTS_COPY  0xCE8A
#define EE_CAUGHT_SUMMARY      0xCE8C
#define EE_HISTORIC_STEPS      0xCEF0
#define EE_EVENT_LOG           0xCF0C
#define EE_EVENT_LOG_SIZE      0x0CC0
#define EE_TEAM_STAGING        0xD480
#define EE_SCENARIO_STAGING    0xD700
#define EE_SCENARIO_SIZE       0x2900
#define EE_MET_PEERS           0xDE24
#define EE_MET_PEERS_SIZE      0x1568
/* Stroll clock for the 3DS Pokéwalker app, in a spot the walker leaves
 * unused (0x0272-0x027F): 'P' 'T', walk start (u32 BE, seconds since
 * 2000-01-01 by the game's clock), last time steps were credited (u32 BE). */
#define EE_STROLL_CLOCK        0x0272

/* walker identity record (0x68 bytes) */
#define ID_UNK0      0x00 /* u32 */
#define ID_UNK1      0x04 /* u32 */
#define ID_UNK2      0x08 /* u16 */
#define ID_UNK3      0x0A /* u16 */
#define ID_TID       0x0C /* u16 */
#define ID_SID       0x0E /* u16 */
#define ID_UNIQUE    0x10 /* 0x28 bytes */
#define ID_EVENTS    0x38 /* 0x10 bytes */
#define ID_NAME      0x48 /* 8 x u16 */
#define ID_UNK4      0x58 /* 2 bytes */
#define ID_EVENT_IDX 0x5A
#define ID_FLAGS     0x5B
#define ID_PROTO     0x5C
#define ID_UNK5      0x5D
#define ID_PROTO_SUB 0x5E
#define ID_UNK8      0x5F
#define ID_LAST_SYNC 0x60 /* u32 BE */
#define ID_STEPS     0x64 /* u32 BE */
#define ID_FLAG_INIT        0x01
#define ID_FLAG_HAS_POKEMON 0x02
#define ID_FLAG_JOINED      0x04

/* health record (0x18 bytes, big-endian fields) */
#define HD_TOTAL_STEPS  0x00
#define HD_TODAY_STEPS  0x04
#define HD_LAST_SYNC    0x08
#define HD_TOTAL_DAYS   0x0C
#define HD_WATTS        0x0E
#define HD_WALK_MINUTES 0x10
#define HD_STEPS_WATT   0x12
#define HD_EVENT_INDEX  0x13
#define HD_SETTINGS     0x17

#define EEPROM(c) ((volatile u8 *)(c)->eeprom)

/* ---- EEPROM image ------------------------------------------------------- */

static void touch(WalkerControl *c, u32 addr, u32 len) {
	if (!len) return;
	for (u32 s = addr / WALKER_SECTOR; s <= (addr + len - 1) / WALKER_SECTOR && s < WALKER_SECTORS; s++)
		c->dirty[s] = 1;
	c->writeSeq++;
}

static void eeRead(WalkerControl *c, u32 addr, u8 *out, u32 len) {
	for (u32 i = 0; i < len; i++) out[i] = EEPROM(c)[(addr + i) & 0xFFFF];
}

static void eeWrite(WalkerControl *c, u32 addr, const u8 *in, u32 len) {
	if (addr >= WALKER_EEPROM_SIZE) return;
	if (len > WALKER_EEPROM_SIZE - addr) len = WALKER_EEPROM_SIZE - addr;
	for (u32 i = 0; i < len; i++) EEPROM(c)[addr + i] = in[i];
	touch(c, addr, len);
}

static void eeFill(WalkerControl *c, u32 addr, u8 value, u32 len) {
	if (addr >= WALKER_EEPROM_SIZE) return;
	if (len > WALKER_EEPROM_SIZE - addr) len = WALKER_EEPROM_SIZE - addr;
	for (u32 i = 0; i < len; i++) EEPROM(c)[addr + i] = value;
	touch(c, addr, len);
}

static void eeCopy(WalkerControl *c, u32 dst, u32 src, u32 len) {
	for (u32 i = 0; i < len; i++) EEPROM(c)[(dst + i) & 0xFFFF] = EEPROM(c)[(src + i) & 0xFFFF];
	touch(c, dst, len);
}

static u8 recordSum(const u8 *p, u32 len) {
	u8 sum = 1;
	for (u32 i = 0; i < len; i++) sum += p[i];
	return sum;
}

/* Reads the first copy with a good checksum. If neither is good, the first
 * copy is returned as it is (on a blank walker: all 0xFF), like the walker. */
static void reliableRead(WalkerControl *c, u32 a1, u32 a2, u8 *out, u32 len) {
	eeRead(c, a2, out, len);
	if (recordSum(out, len) == EEPROM(c)[a2 + len]) {
		eeRead(c, a1, out, len);
		if (recordSum(out, len) != EEPROM(c)[a1 + len]) eeRead(c, a2, out, len);
		return;
	}
	eeRead(c, a1, out, len);
}

static void reliableWrite(WalkerControl *c, u32 a1, u32 a2, const u8 *in, u32 len) {
	u8 sum = recordSum(in, len);
	eeWrite(c, a1, in, len);
	eeWrite(c, a1 + len, &sum, 1);
	eeWrite(c, a2, in, len);
	eeWrite(c, a2 + len, &sum, 1);
}

static void copyBytes(u8 *dst, const u8 *src, u32 len) {
	for (u32 i = 0; i < len; i++) dst[i] = src[i];
}

/* ---- Packets ------------------------------------------------------------- */

static u32 checksum(const u8 *p, u32 len) {
	/* Seed 2; even-indexed bytes count as the high byte of a 16-bit word,
	 * the checksum field itself as zero; carries folded back in. */
	u32 sum = 2;
	for (u32 i = 0; i < len; i++) {
		u32 v = (i == 2 || i == 3) ? 0 : p[i];
		sum += (i & 1) ? v : v << 8;
	}
	while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
	return sum;
}

static void answer(WalkerControl *c, u8 cmd, u8 extra, u32 payloadLen, int ownSession) {
	u8 *r = c->resp;
	const u8 *s = ownSession ? c->ownSession : c->session;
	r[0] = cmd;
	r[1] = extra;
	r[4] = s[0]; r[5] = s[1]; r[6] = s[2]; r[7] = s[3];
	const u32 len = 8 + payloadLen;
	const u32 sum = checksum(r, len);
	r[2] = (u8)sum;
	r[3] = (u8)(sum >> 8);
	c->respLen = len;
}

/* LZ77 (type 0x10) to exactly 128 bytes; returns 0 on bad data. */
static int decompress(const u8 *in, u32 inLen, u8 *out) {
	if (inLen < 4 || in[0] != 0x10 || (in[1] | in[2] << 8 | in[3] << 16) != 128) return 0;
	u32 i = 4, o = 0;
	while (i < inLen && o < 128) {
		const u8 flags = in[i++];
		for (u32 bit = 0x80; bit && o < 128; bit >>= 1) {
			if (flags & bit) {
				if (i + 1 >= inLen) return 0;
				const u32 n = (in[i] >> 4) + 3, back = (((in[i] & 15) << 8) | in[i + 1]) + 1;
				i += 2;
				if (back > o) return 0;
				for (u32 k = 0; k < n && o < 128; k++, o++) out[o] = out[o - back];
			} else {
				if (i >= inLen) return 0;
				out[o++] = in[i++];
			}
		}
	}
	return o == 128;
}

/* ---- Walker actions ----------------------------------------------------- */

static void putBE32(u8 *p, u32 v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }

/* Walker reset, as requested by the game when it (re)registers the walker. */
static void resetWalker(WalkerControl *c, int clearEvents, int clearSteps) {
	u8 *id = c->scratch, *hd = c->scratch + 0x80;
	reliableRead(c, EE_IDENTITY_1, EE_IDENTITY_2, id, EE_IDENTITY_SIZE);
	for (u32 i = ID_UNK0; i < ID_UNIQUE; i++) id[i] = 0;             /* unk0..3, TID, SID */
	for (u32 i = ID_NAME; i < ID_EVENT_IDX; i++) id[i] = 0;          /* name + unk4 */
	if (clearEvents) for (u32 i = ID_EVENTS; i < ID_NAME; i++) id[i] = 0;
	id[ID_EVENT_IDX] = 0;
	id[ID_FLAGS] = 0;
	id[ID_PROTO_SUB] = 0;
	reliableWrite(c, EE_IDENTITY_1, EE_IDENTITY_2, id, EE_IDENTITY_SIZE);

	reliableRead(c, EE_HEALTH_1, EE_HEALTH_2, hd, EE_HEALTH_SIZE);
	if (clearSteps) {
		putBE32(hd + HD_TOTAL_STEPS, 0);
		putBE32(hd + HD_TODAY_STEPS, 0);
		putBE32(hd + HD_LAST_SYNC, 220924800); /* 2 Jan 2008, the walker's epoch */
		hd[HD_TOTAL_DAYS] = hd[HD_TOTAL_DAYS + 1] = 0;
	}
	hd[HD_WATTS] = hd[HD_WATTS + 1] = 0;
	hd[HD_WALK_MINUTES] = hd[HD_WALK_MINUTES + 1] = 0;
	hd[HD_STEPS_WATT] = 0;
	hd[HD_EVENT_INDEX] = 0;
	hd[HD_SETTINGS] = (hd[HD_SETTINGS] & 0xA4) | 0x24;
	reliableWrite(c, EE_HEALTH_1, EE_HEALTH_2, hd, EE_HEALTH_SIZE);

	if (clearSteps) {
		eeFill(c, 0xCE80, 0, 0xD4C);
	} else {
		eeFill(c, EE_CAUGHT_SUMMARY, 0, 0x64);
		eeFill(c, EE_EVENT_LOG, 0xFF, EE_EVENT_LOG_SIZE);
		eeFill(c, EE_HISTORIC_STEPS, 0, 0x1C);
	}
	if (clearEvents) eeFill(c, EE_RECEIVED_BITFIELD, 0, 0x6C8);
	eeFill(c, EE_MET_PEERS, 0, EE_MET_PEERS_SIZE);

	u8 *magic = c->scratch + 0x100; /* "nintendo": a walker that has been set up */
	magic[0] = 'n'; magic[1] = 'i'; magic[2] = 'n'; magic[3] = 't';
	magic[4] = 'e'; magic[5] = 'n'; magic[6] = 'd'; magic[7] = 'o';
	eeWrite(c, EE_NINTENDO, magic, 8);
}

/* Back to a brand-new walker: blank EEPROM, then the power-on setup. */
static void wipeWalker(WalkerControl *c) {
	eeFill(c, 0, 0xFF, WALKER_EEPROM_SIZE);
	resetWalker(c, 1, 1);
	c->resets++;
}

/* A walker whose EEPROM lacks "nintendo" sets itself up on power-on, keeping
 * whatever else the records hold. A new .walker file is all 0xFF. */
static void ensureSetUp(WalkerControl *c) {
	const volatile u8 *m = EEPROM(c);
	if (m[0] == 'n' && m[1] == 'i' && m[2] == 'n' && m[3] == 't' &&
	    m[4] == 'e' && m[5] == 'n' && m[6] == 'd' && m[7] == 'o') return;
	resetWalker(c, 1, 1);
}

static void startWalk(WalkerControl *c, int newDay) {
	u8 *id = c->scratch, *hd = c->scratch + 0x80, *mark = c->scratch + 0x100;

	/* The game stages the route and team; the walker moves them into place,
	 * bracketed by a copy marker so a power cut can be detected. */
	mark[0] = 0xA5;
	reliableWrite(c, EE_COPY_MARKER_1, EE_COPY_MARKER_2, mark, 1);
	eeCopy(c, EE_ROUTE_INFO, EE_SCENARIO_STAGING, EE_SCENARIO_SIZE);
	eeCopy(c, EE_TEAM_DATA, EE_TEAM_STAGING, EE_TEAM_DATA_SIZE);
	mark[0] = 0;
	reliableWrite(c, EE_COPY_MARKER_1, EE_COPY_MARKER_2, mark, 1);

	reliableRead(c, EE_HEALTH_1, EE_HEALTH_2, hd, EE_HEALTH_SIZE);
	if (newDay) putBE32(hd + HD_TODAY_STEPS, 0);
	hd[HD_WALK_MINUTES] = hd[HD_WALK_MINUTES + 1] = 0;
	hd[HD_EVENT_INDEX] = 0;
	hd[HD_WATTS] = hd[HD_WATTS + 1] = 0;
	copyBytes(hd + HD_LAST_SYNC, c->peer + ID_LAST_SYNC, 4);
	reliableWrite(c, EE_HEALTH_1, EE_HEALTH_2, hd, EE_HEALTH_SIZE);

	eeFill(c, EE_EVENT_LOG, 0, EE_EVENT_LOG_SIZE);
	eeFill(c, EE_MET_PEERS, 0, EE_MET_PEERS_SIZE);
	eeFill(c, EE_CAUGHT_SUMMARY, 0, 0x64);

	/* Take over the trainer details the game sent with its identity. */
	reliableRead(c, EE_IDENTITY_1, EE_IDENTITY_2, id, EE_IDENTITY_SIZE);
	const u8 *p = c->peer;
	copyBytes(id + ID_UNK0, p + ID_UNK0, 4);
	copyBytes(id + ID_UNK1, p + ID_UNK0, 4);
	copyBytes(id + ID_UNK2, p + ID_UNK2, 2);
	copyBytes(id + ID_UNK3, p + ID_UNK2, 2);
	copyBytes(id + ID_TID, p + ID_TID, 4); /* TID + SID */
	copyBytes(id + ID_UNIQUE, p + ID_UNIQUE, 0x28);
	copyBytes(id + ID_NAME, p + ID_NAME, 16);
	id[ID_FLAGS] = (id[ID_FLAGS] & ~ID_FLAG_JOINED) | ID_FLAG_INIT | ID_FLAG_HAS_POKEMON;
	id[ID_PROTO] = p[ID_PROTO];
	id[ID_PROTO_SUB] = p[ID_PROTO_SUB];
	id[ID_UNK5] = p[ID_UNK5];
	id[ID_UNK8] = p[ID_UNK8];
	copyBytes(id + ID_LAST_SYNC, p + ID_LAST_SYNC, 4);
	reliableWrite(c, EE_IDENTITY_1, EE_IDENTITY_2, id, EE_IDENTITY_SIZE);

	/* Start the stroll clock that the 3DS app turns into steps. */
	u8 *clock = c->scratch + 0x100;
	clock[0] = 'P'; clock[1] = 'T';
	copyBytes(clock + 2, p + ID_LAST_SYNC, 4);
	copyBytes(clock + 6, p + ID_LAST_SYNC, 4);
	eeWrite(c, EE_STROLL_CLOCK, clock, 10);
}

static void endWalk(WalkerControl *c) {
	u8 *id = c->scratch, *hd = c->scratch + 0x80;
	reliableRead(c, EE_IDENTITY_1, EE_IDENTITY_2, id, EE_IDENTITY_SIZE);
	for (u32 i = 0; i < 4; i++) id[ID_UNK1 + i] = 0;
	id[ID_UNK3] = id[ID_UNK3 + 1] = 0;
	id[ID_FLAGS] &= ~ID_FLAG_HAS_POKEMON;
	reliableWrite(c, EE_IDENTITY_1, EE_IDENTITY_2, id, EE_IDENTITY_SIZE);

	reliableRead(c, EE_HEALTH_1, EE_HEALTH_2, hd, EE_HEALTH_SIZE);
	hd[HD_WATTS] = hd[HD_WATTS + 1] = 0;
	hd[HD_EVENT_INDEX] = 0;
	reliableWrite(c, EE_HEALTH_1, EE_HEALTH_2, hd, EE_HEALTH_SIZE);

	eeFill(c, EE_CAUGHT_SUMMARY, 0, 0x64);
	eeFill(c, EE_EVENT_LOG, 0, EE_EVENT_LOG_SIZE);
	eeFill(c, EE_RECEIVED_BITFIELD, 0, 0x6C8);
	eeFill(c, EE_MET_PEERS, 0, EE_MET_PEERS_SIZE);
	eeFill(c, EE_ROUTE_INFO, 0, 0x10);
	eeFill(c, EE_STROLL_CLOCK, 0xFF, 10);
}

/* ---- Requests ----------------------------------------------------------- */

static void request(WalkerControl *c, u32 len) {
	u8 *p = c->packet, *r = c->resp;
	const u8 cmd = p[0];
	c->lastCommands[c->lastIndex++ & 15] = cmd;

	if (len < 8 || checksum(p, len) != (u32)(p[2] | p[3] << 8)) {
		c->badPackets++;
		return;
	}

	if (cmd == CMD_ASSERT_MASTER) {
		ensureSetUp(c);
		/* New link. Our session ID changes per link; the game's is mixed in. */
		const u32 seed = c->calls * 0x9E3779B1u ^ c->writeSeq;
		c->ownSession[0] = seed; c->ownSession[1] = seed >> 8;
		c->ownSession[2] = seed >> 16; c->ownSession[3] = seed >> 24;
		for (u32 i = 0; i < 4; i++) c->session[i] = c->ownSession[i] ^ p[4 + i];
		c->state = 1;
		c->armed = 0;
		answer(c, CMD_SLAVE_ACK, EXTRA_TO_WALKER, 0, 1);
		return;
	}
	if (c->state != 1) return; /* not linked: ignore stray packets */
	for (u32 i = 0; i < 4; i++) {
		if (p[4 + i] != c->session[i]) { c->badPackets++; return; }
	}

	switch (cmd) {
	case CMD_IDENTITY_REQ: {
		u8 *id = r + 8, *hd = c->scratch;
		reliableRead(c, EE_IDENTITY_1, EE_IDENTITY_2, id, EE_IDENTITY_SIZE);
		reliableRead(c, EE_HEALTH_1, EE_HEALTH_2, hd, EE_HEALTH_SIZE);
		copyBytes(id + ID_STEPS, hd + HD_TOTAL_STEPS, 4);
		/* The game reads the current watts from here later on. */
		eeWrite(c, EE_CURRENT_WATTS_COPY, hd + HD_WATTS, 2);
		answer(c, CMD_IDENTITY_RSP, EXTRA_FROM_WALKER, EE_IDENTITY_SIZE, 0);
		return;
	}
	case 0x32: case 0x40: case 0x52: case 0x60: { /* game sends its identity */
		const u32 n = len - 8 < sizeof(c->peer) ? len - 8 : sizeof(c->peer);
		copyBytes(c->peer, p + 8, n);
		for (u32 i = n; i < sizeof(c->peer); i++) c->peer[i] = 0;
		const u8 *t = c->peer + ID_LAST_SYNC;
		if (t[0] | t[1] | t[2] | t[3]) {
			u8 *hd = c->scratch;
			reliableRead(c, EE_HEALTH_1, EE_HEALTH_2, hd, EE_HEALTH_SIZE);
			copyBytes(hd + HD_LAST_SYNC, t, 4);
			reliableWrite(c, EE_HEALTH_1, EE_HEALTH_2, hd, EE_HEALTH_SIZE);
		}
		answer(c, cmd + 2, EXTRA_TO_WALKER, 0, 0);
		return;
	}
	case 0x00: case 0x02: case 0x80: case 0x82: { /* write 128 bytes, raw or compressed */
		const u32 addr = (u32)p[1] << 8 | (cmd & 0x80);
		const int compressed = !(cmd & 2) && len < WALKER_PACKET_MAX;
		if (compressed) {
			if (!decompress(p + 8, len - 8, c->scratch)) { c->badPackets++; return; }
			eeWrite(c, addr, c->scratch, 128);
		} else {
			eeWrite(c, addr, p + 8, len - 8 < 128 ? len - 8 : 128);
		}
		answer(c, CMD_EEPROM_WRITE_ACK, EXTRA_FROM_WALKER, 0, 0);
		return;
	}
	case CMD_EEPROM_READ_REQ: {
		const u32 addr = (u32)p[8] << 8 | p[9];
		u32 n = p[10];
		if (n > 128) n = 128;
		eeRead(c, addr, r + 8, n);
		answer(c, CMD_EEPROM_READ_RSP, EXTRA_FROM_WALKER, n, 0);
		return;
	}
	case CMD_PING:
		answer(c, CMD_PONG, EXTRA_FROM_WALKER, 0, 0);
		return;
	case CMD_NORX:
		answer(c, CMD_NORX_ACK, EXTRA_FROM_WALKER, 0, 0);
		return;
	case CMD_CONNECT_COMPLETE:
		/* Last exchange of a session: answer, then go idle like the walker. */
		answer(c, CMD_CONNECT_COMPLETE_ACK, EXTRA_FROM_WALKER, 0, 0);
		c->state = 0;
		return;
	case CMD_RESET_EVENTS: case CMD_RESET_KEEP_ALL: case CMD_RESET_EVERYTHING:
		/* Answer with the walker's pairing identity, then reset. */
		reliableRead(c, EE_UNIQUE_ID_1, EE_UNIQUE_ID_2, r + 8, EE_UNIQUE_ID_SIZE);
		answer(c, cmd, EXTRA_FROM_WALKER, EE_UNIQUE_ID_SIZE, 0);
		resetWalker(c, cmd != CMD_RESET_KEEP_ALL, cmd == CMD_RESET_EVERYTHING);
		return;
	case CMD_WALK_START_INIT: case CMD_WALK_START:
		answer(c, cmd, EXTRA_FROM_WALKER, 0, 0);
		startWalk(c, cmd == CMD_WALK_START_INIT);
		return;
	case CMD_WALK_END_REQ:
		answer(c, CMD_WALK_END_ACK, EXTRA_FROM_WALKER, 0, 0);
		endWalk(c);
		return;
	case CMD_DISCONNECT:
		c->state = 0;
		return;
	case 0x36: case 0x44: case 0x56: case 0x64:
		/* "Can't complete": the game is ending the link with a message of
		 * its own (for example "already registered"). Nothing to answer. */
		c->state = 0;
		return;
	default:
		/* Unknown here: stay silent (the game times out) and record it. */
		c->unknownCommands++;
		return;
	}
}

/* Entry point (offset 0 of the blob), called by the IR stub with the MPU off.
 * Returns the received length for WALKER_OP_READ, the sent length otherwise. */
__attribute__((section(".text.entry")))
u32 walkerEntry(WalkerControl *c, u32 op, u32 a, u32 b) {
	if (c->magic != WALKER_MAGIC) return 0;
	c->calls++;
	if (op == WALKER_OP_READ) {
		u8 *out = (u8 *)a;
		c->reads++;
		if (c->respLen) {
            /* Do not tell HGSS a session completed while the final EEPROM
             * image still exists only in RAM. The ARM7 writer (or DS-mode
             * dispatcher) advances flushedSeq only after verified storage. */
            if (c->resp[0] == CMD_CONNECT_COMPLETE_ACK && c->writeSeq != c->flushedSeq) return 0;
			c->idleReads = 0;
			const u32 n = c->respLen;
			copyBytes(out, c->resp, n);
			c->respLen = 0;
			return n;
		}
		if (c->state == 1 && ++c->idleReads >= WALKER_IDLE_READS) { /* link dropped */
			c->state = 0;
			c->armed = 0;
		}
		if (c->state == 0) {
			const u32 keys = ~*(volatile u16 *)WALKER_KEY_REG;
			const int wipe = (keys & (WALKER_KEY_SELECT | WALKER_KEY_LEFT)) == (WALKER_KEY_SELECT | WALKER_KEY_LEFT);
			if (wipe && !c->wipeHeld) wipeWalker(c); /* once per press */
			c->wipeHeld = wipe;
			if (keys & WALKER_KEY_SELECT) c->armed = 1;
			if (c->armed) { /* advertise until the game takes over */
				out[0] = CMD_ADVERTISING;
				return 1;
			}
		}
		return 0;
	}
	if (op == WALKER_OP_INIT) {
		/* A new session: whatever happened to the last one, start over,
		 * and wait for SELECT again. */
		c->state = 0;
		c->armed = 0;
		c->respLen = 0;
		c->idleReads = 0;
		return 0;
	}
	if (op == WALKER_OP_WRITE) {
		const u8 *in = (const u8 *)a;
		u32 len = b;
		c->writes++;
		c->idleReads = 0;
		if (len > WALKER_PACKET_MAX) len = WALKER_PACKET_MAX;
		for (u32 i = 0; i < len; i++) c->packet[i] = in[i] ^ 0xAA;
		c->respLen = 0; /* a new request supersedes an unread answer */
		if (len >= 8) request(c, len);
		return b;
	}
	return 0;
}
