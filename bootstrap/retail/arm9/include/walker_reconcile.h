/* Virtual Pokewalker: launch-time consistency repair. GPL-3.0-or-later.
 *
 * HGSS keeps its own record of a strolling Pokemon in the save (the
 * POKEWALKER block's BoxPokemon, non-zero while one is out). The walker file
 * keeps the other half. If a link was interrupted after the walker applied a
 * send or return but before HGSS saved, the two disagree and the game either
 * refuses to send ("a Pokemon is already on the Pokewalker") or cannot bring
 * its Pokemon home. Before every launch, compare them and repair:
 *
 *   save: out,  walker: same partner   consistent -> refresh .lastgood
 *   save: none, walker: empty          consistent -> refresh .lastgood
 *   save: none, walker: has a partner  leftover of an unfinished send, or a
 *                                      return HGSS saved but the walker did not:
 *                                      the game already has that Pokemon.
 *                                      End the stroll on the walker.
 *   save: out,  walker: empty/wiped/   the walker lost its stroll: restore the
 *         other partner                newest matching .lastgood or prelaunch
 *                                      snapshot.
 *   save: none, walker: unregistered   pairing was cut off after the game's reset
 *         or blank                     but before it wrote its trainer details
 *                                      (or the walker was wiped by accident), so
 *                                      HGSS says "This Pokewalker has not been
 *                                      registered". Restore the newest copy that
 *                                      is registered to this trainer, with any
 *                                      stroll on it ended. A walker that is
 *                                      wiped (SELECT + LEFT held) or brand new
 *                                      (file deleted) is all 0xFF outside its
 *                                      identity records and is left alone, so
 *                                      starting over still works.
 *
 * Only runs for a registered walker whose Trainer ID and Secret ID match the
 * save. A walker that is out on a phone is never reached (the caller skips
 * leased walkers first), and partners that joined an empty walker under older
 * versions (ID_FLAG_JOINED) are left alone. The caller has already taken the
 * prelaunch snapshot, so every repair can be undone through Recovery.
 *
 * "<walker>.lastgood" is written and read only here; the companions do not
 * list it. Pure stdio/dirent so it can be checked on a host.
 */
#ifndef WALKER_RECONCILE_H
#define WALKER_RECONCILE_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <string>

namespace walkerReconcile {

static const unsigned kWalkerSize = 0x10000;
static const unsigned kSaveSize = 0x80000;
static const unsigned kGeneralSize = 0xF628;
static const unsigned kStorageOffset = 0xF700, kStorageSize = 0x12310;
static const unsigned kStorageMons = 18 * 30;
static const unsigned kPartyCount = 0x94, kParty = 0x98, kPartyStride = 0xEC;
static const unsigned kTrainerId = 0x74;      /* TID, SID (u16 each) */
static const unsigned kWalkerBoxMon = 0xE5E0; /* POKEWALKER + 4 */
static const unsigned kBoxMonSize = 0x88;
static const unsigned ID1 = 0xED, ID2 = 0x1ED, IDN = 0x68;
static const unsigned HD1 = 0x156, HD2 = 0x256, HDN = 0x18;
static const unsigned ROUTE = 0x8F00;

enum Action { NONE = 0, LASTGOOD, ENDED_STROLL, RESTORED, REREGISTERED };

struct Mon { bool valid; unsigned species; unsigned moves[4]; };

static unsigned le16(const uint8_t *p) { return p[0] | p[1] << 8; }
static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

static unsigned crc16(const uint8_t *p, unsigned n) {
	unsigned c = 0xFFFF;
	while (n--) { c ^= (unsigned)*p++ << 8; for (int i = 0; i < 8; i++) c = ((c << 1) ^ ((c & 0x8000) ? 0x1021 : 0)) & 0xFFFF; }
	return c;
}

/* Gen IV save block footer: counter, size, 0x20060623, block id, CRC16.
 * The block id is 0 for the general block and 1 for PC storage. */
static bool blockValid(const uint8_t *save, unsigned base, unsigned size, unsigned id, uint32_t *counter) {
	const uint8_t *f = save + base + size - 16;
	if (le32(f + 4) != size || le32(f + 8) != 0x20060623u || le16(f + 12) != id) return false;
	if (crc16(save + base, size - 16) != le16(f + 14)) return false;
	*counter = le32(f);
	return true;
}

/* Newest valid copy of a block (the two copies sit 0x40000 apart). */
static int pickBlock(const uint8_t *save, unsigned offset, unsigned size, unsigned id) {
	uint32_t a = 0, b = 0;
	bool va = blockValid(save, offset, size, id, &a), vb = blockValid(save, 0x40000 + offset, size, id, &b);
	if (va && vb) return (int)(b > a ? 0x40000 + offset : offset);
	if (va) return (int)offset;
	if (vb) return (int)(0x40000 + offset);
	return -1;
}

static const char kOrder[24][5] = {"ABCD","ABDC","ACBD","ACDB","ADBC","ADCB","BACD","BADC","BCAD","BCDA","BDAC","BDCA",
	"CABD","CADB","CBAD","CBDA","CDAB","CDBA","DABC","DACB","DBAC","DBCA","DCAB","DCBA"};

/* Decrypt a Gen IV BoxPokemon far enough to read species and moves. */
static Mon decode(const uint8_t *raw) {
	Mon m; memset(&m, 0, sizeof(m));
	const uint32_t pid = le32(raw);
	const unsigned chk = le16(raw + 6);
	uint8_t data[0x80];
	uint32_t seed = chk, sum = 0;
	for (unsigned i = 0; i < 0x80; i += 2) {
		seed = seed * 0x41C64E6Du + 0x6073u;
		const unsigned w = le16(raw + 8 + i) ^ (seed >> 16);
		data[i] = (uint8_t)w; data[i + 1] = (uint8_t)(w >> 8);
		sum += w;
	}
	if ((sum & 0xFFFF) != chk) return m;
	const char *order = kOrder[((pid >> 13) & 31) % 24];
	const uint8_t *A = 0, *B = 0;
	for (int i = 0; i < 4; i++) { if (order[i] == 'A') A = data + i * 32; if (order[i] == 'B') B = data + i * 32; }
	m.species = le16(A);
	for (int i = 0; i < 4; i++) m.moves[i] = le16(B + i * 2);
	m.valid = m.species > 0 && m.species <= 493;
	return m;
}

static bool anySet(const uint8_t *p, unsigned n) { while (n--) if (*p++) return true; return false; }

struct SaveView {
	bool ok, out;
	unsigned tid, sid;
	Mon walking;
	const uint8_t *save; int storage, general;
};

static SaveView readSave(const uint8_t *save) {
	SaveView v; memset(&v, 0, sizeof(v));
	v.save = save;
	v.general = pickBlock(save, 0, kGeneralSize, 0);
	v.storage = pickBlock(save, kStorageOffset, kStorageSize, 1);
	if (v.general < 0) return v;
	const uint8_t *g = save + v.general;
	v.tid = le16(g + kTrainerId); v.sid = le16(g + kTrainerId + 2);
	v.out = anySet(g + kWalkerBoxMon, kBoxMonSize);
	if (v.out) { v.walking = decode(g + kWalkerBoxMon); if (!v.walking.valid) return v; }
	v.ok = true;
	return v;
}

static bool sameMoves(const unsigned a[4], const uint8_t *summary) {
	for (int i = 0; i < 4; i++) if (a[i] != le16(summary + 4 + i * 2)) return false;
	return true;
}

/* Is a Pokemon matching the walker partner (species and moves) in the
 * party or PC? Then the walker copy is a confirmed duplicate. */
static bool inGame(const SaveView &v, const uint8_t *summary) {
	const unsigned species = le16(summary);
	const uint8_t *g = v.save + v.general;
	const unsigned party = g[kPartyCount] <= 6 ? g[kPartyCount] : 0;
	for (unsigned i = 0; i < party; i++) {
		Mon m = decode(g + kParty + i * kPartyStride);
		if (m.valid && m.species == species && sameMoves(m.moves, summary)) return true;
	}
	if (v.storage >= 0)
		for (unsigned i = 0; i < kStorageMons; i++) {
			const uint8_t *raw = v.save + v.storage + i * kBoxMonSize;
			if (!anySet(raw, 8)) continue;
			Mon m = decode(raw);
			if (m.valid && m.species == species && sameMoves(m.moves, summary)) return true;
		}
	return false;
}

static uint8_t recSum(const uint8_t *p, unsigned n) { unsigned s = 1; while (n--) s += *p++; return (uint8_t)s; }
static const uint8_t *reliable(const uint8_t *ee, unsigned a, unsigned b, unsigned n) {
	if (recSum(ee + a, n) == ee[a + n]) return ee + a;
	if (recSum(ee + b, n) == ee[b + n]) return ee + b;
	return 0;
}
static void mirror(uint8_t *ee, unsigned a, unsigned b, const uint8_t *rec, unsigned n) {
	uint8_t tmp[IDN];
	memcpy(tmp, rec, n);
	memcpy(ee + a, tmp, n); ee[a + n] = recSum(tmp, n);
	memcpy(ee + b, ee + a, n + 1);
}

struct WalkerView { bool ok, setUp, registered, hasPokemon, joined; unsigned tid, sid; uint32_t steps; };

static WalkerView readWalker(const uint8_t *ee) {
	WalkerView w; memset(&w, 0, sizeof(w));
	w.setUp = !memcmp(ee, "nintendo", 8);
	if (!w.setUp) { w.ok = true; return w; }
	const uint8_t *id = reliable(ee, ID1, ID2, IDN), *hd = reliable(ee, HD1, HD2, HDN);
	if (!id || !hd) return w;
	w.registered = id[0x5B] & 1; w.hasPokemon = (id[0x5B] & 2) != 0; w.joined = (id[0x5B] & 4) != 0;
	w.tid = le16(id + 0x0C); w.sid = le16(id + 0x0E);
	w.steps = be32(hd);
	w.ok = true;
	return w;
}

/* The same changes the walker makes when HGSS takes its Pokemon back. */
static void endStroll(uint8_t *ee) {
	uint8_t id[IDN], hd[HDN];
	memcpy(id, reliable(ee, ID1, ID2, IDN), IDN);
	memset(id + 0x04, 0, 4); memset(id + 0x0A, 0, 2);
	id[0x5B] &= (uint8_t)~2;
	mirror(ee, ID1, ID2, id, IDN);
	memcpy(hd, reliable(ee, HD1, HD2, HDN), HDN);
	hd[0x0E] = hd[0x0F] = 0; hd[0x13] = 0;
	mirror(ee, HD1, HD2, hd, HDN);
	memset(ee + 0xCE8C, 0, 0x64);
	memset(ee + 0xCF0C, 0, 0xCC0);
	memset(ee + 0xB800, 0, 0x6C8);
	memset(ee + 0xDE24, 0, 0x1568);
	memset(ee + ROUTE, 0, 0x10);
	memset(ee + 0x272, 0xFF, 10);
}

/* A snapshot that can stand in for the walker while `v.walking` is out. */
static bool restorable(const uint8_t *ee, const SaveView &v) {
	WalkerView w = readWalker(ee);
	return w.ok && w.setUp && w.registered && w.hasPokemon && !w.joined && w.tid == v.tid && w.sid == v.sid &&
		le16(ee + ROUTE) == v.walking.species && sameMoves(v.walking.moves, ee + ROUTE);
}

/* Wiped or newly created: the route block a used walker always fills is
 * still erased flash. */
static bool freshlyErased(const uint8_t *ee) {
	for (unsigned i = 0; i < 0x52; i++) if (ee[ROUTE + i] != 0xFF) return false;
	return true;
}

/* A copy that is registered to this save's trainer (any stroll state). */
static bool registeredTo(const uint8_t *ee, const SaveView &v) {
	WalkerView w = readWalker(ee);
	return w.ok && w.setUp && w.registered && w.tid == v.tid && w.sid == v.sid;
}

static bool readExact(const std::string &path, uint8_t *out, unsigned size) {
	FILE *f = fopen(path.c_str(), "rb");
	if (!f) return false;
	bool ok = fread(out, 1, size, f) == size && fgetc(f) == EOF;
	if (fclose(f)) ok = false;
	return ok;
}

static bool sameFile(const std::string &path, const uint8_t *data, uint8_t *scratch) {
	return readExact(path, scratch, kWalkerSize) && !memcmp(scratch, data, kWalkerSize);
}

/* Rewrite the walker file in place: its clusters, which the cardengine is
 * given at boot, stay the same. */
static bool writeInPlace(const std::string &path, const uint8_t *data, uint8_t *scratch) {
	FILE *f = fopen(path.c_str(), "r+b");
	if (!f) return false;
	bool ok = fwrite(data, 1, kWalkerSize, f) == kWalkerSize;
	if (fflush(f)) ok = false;
	if (fclose(f)) ok = false;
	return ok && sameFile(path, data, scratch);
}

static bool writeLastGood(const std::string &path, const uint8_t *data, uint8_t *scratch) {
	const std::string lastGood = path + ".lastgood", temp = lastGood + ".tmp";
	if (sameFile(lastGood, data, scratch)) return true;
	FILE *f = fopen(temp.c_str(), "wb");
	if (!f) return false;
	bool ok = fwrite(data, 1, kWalkerSize, f) == kWalkerSize;
	if (fclose(f)) ok = false;
	if (!ok || !sameFile(temp, data, scratch)) { remove(temp.c_str()); return false; }
	remove(lastGood.c_str()); /* libfat cannot rename over an existing file */
	return rename(temp.c_str(), lastGood.c_str()) == 0 && sameFile(lastGood, data, scratch);
}

/* Newest restorable copy: .lastgood first, else the prelaunch snapshot with
 * the most lifetime steps. */
typedef bool (*Usable)(const uint8_t *ee, const SaveView &v);
static bool findRestore(const std::string &path, const SaveView &v, uint8_t *out, uint8_t *scratch, std::string *source,
		Usable usable = restorable) {
	if (readExact(path + ".lastgood", out, kWalkerSize) && usable(out, v)) { *source = path + ".lastgood"; return true; }
	const size_t slash = path.find_last_of('/');
	const std::string folder = (slash == std::string::npos ? std::string() : path.substr(0, slash + 1)) + "transfers/backups";
	const std::string prefix = (slash == std::string::npos ? path : path.substr(slash + 1)) + ".prelaunch-";
	DIR *dir = opendir(folder.c_str());
	if (!dir) return false;
	bool found = false; uint32_t best = 0;
	while (struct dirent *e = readdir(dir)) {
		const size_t n = strlen(e->d_name);
		if (strncmp(e->d_name, prefix.c_str(), prefix.size()) || n < 4 || strcmp(e->d_name + n - 4, ".bak")) continue;
		const std::string candidate = folder + "/" + e->d_name;
		if (!readExact(candidate, scratch, kWalkerSize) || !usable(scratch, v)) continue;
		const uint32_t steps = readWalker(scratch).steps;
		if (!found || steps >= best) { memcpy(out, scratch, kWalkerSize); best = steps; found = true; *source = candidate; }
	}
	closedir(dir);
	return found;
}

/* Returns the action taken; *note says what happened, for the user. */
static Action run(const std::string &walkerPath, const std::string &savePath, std::string *note) {
	note->clear();
	uint8_t *save = (uint8_t *)malloc(kSaveSize), *ee = (uint8_t *)malloc(kWalkerSize),
		*other = (uint8_t *)malloc(kWalkerSize), *scratch = (uint8_t *)malloc(kWalkerSize);
	Action action = NONE;
	if (!save || !ee || !other || !scratch) goto done;
	if (!readExact(savePath, save, kSaveSize) || !readExact(walkerPath, ee, kWalkerSize)) goto done;
	{
		const SaveView v = readSave(save);
		const WalkerView w = readWalker(ee);
		if (!v.ok || !w.ok) goto done;
		const bool trainerMatches = w.registered && w.tid == v.tid && w.sid == v.sid;

		if (!v.out) {
			if (!w.setUp || !w.registered) {
				/* Unregistered or blank: put back this trainer's last registered
				 * walker. The game has no Pokemon out, so end any stroll on it. */
				std::string source;
				if (freshlyErased(ee) || !findRestore(walkerPath, v, other, scratch, &source, registeredTo)) goto done;
				const WalkerView r = readWalker(other);
				if (r.hasPokemon && !r.joined) endStroll(other);
				if (writeInPlace(walkerPath, other, scratch)) {
					action = REREGISTERED;
					*note = "The walker had lost its registration (most likely a pairing that was cut off part way), so HGSS would say it has not been registered. Your registered walker, with its steps and watts, was restored from " + source + ".";
					writeLastGood(walkerPath, other, scratch);
				}
				goto done;
			}
			if (!w.hasPokemon) {
				if (trainerMatches && writeLastGood(walkerPath, ee, scratch)) action = LASTGOOD;
				goto done;
			}
			/* The walker holds a partner the game says is not out. */
			if (!trainerMatches || w.joined) goto done;
			const bool duplicate = inGame(v, ee + ROUTE);
			memcpy(other, ee, kWalkerSize);
			endStroll(other);
			if (writeInPlace(walkerPath, other, scratch)) {
				action = ENDED_STROLL;
				*note = duplicate
					? "The walker still held a Pokemon that is already back in your game (party or PC). That leftover copy was removed so you can send a Pokemon again."
					: "The walker held a Pokemon your save says is not out on a stroll. The game cannot take it back, so the stroll was ended so you can send a Pokemon again.";
				writeLastGood(walkerPath, other, scratch);
			}
			goto done;
		}

		/* The game says a Pokemon is out. */
		if (trainerMatches && w.hasPokemon && !w.joined && le16(ee + ROUTE) == v.walking.species &&
			sameMoves(v.walking.moves, ee + ROUTE)) {
			if (writeLastGood(walkerPath, ee, scratch)) action = LASTGOOD;
			goto done;
		}
		if (w.registered && !trainerMatches) goto done; /* another trainer's walker: leave it */
		std::string source;
		if (findRestore(walkerPath, v, other, scratch, &source) && writeInPlace(walkerPath, other, scratch)) {
			action = RESTORED;
			*note = "Your save says a Pokemon is out on a stroll, but the walker had lost it. The walker was restored from " + source + ".";
			writeLastGood(walkerPath, other, scratch);
		}
	}
done:
	free(save); free(ee); free(other); free(scratch);
	return action;
}

} // namespace walkerReconcile
#endif
