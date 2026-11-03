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
  PYTHON=python3 GIT_VER=walker-phone-sync LZSS="$PWD/lzss" all
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
headers together and rebuild bootstrap after regeneration. The archived working
build and existing validated headers have not been replaced by this packaging step.
