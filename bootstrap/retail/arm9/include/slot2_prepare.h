/* Virtual GBA Slot-2 cartridge: launcher-side selection. GPL-3.0-or-later.
 *
 * sd:/_nds/nds-bootstrap/slot2.ini (written by TWiLight Menu++'s per-game
 * "Set as GBA Slot-2 cartridge", or by hand) selects a GBA ROM and/or save:
 *
 *   [SLOT2]
 *   GBA_ROM_PATH = sd:/roms/gba/Pokemon Emerald.gba
 *   GBA_SAV_PATH = sd:/roms/gba/saves/Pokemon Emerald.sav
 *   GBA_GAME     =            (only used when no ROM is given: RUBY, SAPPHIRE,
 *                              EMERALD, FIRERED, LEAFGREEN or a 4-letter code)
 *
 * Any DS game then sees that cartridge in its GBA slot; there is no list of
 * supported games. Problems never block a launch: the cartridge is left out
 * and the reason is written to sd:/_nds/nds-bootstrap/slot2-error.txt.
 */
#ifndef SLOT2_PREPARE_H
#define SLOT2_PREPARE_H
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include "configuration.h"

#define SLOT2_ERROR_PATH "sd:/_nds/nds-bootstrap/slot2-error.txt"

static bool slot2Skip(configuration *conf, const char *reason) {
	conf->slot2SaveCluster = conf->slot2SaveSize = conf->slot2GameCode = conf->slot2RomCluster = 0;
	FILE *log = fopen(SLOT2_ERROR_PATH, "w");
	if (log) {
		fprintf(log, "GBA Slot-2 cartridge not inserted: %s\nGBA_ROM_PATH=%s\nGBA_SAV_PATH=%s\nDS game=%s\n",
			reason, conf->slot2RomPath ? conf->slot2RomPath : "", conf->slot2SavePath ? conf->slot2SavePath : "", conf->ndsPath);
		fclose(log);
	}
	return true;
}

static u32 slot2CodeForKey(const char *key) {
	static const char *const names[5] = {"RUBY", "SAPPHIRE", "EMERALD", "FIRERED", "LEAFGREEN"};
	static const char codes[5][5] = {"AXVE", "AXPE", "BPEE", "BPRE", "BPGE"};
	if (!key || !key[0]) return 0;
	for (int i = 0; i < 5; i++) {
		if (!strcasecmp(key, names[i])) return codes[i][0] | codes[i][1] << 8 | codes[i][2] << 16 | (u32)codes[i][3] << 24;
	}
	if (strlen(key) == 4) return key[0] | key[1] << 8 | key[2] << 16 | (u32)key[3] << 24;
	return 0;
}

static bool slot2Prepare(configuration *conf, u32 dsSaveCluster, u32 dsRomCluster) {
	conf->slot2SaveCluster = conf->slot2SaveSize = conf->slot2GameCode = conf->slot2RomCluster = 0;
	/* slot2Reserved0 bit 0: verified, opt-in Pal Park bypass (set in conf_sd.cpp) */
	const bool haveSave = conf->slot2SavePath && conf->slot2SavePath[0];
	const bool haveRom = conf->slot2RomPath && conf->slot2RomPath[0];
	if (!haveSave && !haveRom) return true; /* no cartridge selected */

	/* DS games only: DSi-enhanced/exclusive titles and Download Play are left alone. */
	unsigned char nds[0x200];
	FILE *f = fopen(conf->ndsPath, "rb");
	if (!f) return true;
	const bool read = fread(nds, 1, sizeof(nds), f) == sizeof(nds);
	fclose(f);
	if (!read || nds[0x12] != 0 || !memcmp(nds + 12, "HND", 3)) return true;
	remove(SLOT2_ERROR_PATH);

	if (conf->b4dsMode || !conf->sdFound || conf->gameOnFlashcard)
		return slot2Skip(conf, "needs SD card launching (not B4DS or a flashcard)");
	if (!haveSave) return slot2Skip(conf, "no GBA save selected (GBA_SAV_PATH)");
	if (strncmp(conf->slot2SavePath, "sd:/", 4)) return slot2Skip(conf, "the GBA save must be on the SD card");

	struct stat st;
	if (stat(conf->slot2SavePath, &st)) return slot2Skip(conf, "GBA save not found");
	const u32 size = st.st_size;
	if (size != 0x8000 && size != 0x10000 && size != 0x20000 && size != 0x20010)
		return slot2Skip(conf, "GBA save must be 32, 64 or 128 KiB (SRAM or flash)");
	if (st.st_ino < 2 || (u32)st.st_ino == dsSaveCluster || (u32)st.st_ino == dsRomCluster)
		return slot2Skip(conf, "GBA save is empty or is the DS game's own file");
	const u32 saveCluster = st.st_ino;

	u32 romCluster = 0, gameCode = 0;
	if (haveRom) {
		if (strncmp(conf->slot2RomPath, "sd:/", 4)) return slot2Skip(conf, "the GBA ROM must be on the SD card");
		if (stat(conf->slot2RomPath, &st) || st.st_size < 0x200 || st.st_ino < 2)
			return slot2Skip(conf, "GBA ROM not found");
		unsigned char header[0x200];
		f = fopen(conf->slot2RomPath, "rb");
		const bool ok = f && fread(header, 1, sizeof(header), f) == sizeof(header);
		if (f) fclose(f);
		if (!ok || header[0xB2] != 0x96) return slot2Skip(conf, "not a GBA ROM (bad header)");
		romCluster = st.st_ino;
		gameCode = header[0xAC] | header[0xAD] << 8 | header[0xAE] << 16 | (u32)header[0xAF] << 24;
	} else {
		gameCode = slot2CodeForKey(conf->slot2GameKey);
		if (!gameCode) return slot2Skip(conf, "no GBA ROM selected and GBA_GAME is not a known game");
	}

	conf->slot2SaveCluster = saveCluster;
	conf->slot2SaveSize = size;
	conf->slot2RomCluster = romCluster;
	conf->slot2GameCode = gameCode;
	return true;
}
#endif
