# Research references

Sources used to check factual EEPROM layouts, sprite bitplanes and common-route mechanics:

- [Dmitry Grinberg: Pokewalker reverse engineering](https://dmitry.gr/?proj=28.+pokewalker&r=05.Projects).
- [lumirth/pw, CC0-1.0 firmware reconstruction](https://github.com/lumirth/pw).
- [Serialized records](https://github.com/lumirth/pw/blob/main/include/records.h).
- [Save counters](https://github.com/lumirth/pw/blob/main/include/save.h).
- [Radar](https://github.com/lumirth/pw/blob/main/src/application/pw_pokeradar.c).
- [Dowsing](https://github.com/lumirth/pw/blob/main/src/application/pw_dowsing.c).
- [Battles](https://github.com/lumirth/pw/blob/main/src/application/pw_battle.c).
- [Companion events](https://github.com/lumirth/pw/blob/main/src/application/pw_follower_events.c).
- [Companion prompts](https://github.com/lumirth/pw/blob/main/src/application/pw_follower_prompts.c).
- [Raster bitplanes](https://github.com/lumirth/pw/blob/main/include/raster_column.h).

Time stepping, sleep-session replay, PW3D metadata, free wandering encounters and automatic sleep-session reward acceptance are app additions. The original firmware is not executed; exact hardware compatibility is not claimed.

- [HGSS Pokewalker save functions](https://github.com/pret/pokeheartgold/blob/master/src/pokewalker.c): returned counter accumulation and course flags.
- [HGSS save counter layout](https://github.com/kwsch/PKHeX/blob/master/PKHeX.Core/Saves/SAV4HGSS.cs).

- [Factual English species-name mapping](https://github.com/kwsch/PKHeX/blob/master/PKHeX.Core/Resources/text/other/en/text_Species_en.txt), National Dex 1–493.
- [libctru rename implementation](https://github.com/devkitPro/libctru/blob/master/libctru/source/archive_dev.c): file replacement and FS error translation.

- [HGSS English item-name bank](https://github.com/pret/pokeheartgold/blob/master/files/msgdata/msg/msg_0222.gmm), factual item ID/name mapping with original Gen IV spellings.

- [libdvm write-back cache](https://github.com/devkitPro/libdvm/blob/master/source/dvm_cache.c) and [FAT driver](https://github.com/devkitPro/libdvm/blob/master/source/fat_driver.c): the NDS companion uses zero cache pages so verification reaches the underlying DLDI media interface.

- [libctru archive timestamp API](https://github.com/devkitPro/libctru/blob/master/libctru/source/archive_dev.c): `archive_getmtime()` supplies UNIX seconds independently of `stat()`.
