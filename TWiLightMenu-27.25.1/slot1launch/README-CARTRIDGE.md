# Real-cartridge virtual Pokewalker

Built as a standalone launcher.

This is a separate HGSS cartridge launcher for Nintendo DSi and Nintendo 3DS,
with a FAT32 console SD card. It does not replace `slot1launch.srldr`, the theme
loaders, nds-bootstrap, or any companion app in the main release. The existing
ROM implementation remains in this branch without changes.

**Hardware status:** the NDS binary builds, synthetic ARM checks pass, and hook
installation was verified against the owner's fourteen HGSS ARM9 dumps across
English, French, German, Italian, Spanish, Japanese and Korean. No physical
cartridge send/return, cartridge-save write, lid/sleep or SD timing round trip
has been verified. This is not yet established as working on either console.

## Use

1. Keep an independent backup of the HGSS cartridge save before initial testing.
2. Copy `Virtual-Pokewalker-Cartridge.nds` onto the **console SD card**, for
   example `/roms/nds/Virtual-Pokewalker-Cartridge.nds`.
3. Start TWiLight from the console SD and run the launcher in **DSi mode**.
   You can insert the retail cartridge before boot. If you started through
   DSpico with console SD access, wait for the insert prompt before swapping
   to the retail cartridge. Press **A** to check the cartridge.
4. Check the displayed cartridge, trainer and partner. Press **A** to launch
   or **B** to cancel. The swap path and cartridge return require hardware
   testing.
5. In HGSS, use the normal Pokewalker connection screen and hold **SELECT**.
   Keep the console SD inserted throughout the game session. After completed
   communication and the game's normal save, use the existing NDS/3DS/phone
   companion workflow with the same `.walker` file.

Files are created at:

```text
sd:/roms/nds/saves/Cartridge-<game code>-<header CRC>.walker
```

The game continues to read its ROM and save from the real cartridge. Its save
is not redirected to an SD `.sav`. The launcher loads/writes only the virtual
walker on SD and preserves a prelaunch snapshot under `saves/transfers/backups`.
Phone leases block launching; a missing active walker with `.bak`, `.tmp`,
`.replace-old`, `.bak.tmp` or `.phone` candidates requires recovery first.

The filename identifies an edition/region/header revision, **not a unique
physical cartridge**. Two cartridges with the same header share that default
path. Check the displayed partner/trainer and use separate SD setups for such
cartridges until per-cartridge save identity support exists. Do not reuse a ROM
walker's active session against a different cartridge save.

## Implementation

The upstream TWiLight Slot-1 launcher loads the real game's executable and
preserves its original cartridge access. The new build enables its ARM7 IRQ
engine and SD clock. After decompression, the installer validates the HGSS IR
function signatures and SDK heap ceiling, lowers the ceiling by 128 KiB to
reserve space for the 80 KiB resident walker and the loader's existing buffers,
then installs the existing protocol blob. Missing/ambiguous signatures, wrong
headers, insufficient space and corrupt staged data stop the launch.

The SD file is preallocated to 64 KiB before launch. A FAT32 reader resolves its
full physical sector map and verifies all bytes against the opened file. A
checksum protects that map; the console SD's CID is captured over FIFO and
checked by the resident service. Runtime writes touch only mapped file data
sectors; they do not edit FAT tables, directory entries or the cartridge save.

The ARM7 service waits for a pending protocol completion, then drains one dirty
4 KiB sector per VBlank. It checks previous-sector fingerprints, writes and
reads back every 512-byte chunk. The inherited final `0x68` response remains
pending until `flushedSeq` catches up. Failed writes stay dirty; known reset
paths require a successful forced flush before altering reset memory. These
checks reduce specific failure risks; physical tests remain necessary.

## Build on Windows or Linux

Requires Python 3, Arm GCC, **legacy libnds 1.8.3 + libfat 1.1.5**, and native
`ndstool`. The current Calico SDK is not interchangeable with this legacy loader.
Keep the complete repository layout: the build reuses `shared/`, the existing
generated walker payload header, and TWiLight's `universal/` directory.

Windows PowerShell:

```powershell
$env:DEVKITARM = 'C:/path/to/devkitARM'
$env:LIBNDS = 'C:/path/to/legacy/libnds'
$env:DEVKITPRO = 'C:/devkitPro'
python TWiLightMenu-27.25.1/slot1launch/build_cartwalker.py
```

Linux:

```sh
export DEVKITARM=/path/to/devkitARM
export LIBNDS=/path/to/legacy/libnds
export DEVKITPRO=/opt/devkitpro
python3 TWiLightMenu-27.25.1/slot1launch/build_cartwalker.py
```

This Windows build used the existing GCC 16.1 executable with the complete
restored legacy sysroot and headers described in the root build instructions.
On mixed compiler/sysroot installations, `CPLUS_INCLUDE_PATH` must point to
that legacy sysroot's matching C++ headers. No MSYS shell is used by this driver.

The driver creates a dedicated bounded-polling SD driver from the existing
source in its ignored build directory, rather than changing the main ROM
implementation. Generated products are in `build-cartwalker/`; the final NDS
file is beside the script. Neither is committed as project source.

## Source and credits

The imported Slot-1 launcher is based on DS-Homebrew/TWiLightMenu **v27.25.1**
(commit `8d3a32ff0b5cb7e588b9d030ab6e88bc2412f1bc`). Existing copyright and
licence notices are retained. Its NitroHax loader/cheat engine derives from
Michael “Chishm” Chisholm's work, with the credited Darkain and yellow wood
goblin contributions. The SD driver retains Normmatt's original licence notice;
imported I2C and crypto sources retain their respective authors' notices.

The protocol and companion file format come from SD Transporter; see the root
`CREDITS.md`, `LICENSE`, and `shared/REFERENCES.md`. This development used OpenAI
Codex. It is not affiliated with Nintendo, Game Freak or The Pokemon Company.
