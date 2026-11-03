# DS Lite walker validation

After building all four bootstrap loaders, run:

```sh
python tests/test_ds_header.py --devkitarm "$COMPILER_DIR" --sdk "$SDK_DIR"
python -m pip install unicorn
python tests/test_ds_runtime.py
```

Run from `bootstrap/`, or adjust the script paths. The header test compiles an offsetof table using the supplied compiler/legacy SDK and compares it with each DS/DSi loader ELF. It also checks at least 6 KiB of DS-loader stack headroom and 4 KiB for the existing DSi loaders. The runtime test uses synthetic ARM9 game routines and mock storage, executing the actual compiled bootloader and relocated DS dispatcher in Unicorn. It executes the real SDK heap scanner and checks installation both before and after the normal loader has patched the heap limit, installation through the real compiled file reader with fragmented mock media and sector-boundary payload reads, rejection of missing/invalid heap locations, ambiguous signatures, insufficient memory, wrong game, short payload, write coalescing, failed writes/readback, retry, cache invalidation and restored device ownership. No retail ROM, save or walker is needed. Hardware booting/IR timing/flashcart behavior still require device testing.


## Virtual cartridge checks

After setting `DEVKITARM` to a devkitARM installation, run from `bootstrap/`:

```sh
python build_slot2_emu.py
python tests/test_slot2_emu.py
python build_gen5_sd_adapter.py
python tests/test_gen5_languages.py
```

These tests need Unicorn (`python -m pip install unicorn`). The Slot-2 suite
builds synthetic ARM/Thumb programs in a temporary directory and executes the
actual blob, including flash commands, save writes, cache-control/install paths,
array reads with matching/differing alignment, bank limits, SRAM wrapping and
chip-ID mode. No retail data is needed. The Gen V policy test executes the
adapter's source-family and language checks, preserves the original source
header and refuses unknown games/languages. It does not establish compatibility
with untranslated/translated retail clients or Japanese/Korean save data.

The Slot-2 generator's checked-in blob was generated with devkitARM GCC 16.1;
the Gen V adapter and matching profile hook metadata use GCC 14.2. Regenerate
paired Gen V headers together. Physical loading time and wireless transfer
stability still require console testing.

## Pre-launch backup helper

`test_walker_history.cpp` is a standard-library C++17 host test (MSVC `/std:c++17` or GCC/Clang `-std=c++17`). It creates synthetic files in a uniquely named temporary directory and tests immutable retry deduplication, preservation of changed walkers, corrupt-existing-copy refusal, short inputs and blocked backup directories. No retail data is required.

The regional development snapshot additionally passed 28 exact native client installation/layout tests, 980 real-header/native save-reader cases, 42 DP/Pt/HGSS native commit/reload cases and production reset/handoff tests for all 28 profiles. Retail client fixtures are intentionally not distributed. These CPU checks do not verify radio timing or Japanese/Korean text conversion on hardware.
