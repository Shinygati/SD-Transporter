# Rebuilding the supplied retail bootstrap

The `retail` tree corresponds to the compiled bootstrap in this release. All runtime source, generated assembly inputs and NitroFS assets are included. `phone-lock.patch` shows this release's change against the SD Transporter working bootstrap.

Use **devkitARM r65 (GCC 14.2.0)** with **libnds 1.8.3 / compatible libfat 1.1.5**, rather than current Calico libraries. `LEGACY_LIBNDS` below must name one SDK directory containing both those legacy headers and libraries. This older compiler installation also needs its legacy `ds_arm7.specs`/startup objects. GNU Make, Python 3, bin2s, ndstool, a POSIX shell, mktemp/tr/rm and an LZSS host tool are needed. Use devkitPro MSYS2 on Windows; on Linux use your configured devkitPro shell. Do not reuse object files built with GCC 16.

Build the included LZSS host tool with a host C compiler if needed:

```sh
cc lzss.c -O2 -o lzss
```

Set `COMPILER_DIR` to the r65 compiler directory and `SDK_DIR` to the merged legacy SDK. From this directory:

```sh
export DEVKITPRO=/opt/devkitpro  # Windows MSYS2 normally uses /opt/devkitpro too
export PATH="$COMPILER_DIR/bin:$DEVKITPRO/tools/bin:$PATH"
make -C retail -j4 \
  DEVKITARM="$PWD/portable-ds-rules" \
  LEGACY_DEVKITARM="$COMPILER_DIR" LEGACY_LIBNDS="$SDK_DIR" LIBDIRS="$SDK_DIR" \
  PYTHON=python3 GIT_VER=sdlang2 LZSS="$PWD/lzss" all
```

On Windows use native/forward-slash paths accepted by the installed make/compiler tools if MSYS path conversion is disabled, and `lzss.exe` / `python` where appropriate. `SHELL=/path/to/sh.exe` can select the POSIX shell for a native Windows GNU Make. Output: `retail/bin/nds-bootstrap.nds`; install as `_nds/nds-bootstrap-nightly.nds` and choose Nightly in TWiLight Menu++.

`portable-ds-rules` contains the platform-neutral equivalent of the rules used for this release: it selects the legacy compiler, enables the language versions required by the working source, and supplies the ELF linker rule omitted from some older child Makefiles. The standalone NDS companion is a separate current-SDK application.


## DS Lite walker payload and version metadata

Run `python build_walker.py` with `LEGACY_DEVKITARM` set to the compiler directory before rebuilding after changes to `retail/pokewalker/`. The generator regenerates both the protocol/stub header and the position-independent DS dispatcher, then packs `retail/nitrofiles/walker_ds.bin` plus its offset/size metadata header. This asset is a required generated build input, not a game dump. Keeping it outside the ARM7 loader preserves stack space. The compiled NDS embeds it; no separate copy of `walker_ds.bin` goes on the SD card.

Copy this directory's `nightly-bootstrap.ver` alongside the compiled loader at `/_nds/nightly-bootstrap.ver` so TWiLight settings can display the custom version. See `tests/README.md` for the loader-header and synthetic ARM runtime checks. DS Lite support remains experimental pending hardware validation.


## Original GBA and Gen 5 payload generators

Normal bootstrap compilation uses the included, validated generated headers.
The corresponding original payload sources and generators are also included:

- `bootstrap/retail/slot2emu/` and `bootstrap/build_slot2_emu.py`
- `bootstrap/retail/gen5-transfer/sd_adapter.c`, `client_profiles.json`, and `bootstrap/build_gen5_sd_adapter.py`
- `bootstrap/retail/gen5-transfer/dlp_arm7_bridge.s` and `bootstrap/build_gen5_arm7_bridge.py`

From the repository root, run `python bootstrap/build_slot2_emu.py`,
`python bootstrap/build_gen5_sd_adapter.py`, or
`python bootstrap/build_gen5_arm7_bridge.py` after changing those payload sources.
They use `LEGACY_DEVKITARM` if set, otherwise `DEVKITARM`; both Windows native/MSYS
paths and Linux devkitPro paths are supported. These freestanding generators do
not require a retail ROM or Download Play executable. The output headers are
written under `bootstrap/retail/common/include/`. Compiler versions can change
payload bytes, relocation offsets and layout metadata, so keep regenerated
headers together and rebuild bootstrap after regeneration. The included Gen V adapter/profile headers were regenerated together with GCC 14.2 for the regional source classifier update. The Slot-2 header uses GCC 16.1. Keep each generated payload and its corresponding hook metadata together.

## Regional received clients and recovery verification

The adapter has 28 verified English/French/German/Italian/Spanish/Japanese/Korean BW/B2W2 layouts, including both received variants. Keep `gen5_profile_codec.py` beside `build_gen5_sd_adapter.py`; it losslessly emits compact profile tables for the ARM7 memory budget. Regenerate the adapter, profile and identity headers together with GCC 14.2. Native client offsets and CRC checks remain profile-specific; modified clients are refused. No retail executable is needed to regenerate these original payloads.

The loader creates immutable verified pre-launch `.walker` snapshots under adjacent `transfers/backups/`. Existing phone locks still prevent HGSS from opening a checked-out walker. The NTR ARM7 walker writer verifies sector writes through checked I/O and retains failed sectors for retry. These are recovery safeguards, not a complete power-loss transaction across the DS save and EEPROM.

## v1.5.1 console write-back rebuild (4 November 2026)

The compiled v1.5.1 bootstrap bounds DSi/3DS background walker saves to one dirty 4 KiB sector per frame, keeps complete forced flushes, and withholds the final session-completion acknowledgement until the verified flush sequence catches up. A completion waiting on storage starts draining immediately. The DS Lite path retains its synchronous verified completion save.

The SDK removed during cleanup was restored from the official `devkitpro/devkitarm:20241104` container snapshot: libnds 1.8.3, libfat 1.1.5, legacy startup/specs and target libraries. This Windows rebuild uses devkitARM GCC 16.1 with that isolated legacy SDK, rather than the baseline r65 compiler above. All objects were built together; no objects from older compilers were reused. The legacy C++ include directories were supplied through `CPLUS_INCLUDE_PATH` (`include/c++/14.2.0`, its `arm-none-eabi` subdirectory, and `backward`). Native GNU Make 4.4.1 and BusyBox-w32 provided the build shell because the installed MSYS2 shell would not run in this environment.

All four loader launch headers and twelve B4DS cardengine headers match their C layouts. Loader stack headroom is 8528, 7088, 24768 and 4320 bytes. The synthetic ARM runtime checks cover valid and rejected hook installation, fragmented FAT reads, verified writes, short/mismatched writes, retries, cache invalidation and device ownership. Final-ACK gating and bounded background writes pass their native failure checks. These checks do not establish physical SD timing or prove the cause of a specific reported lost Pokemon; keep independent `.sav` and `.walker` backups when trying the new loader.
