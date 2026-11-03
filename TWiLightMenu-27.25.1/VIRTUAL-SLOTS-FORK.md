# TWiLight Menu++ 27.25.1 — virtual slot fork

Press **Y** on a ROM to open its per-game settings. Two new entries appear there.
Highlight one and press **A**; press A again later to undo it.

| ROM | Entry | Writes |
|---|---|---|
| Western Diamond / Pearl / Platinum / HeartGold / SoulSilver (`.nds`, SD card) | **Set as virtual Slot-1 cartridge** | `sd:/_nds/nds-bootstrap/poketransfer.ini` |
| Any `.gba` ROM on the SD card | **Set as virtual Slot-2 cartridge** | `sd:/_nds/nds-bootstrap/slot2.ini` |

- **Poké Transfer source:** uses the same save TWiLight launches the game with. That follows your save-location setting and the per-game save number. The save must already exist (512 KiB).
- **GBA save lookup:** the folder from your save-location setting first, then `saves/`, `save/`, next to the ROM, and `sd:/_nds/TWiLightMenu/saves/`. The save must be 32, 64 or 128 KiB (+16 bytes RTC).

These entries need the matching custom nds-bootstrap (`nds-bootstrap-nightly.nds`) and are shown in the DSi/3DS/Saturn/HBL, R4 and Acekard themes. Their text is English only.

**Bypass migration limits: Off** is an additional per-game toggle for candidate USA Diamond/Pearl/Platinum headers. Opening Y does not hash the ROM. At launch, the latest bootstrap uses header/revision/checksum checks and requires one exact migration-helper signature before applying the optional RAM patch. This does not cryptographically prove the whole ROM is original. Supported header revisions are USA Diamond/Pearl Rev 5 and USA Platinum Rev 0/1. HeartGold/SoulSilver already allow repeated migrations in retail and receive no bypass patch.

The toggle is off by default. It skips migration's prior-console/clock/daily-history checks while retaining the six-Pokémon and occupied-Pal-Park checks. It requires an inserted virtual GBA cartridge and the matching bootstrap. Settings are in `sd:/_nds/nds-bootstrap/migration.ini`, keyed by a hash of the ROM path so names containing brackets remain safe. Renaming or moving the ROM resets the new path to Off.

To change the new entries' labels, edit `universal/include/common/pokeSlots.h` (`transferLabel`, `slot2Label`, `migrationLabel`). Layout and other menu options live in each theme's `arm9/source/perGameSettings.cpp`. Rebuild the corresponding theme after editing.

Changed files:
- `universal/include/common/pokeSlots.h` (new)
- `romsel_dsimenutheme/arm9/source/perGameSettings.cpp`
- `romsel_r4theme/arm9/source/perGameSettings.cpp`
- `romsel_aktheme/arm9/source/perGameSettings.cpp`
