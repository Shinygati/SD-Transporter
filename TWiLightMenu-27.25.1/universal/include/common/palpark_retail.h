/* SD Transporter retail-ROM verification. GPL-3.0-or-later.
 * Keep identical to TWiLightMenu's universal/include/common/palpark_retail.h.
 * SHA-1 references: pret/pokediamond *.us.sha1; pret/pokeplatinum README.md.
 * Full untrimmed dumps only. A title ID/header alone never enables a patch. */
#ifndef PALPARK_RETAIL_H
#define PALPARK_RETAIL_H
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
	uint32_t h[5];
	uint64_t bytes;
	unsigned used;
	unsigned char block[64];
} PalParkSha1;

static inline uint32_t palParkRotate(uint32_t x, unsigned n) {
	return (x << n) | (x >> (32 - n));
}

static inline void palParkSha1Block(PalParkSha1 *s) {
	uint32_t w[80], a = s->h[0], b = s->h[1], c = s->h[2], d = s->h[3], e = s->h[4];
	unsigned i;
	for (i = 0; i < 16; i++) {
		const unsigned char *p = s->block + 4*i;
		w[i] = (uint32_t)p[0]<<24 | (uint32_t)p[1]<<16 | (uint32_t)p[2]<<8 | p[3];
	}
	for (; i < 80; i++) w[i] = palParkRotate(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
	for (i = 0; i < 80; i++) {
		uint32_t f, k, t;
		if (i < 20) { f = (b & c) | (~b & d); k = 0x5a827999; }
		else if (i < 40) { f = b ^ c ^ d; k = 0x6ed9eba1; }
		else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8f1bbcdc; }
		else { f = b ^ c ^ d; k = 0xca62c1d6; }
		t = palParkRotate(a, 5) + f + e + k + w[i];
		e = d; d = c; c = palParkRotate(b, 30); b = a; a = t;
	}
	s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d; s->h[4] += e;
}

static inline void palParkSha1Init(PalParkSha1 *s) {
	s->h[0] = 0x67452301; s->h[1] = 0xefcdab89; s->h[2] = 0x98badcfe;
	s->h[3] = 0x10325476; s->h[4] = 0xc3d2e1f0; s->bytes = 0; s->used = 0;
}

static inline void palParkSha1Update(PalParkSha1 *s, const unsigned char *p, unsigned n) {
	s->bytes += n;
	while (n) {
		unsigned take = 64 - s->used;
		if (take > n) take = n;
		memcpy(s->block + s->used, p, take);
		s->used += take; p += take; n -= take;
		if (s->used == 64) { palParkSha1Block(s); s->used = 0; }
	}
}

static inline void palParkSha1Final(PalParkSha1 *s, char hex[41]) {
	const uint64_t bits = s->bytes * 8;
	const unsigned char pad = 0x80, zero = 0;
	unsigned char length[8];
	unsigned i;
	palParkSha1Update(s, &pad, 1);
	while (s->used != 56) palParkSha1Update(s, &zero, 1);
	for (i = 0; i < 8; i++) length[i] = (unsigned char)(bits >> (56 - i*8));
	palParkSha1Update(s, length, 8);
	for (i = 0; i < 40; i++) hex[i] = "0123456789abcdef"[(s->h[i/8] >> (28 - 4*(i%8))) & 15];
	hex[40] = 0;
}

/* Paths can contain '[' or ']': use a fixed, INI-safe per-path section key. */
static inline void palParkPathKey(const char *path, char hex[41]) {
	PalParkSha1 sha;
	palParkSha1Init(&sha);
	palParkSha1Update(&sha, (const unsigned char *)path, (unsigned)strlen(path));
	palParkSha1Final(&sha, hex);
}

typedef struct { const char *tid, *sha1; unsigned revision, size; } PalParkRetail;
static const PalParkRetail palParkRetail[] = {
	{"ADAE", "a46233d8b79a69ea87aa295a0efad5237d02841e", 5, 0x4000000},
	{"APAE", "99083bf15ec7c6b81b4ba241ee10abd9e80999ac", 5, 0x4000000},
	{"CPUE", "0862ec35b24de5c7e2dcb88c9eea0873110d755c", 1, 0x8000000},
	{"CPUE", "ce81046eda7d232513069519cb2085349896dec7", 0, 0x8000000}
};

static inline int palParkCandidate(const char *tid) {
	unsigned i;
	for (i = 0; i < sizeof(palParkRetail)/sizeof(palParkRetail[0]); i++)
		if (!memcmp(tid, palParkRetail[i].tid, 4)) return 1;
	return 0;
}

/* Quick original-ROM check, done on every launch: hashing a whole 64/128 MiB
 * dump took long enough to look like a white-screen hang. The header must be
 * a listed title and revision with intact checksums (the logo CRC and the
 * header CRC16, which also covers the ARM9/ARM7 offsets and sizes). Trimmed
 * dumps are fine. The real safety check is palParkPatchArm9(): it changes the
 * game only if the exact routine occurs exactly once in the decompressed ARM9
 * binary, so a hack or a different build is simply left untouched. */
static inline unsigned palParkCrc16(const unsigned char *p, unsigned n) {
	unsigned crc = 0xffff, bit;
	while (n--) {
		crc ^= *p++;
		for (bit = 0; bit < 8; bit++) crc = (crc & 1) ? (crc >> 1) ^ 0xa001 : crc >> 1;
	}
	return crc;
}

static inline int palParkVerifyHeader(const char *path) {
	unsigned char header[0x160];
	unsigned i;
	FILE *f = fopen(path, "rb");
	if (!f) return 0;
	const int complete = fread(header, 1, sizeof(header), f) == sizeof(header);
	fclose(f);
	if (!complete) return 0;
	if ((unsigned)(header[0x15c] | header[0x15d] << 8) != 0xcf56u) return 0;
	if ((unsigned)(header[0x15e] | header[0x15f] << 8) != palParkCrc16(header, 0x15e)) return 0;
	for (i = 0; i < sizeof(palParkRetail)/sizeof(palParkRetail[0]); i++)
		if (!memcmp(header + 12, palParkRetail[i].tid, 4) && header[30] == palParkRetail[i].revision) return 1;
	return 0;
}
#endif
