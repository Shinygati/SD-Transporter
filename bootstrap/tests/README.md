# DS Lite walker validation

After building all four bootstrap loaders, run:

```sh
python tests/test_ds_header.py --devkitarm "$COMPILER_DIR" --sdk "$SDK_DIR"
python -m pip install unicorn
python tests/test_ds_runtime.py
```

Run from `bootstrap/`, or adjust the script paths. The header test compiles an offsetof table using the supplied compiler/legacy SDK and compares it with each DS/DSi loader ELF. It also checks at least 6 KiB of DS-loader stack headroom and 4 KiB for the existing DSi loaders. The runtime test uses synthetic ARM9 game routines and mock storage, executing the actual compiled bootloader and relocated DS dispatcher in Unicorn. It executes the real SDK heap scanner and checks installation both before and after the normal loader has patched the heap limit, installation through the real compiled file reader with fragmented mock media and sector-boundary payload reads, rejection of missing/invalid heap locations, ambiguous signatures, insufficient memory, wrong game, short payload, write coalescing, failed writes/readback, retry, cache invalidation and restored device ownership. No retail ROM, save or walker is needed. Hardware booting/IR timing/flashcart behavior still require device testing.
