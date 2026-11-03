/*
	Poke Transfer / virtual GBA slot selection for the custom nds-bootstrap.

	The custom bootstrap reads two small INI files that TWiLight never rewrites:

	  sd:/_nds/nds-bootstrap/poketransfer.ini   [POKETRANSFER] NDS_PATH, SAV_PATH
	      The Gen 4 game whose ROM and save the Download Play Poke Transfer
	      client reads instead of a Slot-1 cartridge (virtual Slot-1).

	  sd:/_nds/nds-bootstrap/slot2.ini          [SLOT2] GBA_ROM_PATH, GBA_SAV_PATH
	      The GBA game every DS game sees in its GBA slot (virtual Slot-2),
	      e.g. for Pal Park or dual-slot encounters.

	The per-game settings menus (Y on a ROM) use these helpers to set or clear
	those entries for the highlighted ROM.
*/
#ifndef POKE_SLOTS_H
#define POKE_SLOTS_H

#include <string>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include "common/inifile.h"
#include "common/palpark_retail.h"

namespace pokeSlots {

static const char* const kFolder      = "sd:/_nds/nds-bootstrap";
static const char* const kTransferIni = "sd:/_nds/nds-bootstrap/poketransfer.ini";
static const char* const kSlot2Ini    = "sd:/_nds/nds-bootstrap/slot2.ini";
static const char* const kMigrationIni = "sd:/_nds/nds-bootstrap/migration.ini";

inline bool migrationBypassEnabled(const std::string& path) {
	CIniFile file(kMigrationIni);
	char key[41];
	palParkPathKey(path.c_str(), key);
	return file.GetInt(key, "BYPASS_LIMITS", 0) == 1;
}

inline std::string migrationLabel(const std::string& path) {
	return std::string("Bypass migration limits: ") + (migrationBypassEnabled(path) ? "On" : "Off");
}

inline std::string toggleMigrationBypass(const std::string& path) {
	CIniFile file(kMigrationIni);
	char key[41];
	palParkPathKey(path.c_str(), key);
	file.SetInt(key, "BYPASS_LIMITS", migrationBypassEnabled(path) ? 0 : 1);
	file.SetString(key, "ROM_PATH", path);
	mkdir(kFolder, 0777);
	return file.SaveIniFile(kMigrationIni) ? migrationLabel(path) : "Could not write settings";
}

// Virtual Pokewalker for HeartGold/SoulSilver (any language): nds-bootstrap
// answers the game's IR link from "<save>.walker" next to the save.
inline const char* walkerIni(const std::string& path) {
    return path.compare(0, 5, "fat:/") == 0 ? "fat:/_nds/nds-bootstrap/pokewalker.ini" : "sd:/_nds/nds-bootstrap/pokewalker.ini";
}
inline bool walkerDeviceSupported(bool secondary, bool dsi, bool b4ds) {
    return !secondary || !dsi || b4ds;
}

inline bool isWalkerGame(const char* tid) {
	return tid && (!strncmp(tid, "IPK", 3) || !strncmp(tid, "IPG", 3));
}

inline bool virtualWalkerEnabled(const std::string& path) {
	CIniFile file(walkerIni(path));
	char key[41];
	palParkPathKey(path.c_str(), key);
	return file.GetInt(key, "VIRTUAL_WALKER", 0) == 1;
}

inline std::string walkerLabel(const std::string& path) {
	return std::string("Virtual Pokewalker: ") + (virtualWalkerEnabled(path) ? "On" : "Off");
}

inline std::string toggleVirtualWalker(const std::string& path) {
	CIniFile file(walkerIni(path));
	char key[41];
	palParkPathKey(path.c_str(), key);
	file.SetInt(key, "VIRTUAL_WALKER", virtualWalkerEnabled(path) ? 0 : 1);
	file.SetString(key, "ROM_PATH", path);
	mkdir(path.compare(0, 5, "fat:/") == 0 ? "fat:/_nds/nds-bootstrap" : kFolder, 0777);
	return file.SaveIniFile(walkerIni(path)) ? walkerLabel(path) : "Could not write settings";
}

// Diamond, Pearl, Platinum, HeartGold and SoulSilver: EN US/EU, FR, DE, IT, ES, JA, KO.
inline bool isTransferSource(const char* tid) {
	if (!tid || !tid[0]) return false;
	const bool game = !strncmp(tid, "ADA", 3) || !strncmp(tid, "APA", 3) || !strncmp(tid, "CPU", 3)
				   || !strncmp(tid, "IPK", 3) || !strncmp(tid, "IPG", 3);
	return game && strchr("EPFDISJK", tid[3]) != NULL && tid[3] != 0;
}

inline bool fileSize(const std::string& path, off_t* size) {
	struct stat st;
	if (stat(path.c_str(), &st) != 0) return false;
	*size = st.st_size;
	return true;
}

inline bool sameFile(const std::string& a, const std::string& b) {
	return !a.empty() && strcasecmp(a.c_str(), b.c_str()) == 0;
}

inline std::string withExtension(const std::string& filename, const char* extension) {
	const size_t dot = filename.find_last_of('.');
	return (dot == std::string::npos ? filename : filename.substr(0, dot)) + extension;
}

inline std::string readIni(const char* ini, const char* section, const char* key) {
	CIniFile file(ini);
	return file.GetString(section, key, "");
}

inline bool writeIni(const char* ini, const char* section, const char* key1, const std::string& value1,
					 const char* key2, const std::string& value2) {
	mkdir(kFolder, 0777);
	CIniFile file(ini);
	file.SetString(section, key1, value1);
	file.SetString(section, key2, value2);
	return file.SaveIniFile(ini);
}

/* ---- Virtual Slot-1: Poke Transfer source --------------------------------- */

inline bool isTransferSourceSet(const std::string& romPath) {
	return sameFile(readIni(kTransferIni, "POKETRANSFER", "NDS_PATH"), romPath);
}

inline std::string transferLabel(const std::string& romPath) {
	return isTransferSourceSet(romPath) ? "Remove as virtual Slot-1 cartridge" : "Set as virtual Slot-1 cartridge";
}

// savePath is where TWiLight keeps this game's save (same rules as launching it).
inline std::string toggleTransferSource(const std::string& romPath, const std::string& savePath) {
	if (strncmp(romPath.c_str(), "sd:/", 4) != 0) return "SD card only";
	if (isTransferSourceSet(romPath)) {
		return writeIni(kTransferIni, "POKETRANSFER", "NDS_PATH", "", "SAV_PATH", "")
			? "Virtual Slot-1 cartridge removed" : "Could not write settings";
	}
	off_t size = 0;
	if (!fileSize(savePath, &size)) return "Save not found: play once first";
	if (size != 0x80000) return "Save must be 512 KiB";
	return writeIni(kTransferIni, "POKETRANSFER", "NDS_PATH", romPath, "SAV_PATH", savePath)
		? "Virtual Slot-1 cartridge set" : "Could not write settings";
}

/* ---- Virtual Slot-2: GBA cartridge ---------------------------------------- */

inline bool isSlot2Set(const std::string& romPath) {
	return sameFile(readIni(kSlot2Ini, "SLOT2", "GBA_ROM_PATH"), romPath);
}

inline std::string slot2Label(const std::string& romPath) {
	return isSlot2Set(romPath) ? "Remove as virtual Slot-2 cartridge" : "Set as virtual Slot-2 cartridge";
}

inline bool validGbaSaveSize(off_t size) {
	return size == 0x8000 || size == 0x10000 || size == 0x20000 || size == 0x20010;
}

// The GBA save next to the ROM, in its saves/ or save/ folder, or in TWiLight's
// own saves folder; the folder chosen in TWiLight's settings is tried first.
inline std::string findGbaSave(const std::string& romFolderNoSlash, const std::string& filename, bool twlmFolderFirst, bool gamesFolderFirst) {
	const std::string name = withExtension(filename, ".sav");
	const std::string twlm = std::string("sd:/_nds/TWiLightMenu/saves/") + name;
	const std::string candidates[6] = {
		twlmFolderFirst ? twlm : gamesFolderFirst ? romFolderNoSlash + "/" + name : romFolderNoSlash + "/saves/" + name,
		romFolderNoSlash + "/saves/" + name,
		romFolderNoSlash + "/save/" + name,
		romFolderNoSlash + "/" + name,
		twlm,
		romFolderNoSlash + "/saves/" + withExtension(filename, ".gba.sav"),
	};
	for (const std::string& path : candidates) {
		off_t size = 0;
		if (fileSize(path, &size) && validGbaSaveSize(size)) return path;
	}
	return "";
}

inline std::string toggleSlot2(const std::string& romPath, const std::string& savePath) {
	if (strncmp(romPath.c_str(), "sd:/", 4) != 0) return "SD card only";
	if (isSlot2Set(romPath)) {
		return writeIni(kSlot2Ini, "SLOT2", "GBA_ROM_PATH", "", "GBA_SAV_PATH", "")
			? "Virtual Slot-2 cartridge removed" : "Could not write settings";
	}
	if (savePath.empty()) return "GBA save (32-128 KiB) not found";
	return writeIni(kSlot2Ini, "SLOT2", "GBA_ROM_PATH", romPath, "GBA_SAV_PATH", savePath)
		? "Virtual Slot-2 cartridge set" : "Could not write settings";
}

} // namespace pokeSlots

#endif
