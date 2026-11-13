# Pokewalker 3DS — SD Transporter companion

A graphical companion for SD Transporter's virtual Pokewalker `.walker` EEPROM files. This is an original-style recreation with time-based walking, not an emulator running the original firmware. Wild encounters remain shiny-locked.

## Install

Copy the release ZIP's `SD/` contents to the root of your 3DS SD card. Run **3DS Pokewalker** in the Homebrew Launcher, or install the included `SD/cia/3DS-Pokewalker.cia` with your preferred CIA installer on a homebrew-enabled console. Both builds use the same `.walker` files. The CIA title ID is `000400000F7E1200`.

1. Start a virtual Pokewalker stroll in HGSS using SD Transporter.
2. The app searches `sdmc:/roms` by default. Press **X** to browse the SD card, **A** to open a folder, and **START** to scan that folder and its subfolders. Press **Y** in the picker to search `/roms` again.
3. Open this app, select a walker containing a Pokémon, and press A to start walking. Empty walkers are marked and cannot be opened.
4. Keep this app open to accumulate steps. Closing the lid counts as part of the open session; the elapsed sleep time is applied on wake. Press START to save and exit before returning your Pokémon in HGSS.

The 3DS defaults to its physical pedometer with time-based walking disabled. Settings lets you choose one source at a time, or pause both. The time-based rate defaults to one step every 10 seconds. Timer steps accrue only while the selected walker is open, including lid sleep; there is no closed-app timer catch-up. Pedometer steps are credited from the system counter during the open session, including sleep. Steps before opening that walker are excluded. Save and exit before returning to HGSS.

Settings → Phone sync starts local Wi-Fi transfer to the Android app using only the console IP address; no pairing code is required. Update both companions to this protocol. Android can save named console addresses and retry interrupted transfers with the same handoff session. Checkout locks the console copy until return. Attempting to open a checked-out walker offers Sync or Recover; recovery requires confirmation, restores the pre-checkout snapshot, discards phone progress, and revokes the old phone session. See the suite README for the complete workflow and `.nds`/Android builds.

The main display shows **Stroll steps**, which returns to zero when the Pokémon is returned. The lifetime counter is preserved and shown on the trainer card. Empty walkers earn neither steps nor watts.

## Features

- Two-screen graphical interface with larger text on both screens, wrapping instead of tiny autosizing, a clear selection highlight, forest background and next-watt progress bar. The original Pokewalker large portrait and animation are preserved, including the intentional crop present in some original portraits such as Ho-Oh. Both screens now enforce a readable minimum text size instead of shrinking labels.
- Recursive walker discovery and EEPROM validation show separate progress indicators. Choose the SD root in the folder picker to search the whole card.
- Walking and battle sprites read from the user's EEPROM. No retail artwork, ROMs, saves or firmware are bundled.
- Poké Radar costs 10 watts. Follow rustling patches, then attack, evade or catch. Species, levels, moves and step gates come from the active route.
- Dowsing costs 3 watts: six patches, two attempts and a near/far hint. Items use the active route's table and step gates.
- Three caught Pokémon and three item slots. Full packs require explicit replacement. Complete native Pokémon records are preserved for HGSS's return process.
- Occasional walking item finds and free wild visitors. A visitor waits behind the **Wild Pokémon!** home tile; it is not automatically caught. Events are also processed for sleep within the open session. Empty walkers are blocked.
- Persisted checkpoints and random state prevent duplicate awards after saving/reopening.
- Trainer card, inventory display and adjustable time-based rate.
- Exact-size and mirrored-record validation. Complete backup and replacement files are written and read back before renaming. External changes prevent silent overwriting.

Rate confirmations and routine notifications expire after five seconds. New notices replace the previous notice, and leaving a screen clears its notice. Game instructions, results and save-error messages remain separate. Settings reserve space for notifications below the options. The old one-second factory rate migrates to ten seconds once; other saved rates are preserved.

## Controls and saves

| Screen | Controls |
| --- | --- |
| Walker list | Up/Down selects; L/R pages; A or touch opens; X rescans |
| Home and games | D-pad selects (holding repeats); A or touch confirms; B goes back |
| Settings | Left/Right changes the rate |
| Anywhere | START saves the selected walker and exits |
| Save failure | D-pad and A choose Retry/Discard; Y retries; X opens a discard confirmation |

The `.walker.bak` beside a walker holds its previous saved image. `.walker.tmp` and `.walker.bak.tmp` are temporary writes; these extensions are not discovered as walkers. Keep the app open after a save error to retain unsaved changes. The full-size error panel shows the failing operation and errno; `sd:/3ds/pokewalker3ds/save-error.log` records the details when the SD card permits logging. Writes now flush through the filesystem before verification. The reported backup-replacement failure is addressed with a direct SD filesystem rename helper. It handles both the common and FS-specific “already exists” results instead of falling through to a directory rename that reports a misleading “not found.” Regression checks cover existing destinations, independent UTF-16 paths and propagated native errors. Hardware retesting is still required. Keep an independent save copy for first hardware testing.

## Scope

Normal-route Radar, Dowsing, battles, inventory and companion behavior use documented original mechanics. This app has its own UI, RNG, animations and time simulation. Walking visitors, occasional finds and automatic acceptance of sleeping-session social rewards are companion-app additions.

This does not emulate the H8 CPU, accelerometer, physical IR or peer-to-peer walker play. Special-event gift course transfers are not implemented: their minigames show an explanatory message, while walking remains available. Trainer cards and battle headings display English Pokémon species names instead of species numbers. Item slots still use item numbers. Some international trainer/route characters display as question marks.

Steps cap at 9,999,999, watts at 9,999 and daily steps at 99,999. Sleep steps cover the entire open-session gap. Random reward replay is limited to 8,192 hourly opportunities per update (about eleven months); capacity limits also apply, without silently replacing packed rewards.

The release was compiled for 3DS, checked using synthetic EEPROM fixtures, and visually reviewed through a host rendering of the same UI geometry. Console sleep behavior, the revised save path, CIA installation, and complete HGSS course unlocking still require hardware testing. A synthetic fixture carrying 200 watts and 5,000 lifetime steps was passed through the production virtual walker protocol; both counters reached the HGSS return packets correctly.

### Route unlocking

Available watts are returned through the normal HGSS Pokewalker exchange. HGSS stores the accumulated returned watts and controls course unlocks; this companion does not edit route flags or bypass game progression requirements. Watts spent on Radar/Dowsing are no longer available to return.

## Build: Windows

Install the official [devkitPro toolchain](https://devkitpro.org/wiki/Getting_Started), selecting 3DS development. In its MSYS2 shell:

```sh
pacman -S 3ds-dev
cd /c/path/to/pokewalker-3ds
make -j4
```

Dependencies: GNU Make, devkitARM GCC, libctru, Citro2D, Citro3D and devkitPro's `3dsxtool`/`smdhtool`. The standard `3ds-dev` group supplies these. Use the provided shell so `DEVKITPRO` and `DEVKITARM` are configured.

## Build: Linux

Follow the official [devkitPro pacman setup](https://devkitpro.org/wiki/devkitPro_pacman), then install `3ds-dev` using its package manager. On systems providing the wrapper:

```sh
sudo dkp-pacman -S 3ds-dev
export DEVKITPRO=/opt/devkitpro
export DEVKITARM=$DEVKITPRO/devkitARM
export PATH=$DEVKITARM/bin:$DEVKITPRO/tools/bin:$PATH
cd /path/to/pokewalker-3ds
make -j4
```

Outputs: `pokewalker3ds.3dsx`, `pokewalker3ds.smdh`, and a debugging ELF. Use `make clean` for a clean build.

For the CIA build, install the open-source [Project_CTR makerom](https://github.com/3DSGuy/Project_CTR) and put it on PATH, then run `make cia`. Alternatively set `MAKEROM=/path/to/makerom`. `app.rsf` specifies the app identity and direct SD read/write permissions. The generated original-art `banner.bin` is included. To regenerate it, install [bannertool](https://github.com/Steveice10/bannertool) and run `make banner`. No external firmware, retail banner or Nintendo SDK is needed.

## Credits and license

Part of **SD Transporter**. Thanks to [TWiLight Menu++](https://github.com/DS-Homebrew/TWiLightMenu) and [nds-bootstrap](https://github.com/DS-Homebrew/nds-bootstrap) for the projects underlying SD Transporter, and devkitPro, libctru, Citro2D and Citro3D for the 3DS development libraries.

Original app code is GPL-3.0-or-later; see `LICENSE`. Serialized layouts and mechanics were checked against Dmitry Grinberg's research and the CC0 `lumirth/pw` reconstruction; see `REFERENCES.md`. Their research source trees and the retail firmware are not bundled. This is unofficial homebrew.

## Registered empty walkers and confirmed returns

Registered empty walkers keep earning steps and watts, but since v1.6.1 no Pokémon joins them: HGSS builds a joined partner as an event Pokémon from walker records the companions never had, so those partners were illegal. Send a partner from HGSS. Blank/unregistered or malformed files remain blocked. Console/phone returns still accept a partner that joined under an earlier version, so a phone-held walker is never stranded.

Update the phone and console companions together. The phone requires either `X-Walker-Stored-CRC32` or a structured session-bound `PWRETURN1` receipt matching its returned EEPROM before clearing active ownership; until confirmation succeeds the phone keeps a frozen pending-return copy. Verified immutable recovery exports are retained. DS Lite Wi-Fi scanning cannot accept a handoff, and active/phone-owned walkers block Wi-Fi reconfiguration.

## Recovery and save integrity (1.3 test)

Confirmed handoffs remove the sender's active Pokémon. Unconfirmed copies remain locked; history snapshots never load automatically. Recovery snapshots are limited to **10 per walker**, oldest first after a newly written snapshot is verified. Console files are grouped by original walker path; phones group by the stable device/trainer identity rather than the handoff token. Ownership receipts and in-flight `.phone` snapshots are not part of the history cap.

3DS/NDS Settings **X** opens the recovery manager. The file list **SELECT** finds histories even when the active file is missing. **Y** changes walker history, **L/R** changes copy, **A** deletes a snapshot after confirmation and **X** deletes the active file after preserving a verified snapshot. **SELECT** inside Recovery offers explicit missing-file recovery from verified `.tmp`, `.replace-old`, `.bak` or `.bak.tmp` candidates. Choose with **L/R**, inspect trainer/pack details and press **A** to restore; candidates are kept, and no phone lock is bypassed. Mobile Settings offers export/delete histories and confirmed active deletion.

Console saves reread the final filename after replacement. FAT/3DS replacement keeps an old-file rollback instead of deleting the target first. Catches and item finds get their own verified recovery snapshot; success is shown only after the active save succeeds. Mobile reward saves follow the same verification rule. Trainer previews show the verified HGSS trainer name and public five-digit Trainer ID where available.

Fault checks cover failed and silently dropped writes, rollback failures, explicit backup restoration and journal replay. Use at your own risk and keep independent backups. iPhone source changes have host C and Swift syntax checks only, not an Apple SDK build.

Companion 1.4 adds named phone profiles with all-active walking and trainer-name/public-ID binding. Console Recovery uses START to restore a selected snapshot, and SELECT for interrupted-save candidates. Final-file existence/content is rechecked before return acknowledgement. See the root howtouse.txt for controls, recovery implications and current build limitations.
