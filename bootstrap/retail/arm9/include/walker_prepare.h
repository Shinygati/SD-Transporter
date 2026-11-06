/* Virtual Pokéwalker: launcher-side selection. GPL-3.0-or-later.
 *
 * TWiLight Menu++'s per-game "Virtual Pokéwalker" option (HeartGold and
 * SoulSilver only) writes sd:/_nds/nds-bootstrap/pokewalker.ini:
 *
 *   [<SHA-1 of the ROM path>]
 *   VIRTUAL_WALKER = 1
 *   ROM_PATH = sd:/roms/nds/Pokemon HeartGold.nds
 *
 * Each save gets its own walker: next to "<game>.sav" is "<game>.walker",
 * the walker's 64 KiB EEPROM. A new file is created blank (all 0xFF), which
 * the game treats as a walker that has never been registered. The companion
 * app reads and writes the same file. Problems never block a launch: the
 * game simply finds no walker, and the reason is written to
 * sd:/_nds/nds-bootstrap/pokewalker-error.txt.
 */
#ifndef WALKER_PREPARE_H
#define WALKER_PREPARE_H
#include <stdio.h>
#include <string.h>
#include <string>
#include <sys/stat.h>
#include <easysave/ini.hpp>
#include "configuration.h"
#include "palpark_retail.h"
#include "nitrofs.h"
#include "walker_ds_payload.h"
#include "walker_history.h"
#include "walker_reconcile.h"

#define WALKER_INI_PATH(conf) ((conf)->gameOnFlashcard ? "fat:/_nds/nds-bootstrap/pokewalker.ini" : "sd:/_nds/nds-bootstrap/pokewalker.ini")
#define WALKER_ERROR_PATH(conf) ((conf)->gameOnFlashcard ? "fat:/_nds/nds-bootstrap/pokewalker-error.txt" : "sd:/_nds/nds-bootstrap/pokewalker-error.txt")
#define WALKER_REPAIR_PATH(conf) ((conf)->gameOnFlashcard ? "fat:/_nds/nds-bootstrap/pokewalker-repair.txt" : "sd:/_nds/nds-bootstrap/pokewalker-repair.txt")
#define WALKER_FILE_SIZE  0x10000

static void walkerSkip(configuration *conf, const char *reason, const char *path) {
	conf->walkerCluster = 0;
	FILE *log = fopen(WALKER_ERROR_PATH(conf), "w");
	if (log) {
		fprintf(log, "Virtual Pokewalker not connected: %s\nWALKER_PATH=%s\nGAME=%s\n",
			reason, path ? path : "", conf->ndsPath);
		fclose(log);
	}
}

/* "sd:/x/Pokemon HeartGold.sav" -> "sd:/x/Pokemon HeartGold.walker" */
static std::string walkerPathForSave(const char *savPath) {
	std::string path(savPath);
	const size_t slash = path.find_last_of('/');
	const size_t dot = path.find_last_of('.');
	if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) path.erase(dot);
	return path + ".walker";
}

static void walkerPrepare(configuration *conf) {
	conf->walkerCluster = 0;
    conf->walkerPayloadOffset = conf->walkerPayloadSize = 0;
	if (!conf->ndsPath || conf->isDSiWare) return;
    /* Both files must be on the filesystem that the selected cardengine owns. */
    if (conf->gameOnFlashcard != conf->saveOnFlashcard) return;
    if (conf->gameOnFlashcard && dsiFeatures() && !conf->b4dsMode) return;

	char key[41];
	palParkPathKey(conf->ndsPath, key);
	easysave::ini settings(WALKER_INI_PATH(conf));
	if (settings.fetch(key, "VIRTUAL_WALKER", "0") != "1") return;

	char code[4] = {0};
	FILE *rom = fopen(conf->ndsPath, "rb");
	if (rom) {
		fseek(rom, 0x0C, SEEK_SET);
		if (fread(code, 1, 3, rom) != 3) code[0] = 0;
		fclose(rom);
	}
	if (strncmp(code, "IPK", 3) && strncmp(code, "IPG", 3)) {
		walkerSkip(conf, "not HeartGold or SoulSilver", NULL);
		return;
	}
	if (!conf->savPath || !conf->savPath[0]) {
		walkerSkip(conf, "no save file", NULL);
		return;
	}

	const std::string path = walkerPathForSave(conf->savPath);
	struct stat st;
	/* Any outstanding checkout blocks HGSS, including an interrupted return. */
	if (stat((path + ".lease").c_str(), &st) == 0) {
		walkerSkip(conf, "walker is checked out to a phone; return or force-unlock it in the companion", path.c_str());
		return;
	}
	if (stat(path.c_str(), &st) != 0) {
		FILE *f = fopen(path.c_str(), "wb");
		if (!f) {
			walkerSkip(conf, "could not create the walker file", path.c_str());
			return;
		}
		static unsigned char blank[0x1000];
		memset(blank, 0xFF, sizeof(blank));
		bool ok = true;
		for (int i = 0; i < WALKER_FILE_SIZE / (int)sizeof(blank) && ok; i++)
			ok = fwrite(blank, 1, sizeof(blank), f) == sizeof(blank);
		fclose(f);
		if (!ok || stat(path.c_str(), &st) != 0) {
			walkerSkip(conf, "could not create the walker file", path.c_str());
			return;
		}
	}
	if (st.st_size != WALKER_FILE_SIZE) {
		walkerSkip(conf, "walker file is not 64 KiB", path.c_str());
		return;
	}
    if (!walkerPrelaunchSnapshot(path)) {
        walkerSkip(conf,"could not preserve the pre-launch recovery snapshot",path.c_str());
        return;
    }
    /* With the untouched state preserved above, bring the walker back in line
     * with what the save says is out on a stroll (see walker_reconcile.h). */
    {
        std::string note;
        if (walkerReconcile::run(path, conf->savPath, &note) >= walkerReconcile::ENDED_STROLL) {
            FILE *log = fopen(WALKER_REPAIR_PATH(conf), "w");
            if (log) {
                fprintf(log, "The virtual Pokewalker was repaired before launch.\n%s\nWALKER_PATH=%s\nSAVE=%s\n"
                        "The walker as it was before the repair is in transfers/backups (prelaunch snapshot).\n",
                        note.c_str(), path.c_str(), conf->savPath);
                fclose(log);
            }
        }
    }
    if (conf->gameOnFlashcard) {
        struct nitroFSStruct payload;
        if (nitroFSOpen(NULL, &payload, "nitro:/walker_ds.bin", 0, 0) < 0 || payload.start < 0 ||
            payload.end - payload.start != WALKER_DS_PAYLOAD_SIZE) {
            walkerSkip(conf, "DS walker payload is missing or incomplete", path.c_str());
            return;
        }
        conf->walkerPayloadOffset = payload.start;
        conf->walkerPayloadSize = payload.end - payload.start;
    }
	conf->walkerCluster = st.st_ino;
	remove(WALKER_ERROR_PATH(conf));
}
#endif
