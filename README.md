# SD Transporter

Current version **1.6.2**.

For app setup, activities, Wi-Fi handoffs and recovery, read [howtouse.txt](howtouse.txt).

For TWiLight setup and help with missing Pokémon, saving, or phone sync, see the [Pokéwalker troubleshooting guide](https://github.com/Shinygati/SD-Transporter/wiki/Troubleshooting-%E2%80%90-Pokewalker).

SD Transporter adds virtual cartridge support to TWiLight Menu++ and nds-bootstrap, allowing compatible Pokémon games to migrate or transfer Pokémon using ROMs and saves stored on the console SD card. It also provides a virtual Pokéwalker for HeartGold and SoulSilver, with companion applications for 3DS, DS/DSi and Android, plus an iPhone source port.

This is an independent homebrew project built on upstream open-source software. Game ROMs, saves, console firmware and retail Download Play applications are not distributed with the project.

## Educational purpose and AI assistance

SD Transporter is an educational homebrew project developed with assistance from **OpenAI Codex** and **Anthropic Claude**. It is intended for learning, experimentation and studying virtual cartridge interfaces and Pokewalker communication.

SD Transporter has undergone extensive testing, but bugs and device-specific issues may remain. Use it at your own risk, and keep backups of your saves and walker files. Users may compile the project from source using the instructions in this repository, subject to the applicable project and upstream licenses.

AI assistance does not replace the authorship, credits or license requirements of the upstream projects and other contributors. Their notices and source attribution are retained in [CREDITS.md](CREDITS.md), the component licenses and individual source files.

## Features

### Virtual cartridges

- **Virtual Slot-2:** select a GBA ROM for Gen III → IV migration through the game's normal Pal Park flow.
- **Virtual Slot-1:** select a compatible Gen IV DS ROM for Gen IV → V Poké Transfer through the received Download Play application.
- Set or remove a virtual cartridge from the per-game **Y menu** in the DSi-based, R4 and Acekard themes.
- ROM/save access and save-write handling through the virtual cartridge interface; the games retain their Pokémon conversion logic and transfer minigame.
- Source-save write bounds, read-back verification and backups under `saves/transfers/backups/`.
- Optional, per-game migration-limit bypass for the supported Diamond, Pearl and Platinum revisions. The option defaults to **Off**.

Black, White, Black 2 and White 2 Download Play profiles cover English, French, German, Italian, Spanish, Japanese and Korean clients. The 28 verified layouts include both received-client variants for each game family/language. Each decompressed client is checked against its profile before patching; modified or unknown clients are refused. The Japanese Black/White constructor has its own verified register layout. Both native Gen IV language-check paths are handled, while the actual source ROM header and native save reader/writer remain in use.

### Virtual Pokéwalker

Enable **Virtual Pokewalker** in the Y menu for HeartGold or SoulSilver, then use the game's normal Pokéwalker menu. Each save uses a separate 64 KiB `.walker` EEPROM file alongside the save. On DS Phat/Lite, boot the flashcart kernel, launch TWiLight Menu++, and enable the option over the HGSS ROM. An enabled walker routes that game through the modified Nightly bootstrap, overriding kernel/Pico selection for that launch.

The companions provide:

- Steps, watts, current-stroll and lifetime counters, trainer information and the walking Pokémon's species name.
- Poké Radar, attack/evade/catch battles, Dowsing, Pokémon/item storage and explicit replacement choices when the pack is full.
- Walking events, including item discoveries and wild visitors.
- The original large portrait and animation stored in the walker file, including original portrait crops. No retail sprite collection is bundled. The phone shell uses the image supplied by the project owner; its LCD is replaced at runtime with current profile data.
- `/roms`-only walker discovery on the console, readable menus, D-pad navigation and graphical progress indicators.
- Registered empty walkers keep earning steps and watts, but no Pokémon joins them: HGSS treats a joined partner as an event Pokémon and builds it from walker records the companions never had, which made it illegal. Send a partner from HGSS. Blank/unregistered files and empty files without valid course data are refused.

The companions recreate Pokéwalker activities without executing retail firmware. Encounters are shiny-locked. On Android and iPhone, tap the Pokéwalker to open its original-style menu, three-button controls and uploaded sound scores. Progress is shared with the current profile. Display assets come from your own data; menu timing and piezo audio are recreated.

**Visits and friendly battles:** connect two profiles on the same phone for a daily visit, or use Bluetooth LE between nearby Android/iPhone devices. Visits keep both partners in their profiles and save their gifts. Friendly battles are available only between phones and cost **30 watts per player** after both accept. They use the retained four moves, PP, power, accuracy and priority, with simplified health and status rules; full game stats, type matchups and special move effects are not simulated. Winners receive an item with higher rare-item rates. Entry and reward receipts survive restarts.

**Optional expanded items:** off by default. Dowsing follows the setting for the current profile. Friendly-battle rewards expand only when **both phones** enable it. If either phone disables it, every battle reward comes from the current course's original item table, weighted toward its rarer entries. Different settings trigger a warning before acceptance or entry payment.

| Item group | Dowsing find | Friendly-battle win |
| --- | ---: | ---: |
| Master Ball | 0.01% | 0.5% |
| PP Max | 0.09% | 4.5% |
| PP Up | 0.20% | 10% |
| TM01–TM92, combined | 1.70% | 35% |
| Vitamins: HP Up, Protein, Iron, Carbos, Calcium, Zinc | 3% | 10% |
| Rare Candy | 0.5% | 5% |
| EV training: Macho Brace and six Power items | 0.5% | 5% |
| Evolution stones and held evolution items | 1% | 5% |
| Battle held items | 0.5% | 5% |
| Heart Scale | 0.5% | 5% |
| Current course item table | 92% | 15% |

Each item is equally likely within its group. Evolution rewards include the ten Gen IV stones, King's Rock, DeepSeaTooth, DeepSeaScale, Metal Coat, Dragon Scale, Up-Grade, Dubious Disc, Protector, Electirizer, Magmarizer, Reaper Cloth, Razor Claw and Razor Fang. The held-item group contains White Herb, Choice Band, Scope Lens, Leftovers, Shell Bell, Muscle Band, Wise Glasses, Expert Belt, Life Orb, Power Herb, Focus Sash, Choice Scarf and Choice Specs.

Original-course battle weights are 30%, 25%, 15%, 12%, 8%, 5%, 2%, 1%, 1% and 1% for course slots 1–10. Invalid entries are omitted and weights are normalized. Course data is never rewritten. Ordinary Dowsing with expanded items disabled keeps the game's original course selection rules.

**Special courses:** in the console companion's file list, press **Y**, choose the HGSS game and press **X**, then confirm with **A**. The companion validates the matching raw 512 KiB save, preserves a verified original under saves/transfers/backups/, enables that save language's supported special-course flags and updates the save checksum. Select an activated course in HGSS when sending a Pokémon. Ordinary watt-based course progression is unchanged. RAM-upload event commands and unsupported regional course entries are not enabled.

### Step sources

| Platform | Default | Available settings |
| --- | --- | --- |
| 3DS | System pedometer on; timer off | System pedometer, time-based counter, or pause |
| Android | Phone step sensor on; timer off | Phone step sensor, time-based counter, or pause |
| iPhone source port | Core Motion pedometer on; timer off | Physical step history, foreground timer, or pause |
| DS/DSi | Time-based counter on | Time-based counter or pause |

Each phone profile uses one step source at a time. The timer defaults to **one step every 10 seconds**. Missing physical sensors do not automatically enable timer-based steps.

On 3DS and DS/DSi, time-based walking counts while the selected walker is open, including lid sleep; elapsed sleep time is applied on waking. Closing the companion excludes that closed interval. The 3DS pedometer uses an opening baseline and includes system-recorded steps during the open session, including sleep; earlier steps are excluded. Android tracking runs through its foreground service, with an ongoing notification, including when its screen is off. Android's optional timer excludes intervals while that service is stopped.

The iPhone port queries Core Motion history from the current handoff baseline and credits it together with its saved journal; iOS supplies up to seven days of physical history. Its optional timer counts only while the app is active, excluding suspension and closed-app time. The iPhone app is supplied as source and requires an Apple SDK build. See [ios/README.md](ios/README.md).

Current-stroll steps reset when HGSS returns the Pokémon; lifetime steps remain available on the trainer card. Available watts and pack contents return through HGSS's normal exchange. HGSS controls accumulated returned watts and course unlocks; normal walking does not change course-unlock flags directly. The separate, confirmed special-course option described above edits only the supported special flags.

### Local Wi-Fi handoff

- Sync using the console's **IP address**, with no pairing-code entry.
- Save named consoles on Android and iPhone, select an existing address, or remove a saved entry.
- Check out the walker to a phone and lock console/HGSS loading until it is returned.
- Verify response length, checksum, handoff session, walker identity and stroll information.
- Retry interrupted transfers automatically using the same saved session, including a return whose acknowledgement was lost.
- Recover on the console if the phone is unavailable. Confirmation restores the pre-checkout snapshot and revokes the old phone session; Phone-only progress is not merged into the restored snapshot, and existing recovery copies are retained.

The console refuses a receive retry or return that would replace a different, newer walker. Durable per-session receipts preserve revocation across later handoffs. A repeated completed return preserves newer console progress and requires a valid final walker file before it can acknowledge success. Android and iPhone freeze activities during a pending return and retain the pending state across process restarts.

The lock remains after an interrupted download because the console cannot assume that the phone received nothing. Retry **Receive Pokémon from console** on the same phone, or use **Recover** on the console to cancel the handoff. An old revoked phone session cannot upload its progress; a fresh checkout is required.

## Requirements and compatibility

| Component | Requirements and limits |
| --- | --- |
| Virtual cartridges / HGSS integration | Matching modified TWiLight Menu++ and nds-bootstrap builds, compatible games, and console SD access on a homebrew-capable DSi or 3DS/2DS. GBA/Gen V virtual-cartridge paths still require console SD. The virtual Pokéwalker also supports DS Phat/Lite flashcarts: HGSS, its save and its walker must all be on the flashcart microSD. |
| Gen IV → V transfer | Two consoles and the supported received Download Play client setup. This does not replace the wireless host with a single-console transfer tool. |
| GBA source | Region-independent ROM-header and save emulation, including Japanese GBA releases; the destination game retains its own migration rules. Supported 32, 64 or 128 KiB saves; a supported 16-byte RTC trailer is preserved. Header/save access is emulated, not every GBA cartridge feature. Reads beyond the 512-byte ROM header return open-bus data; EEPROM saves and arbitrary Slot-2 DMA are not implemented. |
| Gen IV source | Diamond, Pearl, Platinum, HeartGold and SoulSilver with 512 KiB saves. The selector, adapter and client family classifier accept English US/EU/Australian, French, German, Italian, Spanish, Japanese and Korean game codes. The actual ROM header is preserved. Native save validation remains active; end-to-end Japanese/Korean transfers still need verification with real regional ROMs/saves. |
| Gen V client | Verified layouts for English, French, German, Italian, Spanish, Japanese and Korean Black/White/Black 2/White 2 received clients. Native CPU read/commit tests passed; regional radio stability and Japanese/Korean text round-trips still need hardware verification. Modified or unknown clients are refused. |
| Standalone NDS companion | DS/DSi-compatible homebrew launch with working DLDI/SD access. Native 256x192 UI with cached redraws. DS Lite adds in-app open/WEP Wi-Fi setup; other models use saved Nintendo WFC settings. Access-point compatibility depends on hardware and launch mode. |
| iPhone companion | Initial iOS 16+ SwiftUI source port; real Xcode build/device validation pending. Build on macOS, then sign/install an IPA from Windows using AltStore. Free-account signing normally needs weekly refresh. |
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

### Console SD and DSpico packages

- **3DS or DSi cfw only** contains the console SD files. On a modded DSi, start TWiLight Menu++ from the console SD using `BOOT.NDS` in Unlaunch.
- **DSi Flashcart / 3DS or DSi SD** goes on the console SD. **DSi Flashcart / Pico SD** goes on the DSpico microSD. Launch `BOOT_ALT.NDS` in DSi mode and select the console SD in TWiLight.

### Physical HeartGold and SoulSilver

The [standalone cartridge launcher](TWiLightMenu-27.25.1/slot1launch/README-CARTRIDGE.md) runs in DSi mode on DSi and 3DS and stores the virtual walker on the console SD. Insert the retail cartridge before boot or use the launcher’s insert prompt if you started through DSpico with console SD access. The physical cartridge send-and-return cycle still needs hardware testing.

### Migration and Poké Transfer

1. Hover over the source GBA or supported DS ROM and press **Y**.
2. Choose **Set as virtual Slot-2 cartridge** or **Set as virtual Slot-1 cartridge**. The entry changes to **Remove as…** for the selected cartridge.
3. Launch the destination Gen IV game for GBA migration, or use the Gen V host and received Download Play client for Poké Transfer.
4. Complete the game's normal transfer flow.

Settings are stored under `sd:/_nds/nds-bootstrap/`: `slot2.ini`, `poketransfer.ini`, `migration.ini` and `pokewalker.ini`. A source save such as `saves/Pokemon Pearl.sav` is backed up under `saves/transfers/backups/` with a content-identifying `.poketransfer-XXXXXXXX.bak` suffix.

### Pokéwalker and phone sync

1. Enable the virtual Pokéwalker for HGSS and send a Pokémon on a stroll using the game's menu.
2. Close HGSS, open the console companion, and select a valid registered walker beneath `/roms`. It must have a partner sent from HGSS for Radar, Dowsing and catches; an empty walker only earns steps and watts.
3. For phone use, connect both devices to the same local network and open **Settings → Phone sync** on the console.
4. Open **Console Wi-Fi sync** on the phone, enter or select the console IP, and choose **Receive Pokémon from console**. Keep the console sync screen open.
5. To return, reopen console Phone sync and choose **Return Pokémon** on the phone.
6. Press **START → Save and exit** before reopening HGSS. Walking progress and rewards stay in memory until you save or complete a phone handoff.

Local sync uses TCP port **30820** over unencrypted HTTP. Trainer/game information and step totals travel directly to the entered console IP; session tokens and CRC32 do not encrypt the connection. The phone discloses this before first use. No developer-operated server receives sync data. Guest-network client isolation can prevent devices from reaching each other. On DS Lite, use **Settings > Wi-Fi setup**, or press **Y** on the walker list to configure Wi-Fi before opening a walker. Select an open/WEP network; WEP uses a 10-, 26- or 32-digit hexadecimal key. Select a blank-name result to enter its hidden SSID, or press **Y** inside Wi-Fi setup to enter a name and run a directed search. The SSID keyboard preserves case, spaces and symbols, accepts 1–32 printable ASCII bytes, and uses **R** to switch keyboard pages, **X** to delete, **START** to submit and **B** to cancel. The **SP** key inserts a space. D-pad/A and touch entry are supported; X deletes a key digit and START connects. A successfully connected network is saved to `/data/pokewalker/wifi.bin` on the flashcart card and reused for phone sync. This profile contains the network key; keep it private. WPA/WPA2/WPA3 are not supported by this DS Lite setup. Other console models keep their existing Nintendo WFC settings path. The DS Lite setup does not write the console firmware.

Keep `.walker.phone`, `.walker.lease` and `.walker.returned` files beside the walker: they hold the recovery snapshot, checkout lock and retry/revocation receipt. A checked-out `.walker` is intentionally a zero-byte placeholder. Do not overwrite it with a backup to bypass checkout; use **Recover**. The supplied bootstrap also checks `.walker.lease` before opening or creating a walker. Immutable checkout, return, conflict and recovery snapshots and per-token receipts are kept under adjacent `transfers/backups/`; do not delete them during an active handoff. Bootstrap also creates a verified pre-launch snapshot before connecting HGSS to an existing walker. Holding **SELECT + LEFT** during the HGSS connection search still resets the virtual walker to an unregistered state. These backups provide recovery points, not a guarantee against every power or storage failure. Phone recovery copies can be exported through Settings.

## Source and building

| Folder | Purpose |
| --- | --- |
| `TWiLightMenu-27.25.1/` | Modified DSi-based, R4 and Acekard menus and shared upstream code/assets |
| `bootstrap/` | Modified nds-bootstrap retail source, GBA/Gen V/HGSS integration and phone-lock guard |
| `3ds/` | 3DS companion, native filesystem handling and console UI |
| `nds/` | Standalone NDS companion and framebuffer/input adapters |
| `android/` | Android app, foreground step service, saved consoles and Wi-Fi client |
| `ios/` | Initial SwiftUI iPhone port, Core Motion tracking, shared C bridge and reproducible Xcode project |
| `shared/` | Portable EEPROM/gameplay core used by Android and iPhone; keep it consistent with the console copies when changing shared logic |

**[Windows build instructions](BUILDING-WINDOWS.md)** cover every independently built component, including each menu theme. The legacy TWiLight Menu++/bootstrap SDK and current Calico-based NDS companion SDK must be kept separate. The shared folder is compiled by its consumers and has no standalone application target.

On Linux, use the corresponding devkitPro SDKs with GNU Make and the commands in the component documentation: [bootstrap](bootstrap/BUILD.md), [3DS](3ds/README.md), and `python3 nds/build.py`. Android uses its Gradle/CMake project on either platform. iOS builds require macOS/Xcode; [iPhone build and Windows installation instructions](ios/README.md) and an optional manual CI template are included. Linux builds have not been validated for this snapshot.

## DS Lite support

Launch the flashcart kernel, then TWiLight Menu++. Keep HGSS, its save and its walker on the same flashcart microSD. Enable **Virtual Pokewalker** in the HGSS Y menu and launch with the matching modified Nightly bootstrap. Hold **SELECT** on the game's Pokéwalker connection screen. The standalone NDS companion opens the same walker under `/roms` and provides DS Lite Wi-Fi setup for compatible open/WEP networks.

## Backups, recovery and readable names

Companions display English in-game Pokémon and item names, including catch/find confirmations. Console walker previews and phone recovery entries show trainer names and public five-digit Trainer IDs when available. The 3DS recovery browser reads native SD timestamps for dates and oldest-first ordering; unavailable dates display **Unknown date**.

Recovery histories retain **10 snapshots per walker**, removing the oldest only after a new snapshot is verified. Active files, in-flight phone snapshots and ownership receipts are protected separately. Backups never become active profiles automatically. Settings offers confirmed deletion of individual snapshots or the active walker; active deletion preserves a verified recovery copy first.

On consoles, **Settings → X** or **file list → SELECT** opens Recovery. **Y** changes walker history and **L/R** selects a snapshot. **START** opens selected-backup restoration; **A** confirms it. **SELECT** inside Recovery opens missing-file recovery from adjacent `.tmp`/`.bak` candidates. Phone Settings provides export/delete recovery controls. See [howtouse.txt](howtouse.txt) for full controls and interrupted-transfer recovery.

## Mobile profiles


Android and the iPhone source app support several named walker profiles. The existing single walker remains in its original storage location as Walker 1. Each profile has a separate ownership journal, trainer binding, progress, pack, settings and recovery folder. Use **Walker profiles** to add, select or rename one. All active profiles earn steps; viewing another profile does not pause the others. Paused, revoked, pending-return and currently syncing profiles do not earn steps. Android's notification provides **Pause all**.

After its first receive, a profile accepts only the same trainer name and public Trainer ID. Matching consoles check these before checkout, and phones verify the received data too. This intentionally treats saves with identical trainer names and IDs as the same trainer; existing session checks continue to protect returns. Update console and phone companions together. A returned profile retains its trainer binding. Deleting its active Pokémon clears that binding; recovery history remains available.

Console Recovery now supports **START → Restore selected backup → A Confirm**, including over an existing empty walker. The current file is preserved first, trainer mismatches are refused, and an outstanding phone lease is revoked. This is deliberate recovery: an old checkpoint can duplicate Pokémon already received in HGSS. **SELECT** in the recovery browser selects missing-file recovery from adjacent `.tmp`, `.replace-old`, `.bak` and `.bak.tmp` candidates; START also offers it when no history snapshot is selected.

The NDS companion mounts storage without libdvm's write-back cache so save verification reaches the underlying DLDI media interface. Both console companions recheck the final `.walker` after return cleanup. A receipt alone cannot confirm a missing, empty or invalid final file. Phone copies stay locked until a verified acknowledgement; recovery histories remain separate from active profiles.

Phone battles now show the original two-frame partner and opponent images supplied by the walker, with attack, evade and Poké Ball presentation. Motion timing is recreated; the original firmware is not executed. A catch with an available pack slot is saved before its animation plays. Native console battle frames are preserved.

Automated checks cover profile migration/isolation, all-active physical/timer credit, catalogue write failures, trainer mismatch refusal before checkout, final-file acknowledgement checks, explicit snapshot restoration and existing save/restart/ownership behavior. The iPhone app is provided as source; an Apple SDK build is required to produce an IPA.

## Repository contents and validation

`.gitignore` excludes game dumps, saves, walker EEPROMs and sidecars, local firmware/BIOS/NAND dumps, compiled distributions, signing keys and build caches. Required source assets and binary patch tables remain eligible for version control.

Extensive testing includes portable gameplay and ownership checks, interrupted-transfer and storage-failure injection, Android profile/journal tests, native ARM checks for DS startup, Wi-Fi and save/reload behavior, and build/signature verification. Use the project at your own risk and keep independent backups of your saves and walkers.

For a sync issue, include the console/app version, phone model, direction and displayed error. Console diagnostics are written to `sd:/3ds/pokewalker3ds/sync-error.log` on 3DS and `/data/pokewalker/sync-error.log` on NDS. Keep private ROMs and saves out of public issues.

Android Play Store preparation documents are in [android/play-store](android/play-store/README.md), including draft listing, privacy and testing material.

## Credits and license

SD Transporter builds on **[TWiLight Menu++](https://github.com/DS-Homebrew/TWiLightMenu)** and **[nds-bootstrap](https://github.com/DS-Homebrew/nds-bootstrap)**. Credit belongs to Rocket Robz, ahezard, shutterbug2000, Pk11 and the upstream projects' developers, designers, translators and other contributors.

The project also uses work from devkitPro and its library contributors, bundled third-party libraries, and Pokémon/Pokéwalker research projects. **[CREDITS.md](CREDITS.md)** records source provenance, additional authors, research references and the locations of retained notices. Existing file-level authorship and licenses remain authoritative.

See [LICENSE](LICENSE), the component license files and individual source notices. This independent project is not affiliated with or endorsed by Nintendo, Game Freak or The Pokémon Company.

### Returning after local files were deleted

Phone sync remains available when the console has no walker files. From the file list press **Y**, select the HGSS ROM under /roms, then **A** and confirm receiving the phone copy. This creates the matching missing save-folder destination and verifies the final .walker before acknowledging the return. If Android/iPhone reports a revoked old session, use its explicit phone-copy recovery confirmation with this console screen open. Existing active console files and other phone leases cannot be overwritten.

Deleting a phone profile's active Pokémon clears its trainer-name/public-ID pairing. The next receive can bind that empty profile to another trainer. An ordinary successful return keeps the pairing. Recovery copies remain separate and do not appear as active Pokémon.

## v1.6.2 update

- **"This Pokéwalker has not been registered":** when HGSS pairs a walker it first resets it, which clears the registration, then writes your trainer details. If the link is cut between those two steps, the walker is left unregistered. At launch, if the save has no Pokémon out and the walker has lost its registration, the bootstrap now puts back the newest copy registered to the same trainer (`<walker>.lastgood`, or the newest prelaunch snapshot), keeping steps and watts and ending any leftover stroll. A walker wiped on purpose, or a `.walker` file you deleted, is recognised (still erased) and left as a new walker. Noted in `_nds/nds-bootstrap/pokewalker-repair.txt`.
- **SELECT + LEFT now needs a deliberate hold:** both buttons must be held for about two seconds on the search screen before the walker is wiped. A quick or accidental press does nothing, and it never works once a connection has started.

## v1.6.1 update

- **Lost walker Pokémon and "a Pokémon is already on the Pokéwalker" (DSi/3DS):** since v1.5.1 the walker held back its final "connection complete" reply until every change was verified on the SD card. On a slower card that could outlast HGSS's timeout, so the game abandoned a return the walker had already applied: the Pokémon was in neither place, and the game's save still said it was out on the walker. The walker now replies straight away, writes everything as soon as the session ends, and answers a resent completion. The real-cartridge launcher saves the same way.
- **Holding SELECT through a transfer (all setups, including DS flashcarts):** the walker re-armed and advertised on the very first poll after a session ended if SELECT was still held, while HGSS was closing the link, so the game abandoned the send or return without saving. SELECT now only arms the walker on a fresh press after each link, and the walker stays silent for 120 polls after any session ends.
- **Automatic walker repair at launch:** before HGSS starts, the bootstrap compares the walker with the save. A Pokémon the save says isn't out (left on the walker by an unfinished send, and still in the party or PC) is removed so sending works again; a walker that lost the Pokémon the save says is out is restored from `<walker>.lastgood` or the newest matching prelaunch snapshot. Same trainer only, never a phone-held walker, and older joined partners are left alone. Changes are noted in `_nds/nds-bootstrap/pokewalker-repair.txt`; the pre-repair walker stays in the prelaunch snapshot. ROM launches only.
- **Real HGSS cartridge launcher:** fixed "Cannot verify the SD sector map". Raw SD reads went into stack buffers in ARM9 DTCM, which the ARM7 SD driver cannot reach; they now go through a main-RAM buffer.
- **Empty move slot with PP:** HGSS's overlay 112 sets walker move IDs over the default moveset without fixing PP. While the walker session runs, the bootstrap patches that routine in RAM so empty slots get 0 PP. It only patches an exact, unique match.
- **Illegal Pokémon:** removed the solo-join feature, which produced event-path Pokémon HGSS could not build legally.
- **Data checks:** before every save, the NDS, 3DS, Android and iPhone companions repair a damaged mirrored-record copy from the good one and refuse to save malformed walker data (species, level, item, form, or moves: missing first move, gaps or repeats). Consoles apply the same check to phone returns. Caught Pokémon are only stored from well-formed course data.
- Removed unused code: the UI preview build hooks and unused helpers.
- Release build: removed the DS-flashcart walker diagnostics (forced nds-bootstrap logging and `pokewalker-status.txt`). `pokewalker-error.txt` still explains why a walker did not connect.

## v1.5.1 update

The rebuilt bootstrap spreads background EEPROM writes across frames and holds the final session-completion acknowledgement until verified write-back has completed. Source, binaries and build records are included. These safeguards address a specific RAM-to-disk completion window; they do not establish the cause of every reported lost Pokemon. See [the bootstrap build record](bootstrap/BUILD.md).
