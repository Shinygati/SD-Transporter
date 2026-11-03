# Credits and source provenance

SD Transporter modifies and combines existing open-source projects. The companion apps add shared EEPROM/gameplay handling, platform interfaces and local Wi-Fi handoff. This file identifies the upstream projects, source authors and research used; it does not replace the original copyright/license notices retained in the tree.

## Primary upstream projects

| Project / contributors | Contribution and source locations |
| --- | --- |
| [TWiLight Menu++](https://github.com/DS-Homebrew/TWiLightMenu), Rocket Robz and its contributors | Launcher UI, DSi-based/R4/Acekard themes, settings, loaders, assets, designs and translations. Modified source is in `TWiLightMenu-27.25.1/`; retain its [license](TWiLightMenu-27.25.1/LICENSE) and theme `COPYING` files. |
| [nds-bootstrap](https://github.com/DS-Homebrew/nds-bootstrap), ahezard, Rocket Robz, shutterbug2000, Pk11 and its contributors | Loader, card engines, runtime patching, filesystem support and the platform used by virtual cartridges and HGSS integration. Source is in `bootstrap/`; see its [license](bootstrap/LICENSE) and source notices. |
| [devkitPro](https://devkitpro.org/) and its tool/library contributors | devkitARM, binutils/startup/build rules, libnds, libfat, Maxmod, libctru, Citro2D, Citro3D, Calico, DSWifi and platform tools. These projects supply the compiler/runtime libraries and device APIs. Their licenses remain applicable independently of the SD Transporter license. |
| Android Open Source Project and Android SDK/NDK contributors | Android framework, sensor/foreground-service APIs, packaging tools and native toolchain used by the Android companion. |

The upstream projects include many additional contributors. Existing source headers, upstream histories and embedded credits retain their authorship; the named people above are not an exhaustive replacement for those records.

## Bundled source and historical contributions

The following names/projects appear in the supplied source notices or import comments. Paths identify representative files or subtrees; a contributor may be credited in additional files.

| Author / project | Source contribution |
| --- | --- |
| Michael “Chishm” Chisholm; Dave “WinterMute” Murphy; Michael Noland (“joat”); Jason Rogers (“dovoto”) | DS startup, ARM7, cartridge, DLDI and filesystem code inherited through libnds/libfat and the parent projects; see bootstrap ARM7/bootloader sources and `TWiLightMenu-27.25.1/universal/`. |
| Darkain | Original boot/cache-clear routines, subsequently modified by Chishm; see bootstrap bootloader assembly and universal bootloader sources. |
| fincs | Block codec and device-related support, and contributions to the devkitPro ecosystem; see `bootstrap/retail/common/include/blocks_codec.h` and universal ARM7 code. |
| Normmatt; Smiths; bliss | SD/MMC, cheat and menu code; see bootstrap `common` SD/MMC sources and the themes' `cheat.cpp` headers. |
| Acekard; yellow wood goblin; the author identified as “somebody” in retained headers | Menu/string/INI, flashcard and decompression code, inherited through the upstream projects. Preserve those original notices, including names not further identified in the source. |
| Viruseb; MightyMax; M3Adapter; SaTa | Flashcard/storage driver contributions or sources acknowledged by inherited driver comments. The M3 driver itself notes possible historical SaTa-derived work; that attribution uncertainty is retained rather than asserted as a new authorship claim. |
| Sven Peter | Lock-support code; see `TWiLightMenu-27.25.1/universal/arm9/include/lock.h`. |
| lifehackerhansol; coderkei; Rocket Robz | DSpico launcher code/modifications; see universal ARM9 `DSpicoLauncher` files. |
| Arisotura | ROM-list data/code; see `TWiLightMenu-27.25.1/universal/include/ROMList.h`. |
| profi200 | Color lookup-table reference credited in `universal/source/common/ColorLut.itcm.cpp`. |
| PAlib contributors | Code explicitly described as ported from PAlib in universal ARM9 support. |
| [GodMode9i](https://github.com/DS-Homebrew/GodMode9i) contributors | Imported `f_xy` support acknowledged in `bootstrap/retail/arm9/include/gm9i/` and `source/gm9i/`. |
| [LodePNG](https://lodev.org/lodepng/), Lode Vandevenne | PNG decoding in universal `lodepng` source and headers; the original notice is retained. |
| ADPCM-XQ, David Bryant | ADPCM conversion/tool code under the DSi-based theme's `arm9/source/tool/`; retain the notices in `adpcm-lib` and `adpcm-xq`. |
| minIni contributors | INI/std-stdio glue acknowledged in `bootstrap/retail/arm9/include/minGlue.h`. |
| libeasysave, Jonathan Archer | INI support under `bootstrap/retail/arm9/libeasysave/`; see its [MIT license](bootstrap/retail/arm9/libeasysave/license.md). |
| RonnChyran | Singleton helper used by the bootstrap and menu code; see their `singleton.h` notices. |
| CUE | LZSS and related compression/decompression tool code; see `bootstrap/lzss.c` and related source headers. |
| PolarSSL / Brainspark B.V.; Mbed TLS / ARM Limited and contributors | Cryptographic code in bootstrap `polarssl` and `mbedtls` directories. Preserve the licenses and notices in the individual imported files. |
| The #dsdev community and references acknowledged in NitroFS source | DS ROM layout and filesystem information used by inherited NitroFS code. |

Graphics, sound, themes, translations and fonts shipped through TWiLight Menu++/nds-bootstrap retain their upstream authorship and applicable notices. This project does not claim authorship of those assets. The unchanged `COPYING`, `LICENSE` and source-header records should accompany redistributions as applicable.

## Pokémon and Pokéwalker research

These projects informed format/mechanics research, compatibility checks and factual data. A research reference is distinguished from a bundled source import.

| Reference | Use |
| --- | --- |
| [Dmitry Grinberg's Pokéwalker reverse engineering](https://dmitry.gr/?proj=28.+pokewalker&r=05.Projects) | Pokéwalker behavior, protocol and storage research. |
| [lumirth/pw](https://github.com/lumirth/pw) and contributors | CC0-1.0 reconstruction used as a reference for serialized records, counters, Radar, Dowsing, battles, follower events and raster layouts. Its research tree and retail firmware are not bundled. |
| [pret/pokediamond](https://github.com/pret/pokediamond), [pret/pokeplatinum](https://github.com/pret/pokeplatinum) and [pret/pokeheartgold](https://github.com/pret/pokeheartgold) contributors | Reference information on retail migration behavior, revisions, HGSS Pokéwalker data and course handling. |
| [PKHeX](https://github.com/kwsch/PKHeX), kwsch and contributors | Reference for HGSS save counters and the factual English species-name mapping. |
| [libctru](https://github.com/devkitPro/libctru) contributors | Platform APIs and implementation references for filesystem replacement/error translation and socket behavior. |

See [3ds/REFERENCES.md](3ds/REFERENCES.md) for the more detailed Pokéwalker reference list and links to the examined records/mechanics.

The companions' time-based stepping, open-session sleep accounting, extra metadata, platform interfaces, named console storage and handoff/recovery behavior are SD Transporter additions. The original Pokéwalker firmware is not executed.

## License records

- [Project license](LICENSE).
- [3DS companion license](3ds/LICENSE).
- [nds-bootstrap license](bootstrap/LICENSE).
- [TWiLight Menu++ license](TWiLightMenu-27.25.1/LICENSE) and the three theme `COPYING` files.
- [libeasysave license](bootstrap/retail/arm9/libeasysave/license.md).
- Copyright/license notices embedded in imported source files and the licenses of linked SDK libraries.

This list documents provenance; it does not relicense third-party components or replace their original notices. Pokémon, Nintendo and related names/characters belong to their respective owners. SD Transporter is an independent homebrew project.

## Native NDS interface and DS Lite Wi-Fi setup

The NDS companion uses libnds's native console font and the devkitPro Calico/DSWifi WFC APIs. The DS Lite setup follows the documented scan/connect workflow in devkitPro's [access-point search example](https://github.com/devkitPro/nds-examples/blob/master/dswifi/ap_search/source/template.c), credited there to dovoto. The framebuffer UI, input handling and card-based profile persistence are project code; the example's console UI and assets are not bundled. See the [DSWifi source and license](https://github.com/devkitPro/dswifi) for the upstream network stack and [Calico](https://github.com/devkitPro/calico) for its wireless infrastructure.
