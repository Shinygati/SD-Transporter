# SD Transporter

SD Transporter adds virtual cartridge support to TWiLight Menu++ and nds-bootstrap, allowing compatible Pokémon games to migrate or transfer Pokémon using ROMs and saves stored on the console SD card. It also provides a virtual Pokéwalker for HeartGold and SoulSilver, with companion applications for 3DS, DS/DSi and Android.

This is an independent homebrew project built on upstream open-source software. Game ROMs, saves, console firmware and retail Download Play applications are not distributed with the project.

## Educational purpose, AI assistance and releases

SD Transporter is an educational homebrew project developed with assistance from **OpenAI Codex** and **Anthropic Claude**. It is intended for learning, experimentation and studying virtual cartridge interfaces and Pokewalker communication.

**No official releases or precompiled binaries will be published.** Users may compile the project from source using the instructions in this repository, subject to the applicable project and upstream licenses.

AI assistance does not replace the authorship, credits or license requirements of the upstream projects and other contributors. Their notices and source attribution are retained in [CREDITS.md](CREDITS.md), the component licenses and individual source files.

## Features

### Virtual cartridges

- **Virtual Slot-2:** select a GBA ROM for Gen III → IV migration through the game's normal Pal Park flow.
- **Virtual Slot-1:** select a compatible Gen IV DS ROM for Gen IV → V Poké Transfer through the received Download Play application.
- Set or remove a virtual cartridge from the per-game **Y menu** in the DSi-based, R4 and Acekard themes.
- ROM/save access and save-write handling through the virtual cartridge interface; the games retain their Pokémon conversion logic and transfer minigame.
- Source-save write bounds, read-back verification and backups under `saves/transfers/backups/`.
- Optional, per-game migration-limit bypass for the supported Diamond, Pearl and Platinum revisions. The option defaults to **Off**.

English Black, White, Black 2 and White 2 Download Play client profiles are included. A supported Gen V client is identified before its cartridge access is redirected. The profile-specific code patches the client.

### Virtual Pokéwalker

Enable **Virtual Pokewalker** in the Y menu for HeartGold or SoulSilver, then use the game's normal Pokéwalker menu. Each save uses a separate 64 KiB `.walker` EEPROM file alongside the save. On DS Phat/Lite, boot the flashcart kernel, launch TWiLight Menu++, and enable the option over the HGSS ROM. An enabled walker routes that game through the modified Nightly bootstrap, overriding kernel/Pico selection for that launch.

The companions provide:

- Steps, watts, current-stroll and lifetime counters, trainer information and the walking Pokémon's species name.
- Poké Radar, attack/evade/catch battles, Dowsing, Pokémon/item storage and explicit replacement choices when the pack is full.
- Walking events, including item discoveries and wild visitors.
- The original large portrait and animation stored in the walker file, including original portrait crops. No retail sprite collection is bundled.
- `/roms`-only walker discovery on the console, readable menus, D-pad navigation and graphical progress indicators.
- Rejection of empty walkers; a Pokémon must be present before walking or checkout to Android.

These applications recreate supported normal-route activities. They do not run the original Pokéwalker firmware or claim exact hardware emulation. Encounters are shiny-locked. Special-event gift courses, device-to-device peer play and RAM-upload event commands are not implemented as a complete original feature set.

### Step sources

| Platform | Default | Available settings |
| --- | --- | --- |
| 3DS | System pedometer on; timer off | System pedometer, time-based counter, or pause |
| Android | Phone step sensor on; timer off | Phone step sensor, time-based counter, or pause |
| DS/DSi | Time-based counter on | Time-based counter or pause |

Only one source is active at a time. The timer defaults to **one step every 10 seconds**. Missing physical sensors do not automatically enable timer-based steps.

On 3DS and DS/DSi, time-based walking counts while the selected walker is open, including lid sleep; elapsed sleep time is applied on waking. Closing the companion excludes that closed interval. The 3DS pedometer uses an opening baseline and includes system-recorded steps during the open session, including sleep; earlier steps are excluded. Android tracking runs through its foreground service, with an ongoing notification, including when its screen is off. Android's optional timer excludes intervals while that service is stopped.

Current-stroll steps reset when HGSS returns the Pokémon; lifetime steps remain available on the trainer card. Available watts and pack contents return through HGSS's normal exchange. HGSS controls accumulated returned watts and course unlocks; the companions do not directly change course-unlock flags in the game save.

### Local Wi-Fi handoff

- Sync using the console's **IP address**, with no pairing-code entry.
- Save named consoles on Android, select an existing address, or remove a saved entry.
- Check out the walker to Android and lock console/HGSS loading until it is returned.
- Verify response length, checksum, handoff session, walker identity and stroll information.
- Retry interrupted transfers automatically using the same saved session, including a return whose acknowledgement was lost.
- Recover on the console if the phone is unavailable. Confirmation restores the pre-checkout snapshot and revokes the old phone session; phone-side progress is discarded.

The lock remains after an interrupted download because the console cannot assume that the phone received nothing. Retry **Receive walker from console** on the same phone, or use **Recover** on the console to cancel the handoff. An old revoked phone session cannot upload its progress; a fresh checkout is required.

## Requirements and compatibility

| Component | Requirements and limits |
| --- | --- |
| Virtual cartridges / HGSS integration | Matching modified TWiLight Menu++ and nds-bootstrap builds, compatible games, and console SD access on a homebrew-capable DSi or 3DS/2DS. GBA/Gen V virtual-cartridge paths still require console SD. The virtual Pokéwalker additionally has an experimental DS Phat/Lite flashcart path: HGSS, its save and its walker must all be on the flashcart microSD. |
| Gen IV → V transfer | Two consoles and the supported received Download Play client setup. This does not replace the wireless host with a single-console transfer tool. |
| GBA source | Supported 32, 64 or 128 KiB saves; a supported 16-byte RTC trailer is preserved. Header/save access is emulated, not every GBA cartridge feature. Reads beyond the 512-byte ROM header return open-bus data; EEPROM saves and arbitrary Slot-2 DMA are not implemented. |
| Gen IV source | Western Diamond, Pearl, Platinum, HeartGold and SoulSilver headers with 512 KiB saves: English US/EU/Australian, French, German, Italian and Spanish releases. Japanese/Korean source games are not accepted by the current virtual Slot-1 source policy. |
| Gen V client | Included profiles target English Black/White/Black 2/White 2 received clients. Modified or unknown clients are refused. |
| Standalone NDS companion | DS/DSi-compatible homebrew launch with working DLDI/SD access. Native 256x192 UI with cached redraws. DS Lite adds in-app open/WEP Wi-Fi setup; other models use saved Nintendo WFC settings. Access-point compatibility depends on hardware and launch mode. |
| Android companion | Android 8.0 or newer. Physical walking requires a `TYPE_STEP_COUNTER` sensor and physical-activity permission where required. ARM64, ARMv7 and x86-64 builds are supported. |

ROM hacks retaining compatible headers and native save formats may work through the virtual cartridge interface. Changes to game code, save layouts or transfer assumptions can break compatibility. Support for every game revision, localization or hardware combination is not implied.

The migration bypass currently targets USA Diamond/Pearl Rev 5 and USA Platinum Rev 0/1. Header/revision checks and a unique migration-helper instruction signature guard the patch; these are **not full-ROM cryptographic verification**. Normal eligibility rules, a six-Pokémon batch and an empty Pal Park still apply. Retail HeartGold/SoulSilver already removed the daily/clock restrictions, so they do not receive this bypass patch.

## Installation

Start with an existing working TWiLight Menu++ installation. Compile the required SD Transporter components from source using [BUILDING-WINDOWS.md](BUILDING-WINDOWS.md) and the component build instructions. The following table describes where to install your compiled outputs.

| Build output | Destination / action |
| --- | --- |
| `dsimenu.srldr`, `r4menu.srldr`, `akmenu.srldr` | Copy to `sd:/_nds/TWiLightMenu/` |
| `nds-bootstrap-nightly.nds` and `nightly-bootstrap.ver` | Copy both to `/_nds/` on the console SD or flashcart microSD; select **Nightly** for this build |
| `pokewalker3ds.3dsx` and its `.smdh` | Copy to `sd:/3ds/pokewalker3ds/` for the Homebrew Launcher |
| `3DS-Pokewalker.cia` | Install on a homebrew-enabled 3DS using a CIA installer |
| `Pokewalker-NDS.nds` | Place under `/roms`, then launch through TWiLight Menu++ or a suitable flashcart |
| `Pokewalker-Android.apk` | Install on Android; update over the existing app to retain its handoff state |

The 3DS CIA title ID is `000400000F7E1200`.

### Migration and Poké Transfer

1. Hover over the source GBA or supported DS ROM and press **Y**.
2. Choose **Set as virtual Slot-2 cartridge** or **Set as virtual Slot-1 cartridge**. The entry changes to **Remove as…** for the selected cartridge.
3. Launch the destination Gen IV game for GBA migration, or use the Gen V host and received Download Play client for Poké Transfer.
4. Complete the game's normal transfer flow.

Settings are stored under `sd:/_nds/nds-bootstrap/`: `slot2.ini`, `poketransfer.ini`, `migration.ini` and `pokewalker.ini`. A source save such as `saves/Pokemon Pearl.sav` is backed up under `saves/transfers/backups/` with a content-identifying `.poketransfer-XXXXXXXX.bak` suffix.

### Pokéwalker and Android sync

1. Enable the virtual Pokéwalker for HGSS and send a Pokémon on a stroll using the game's menu.
2. Close HGSS, open the console companion, and select the populated walker beneath `/roms`.
3. For phone use, connect both devices to the same local network and open **Settings → Phone sync** on the console.
4. Open **Console Wi-Fi sync** on Android, enter or select the console IP, and choose **Receive walker from console**. Keep the console sync screen open.
5. To return, reopen console Phone sync and choose **Return walker and unlock on console** on Android.
6. Close the console companion before opening HGSS to exchange the returned data.

Local sync uses TCP port **30820**. Guest-network client isolation can prevent devices from reaching each other. On DS Lite, use **Settings > Wi-Fi setup**, or press **Y** on the walker list to configure Wi-Fi before opening a walker. Select an open/WEP network; WEP uses a 10-, 26- or 32-digit hexadecimal key. Select a blank-name result to enter its hidden SSID, or press **Y** inside Wi-Fi setup to enter a name and run a directed search. The SSID keyboard preserves case, spaces and symbols, accepts 1–32 printable ASCII bytes, and uses **R** to switch keyboard pages, **X** to delete, **START** to submit and **B** to cancel. The **SP** key inserts a space. D-pad/A and touch entry are supported; X deletes a key digit and START connects. A successfully connected network is saved to `/data/pokewalker/wifi.bin` on the flashcart card and reused for phone sync. This profile contains the network key; keep it private. WPA/WPA2/WPA3 are not supported by this DS Lite setup. Other console models keep their existing Nintendo WFC settings path. The DS Lite setup does not write the console firmware.

Keep `.walker.phone`, `.walker.lease` and `.walker.returned` files beside the walker: they hold the recovery snapshot, checkout lock and retry/revocation receipt. A checked-out `.walker` is intentionally a zero-byte placeholder. Do not overwrite it with a backup to bypass checkout; use **Recover**. The supplied bootstrap also checks `.walker.lease` before opening or creating a walker.

## Source and building

| Folder | Purpose |
| --- | --- |
| `TWiLightMenu-27.25.1/` | Modified DSi-based, R4 and Acekard menus and shared upstream code/assets |
| `bootstrap/` | Modified nds-bootstrap retail source, GBA/Gen V/HGSS integration and phone-lock guard |
| `3ds/` | 3DS companion, native filesystem handling and console UI |
| `nds/` | Standalone NDS companion and framebuffer/input adapters |
| `android/` | Android app, foreground step service, saved consoles and Wi-Fi client |
| `shared/` | Portable EEPROM/gameplay core used by Android; keep it consistent with the console copies when changing shared logic |

**[Windows build instructions](BUILDING-WINDOWS.md)** cover every independently built component, including each menu theme. The legacy TWiLight Menu++/bootstrap SDK and current Calico-based NDS companion SDK must be kept separate. The shared folder is compiled by its consumers and has no standalone application target.

On Linux, use the corresponding devkitPro SDKs with GNU Make and the commands in the component documentation: [bootstrap](bootstrap/BUILD.md), [3DS](3ds/README.md), and `python3 nds/build.py`. Android uses its Gradle/CMake project on either platform. Linux builds have not been validated for this snapshot.

## Repository contents and validation

`.gitignore` excludes game dumps, saves, walker EEPROMs and sidecars, local firmware/BIOS/NAND dumps, compiled distributions, Android SDK configuration, signing keys and build caches. Required upstream graphics, fonts, compressed source assets and binary patch tables remain eligible for version control. Ignore rules apply to untracked files; they do not remove files already committed.

Portable core and lease checks, localhost HTTP tests, injected connection-reset/retry tests and host-JVM execution of the Android native bridge have passed during development. The applications and modified bootstrap compile, and APK signatures have been verified. The DS Lite port also checks the C/assembly launch-header layout and runs synthetic boot/EEPROM write-back tests in an ARM emulator. The user has confirmed HGSS virtual Pokewalker connectivity on their DS Lite/R4 setup with sdwalk4. The native NDS UI, DS Lite Wi-Fi configuration and DS socket acceptance/cleanup compile and pass simulated ARM checks; The user has also confirmed DS Lite-to-Android receive works with the socket fix. Other network/hardware combinations and the complete physical return path still require testing. These checks do not substitute for device testing: physical-sensor delivery, Android background restrictions and console-to-phone Wi-Fi require validation on the target hardware. The latest 3DS sync transport changes address a reported connection reset and remain subject to hardware confirmation.

For a sync failure, report the console/app build, phone model/Android version, transfer direction and displayed error. Console socket diagnostics are written to `sd:/3ds/pokewalker3ds/sync-error.log` on 3DS and `/data/pokewalker/sync-error.log` on NDS. Do not attach private ROMs or saves to public issues.

## Credits and license

SD Transporter builds on **[TWiLight Menu++](https://github.com/DS-Homebrew/TWiLightMenu)** and **[nds-bootstrap](https://github.com/DS-Homebrew/nds-bootstrap)**. Credit belongs to Rocket Robz, ahezard, shutterbug2000, Pk11 and the upstream projects' developers, designers, translators and other contributors.

The project also uses work from devkitPro and its library contributors, bundled third-party libraries, and Pokémon/Pokéwalker research projects. **[CREDITS.md](CREDITS.md)** records source provenance, additional authors, research references and the locations of retained notices. Existing file-level authorship and licenses remain authoritative.

See [LICENSE](LICENSE), the component license files and individual source notices. This independent project is not affiliated with or endorsed by Nintendo, Game Freak or The Pokémon Company.

### DS Lite test build

The custom DS-mode startup header previously lacked fields present in the shared C launch structure. The corrected loader reserves them and gates DSi-only parameter writes to the DSi path. Size optimization keeps the DS startup stack clear of code and static data. `nightly-bootstrap.ver` supplies TWiLight's separate version label; it does not determine whether the loader boots.

First launch HGSS with the walker **Off** to confirm Nightly booting, then enable the walker and use the normal HGSS menu. Press SELECT during its walker connection search. This port reserves 80 KiB using the detected SDK heap limit and preserves loader/flashcart buffers; unmatched signatures, mixed storage backends or insufficient memory leave the walker disconnected. EEPROM writes are coalesced and read back through the flashcart before a completion acknowledgement is returned. Phone ownership locks and recovery are retained.
