# Building SD Transporter on Windows

Run commands from the repository root unless a section says otherwise. The independently built components are the bootstrap, three TWiLight Menu++ themes, 3DS companion, NDS companion and Android app. `shared/` contains library code consumed by Android, not a separate executable.

## Toolchains

| Components | Required SDK |
| --- | --- |
| `bootstrap/`, all three menu themes | devkitARM r65 / GCC 14.2.0, legacy libnds 1.8.3 and compatible libfat 1.1.5; themes additionally need Maxmod 1.0.15 ARM7/ARM9 libraries |
| `3ds/` | Current devkitPro `3ds-dev`: devkitARM, libctru, Citro2D/Citro3D and 3DS packaging tools |
| `nds/` | Current devkitPro `nds-dev`: devkitARM, libnds, libfat, DSWifi, Calico and ndstool |
| `android/` | JDK 17+, Gradle 9.3.1, AGP 9.1.1, Android SDK Platform 36, Build Tools 36, NDK 29.0.14206865 and CMake 3.22.1 or a compatible configured version |

Install [devkitPro and its MSYS2 environment](https://devkitpro.org/wiki/Getting_Started), then install the appropriate package groups in that shell:

```sh
pacman -S 3ds-dev nds-dev
```

Also install Git, Python 3, GNU Make and a host C compiler. Legacy builds need the devkitPro `bin2s`, `grit`, `mmutil` and `ndstool` tools plus POSIX utilities such as `mktemp`, `tr`, `cp` and `rm`. The 3DS CIA build additionally uses [makerom](https://github.com/3DSGuy/Project_CTR); put its executable on PATH.

**Keep the legacy and current DS SDKs separate.** Installing the latest `nds-dev` does not provide the legacy library versions used by the menus/bootstrap. Do not copy Calico headers into the legacy SDK or reuse objects across compiler versions. The legacy compiler also needs its DS startup/specs files; a compiler-only archive is insufficient.

## 1. Legacy environment for bootstrap and menu themes

Use the **devkitPro MSYS2 shell** for these Makefiles. They invoke POSIX tools; ordinary PowerShell alone is not the intended environment.

Prepare a separate r65 compiler and a legacy SDK directory containing `include/` and `lib/`. That SDK must contain libnds, compatible libfat, and both Maxmod libraries for the theme builds. Set paths to your own installations; the following are examples:

```sh
cd /c/Users/YOUR_NAME/.../SD-Transporter
export REPO_ROOT="$PWD"
export DEVKITPRO=/opt/devkitpro
export COMPILER_DIR=/c/devkitPro-legacy/devkitARM
export SDK_DIR=/c/devkitPro-legacy/nds-sdk
export PATH="$COMPILER_DIR/bin:$DEVKITPRO/tools/bin:$PATH"

arm-none-eabi-gcc --version  # must be GCC 14.2.0 for this configuration
test -f "$SDK_DIR/lib/libnds9.a"
test -f "$SDK_DIR/lib/libnds7.a"
test -f "$SDK_DIR/lib/libfat.a"
test -f "$SDK_DIR/lib/libmm9.a"
test -f "$SDK_DIR/lib/libmm7.a"
```

`bootstrap/portable-ds-rules` selects that compiler and supplies the legacy linking rules used for these sources. In the commands below, `DEVKITARM` points at those rules, while `LEGACY_DEVKITARM` points at the real compiler. `LIBDIRS` overrides historical machine-specific library paths.

MSYS2/native-tool path conversion varies by installation. If a native make/compiler cannot resolve `/c/...`, use forward-slash Windows paths such as `C:/devkitPro-legacy/devkitARM` and `C:/devkitPro-legacy/nds-sdk` for compiler/library arguments. A native Windows GNU Make may also need `SHELL=C:/path/to/sh.exe`.

### Bootstrap: `bootstrap/`

Build the included LZSS host tool, then the retail bootstrap:

```sh
cd "$REPO_ROOT"
gcc bootstrap/lzss.c -O2 -o bootstrap/lzss.exe
export LEGACY_DEVKITARM="$COMPILER_DIR"
python bootstrap/build_walker.py

make -C bootstrap/retail -j4 \
  DEVKITARM="$REPO_ROOT/bootstrap/portable-ds-rules" \
  LEGACY_DEVKITARM="$COMPILER_DIR" LEGACY_LIBNDS="$SDK_DIR" LIBDIRS="$SDK_DIR" \
  PYTHON=python GIT_VER=sd-transporter LZSS="$REPO_ROOT/bootstrap/lzss.exe" all
```

Output: `bootstrap/retail/bin/nds-bootstrap.nds`.

Install that file as `/_nds/nds-bootstrap-nightly.nds` and copy `bootstrap/nightly-bootstrap.ver` to `/_nds/nightly-bootstrap.ver` on the appropriate console SD or flashcart microSD, and choose **Nightly** in the relevant game settings. The source includes GBA migration, Gen V Download Play redirection, virtual HGSS walker support and the phone-lock guard. See [bootstrap/BUILD.md](bootstrap/BUILD.md) for additional legacy toolchain details.

### DSi-based theme: `TWiLightMenu-27.25.1/romsel_dsimenutheme/`

```sh
make -C TWiLightMenu-27.25.1/romsel_dsimenutheme -j1 \
  DEVKITARM="$REPO_ROOT/bootstrap/portable-ds-rules" \
  LEGACY_DEVKITARM="$COMPILER_DIR" LEGACY_LIBNDS="$SDK_DIR" LIBDIRS="$SDK_DIR"
```

Output: `TWiLightMenu-27.25.1/romsel_dsimenutheme/romsel_dsimenutheme.nds`.

Rename/copy it to `sd:/_nds/TWiLightMenu/dsimenu.srldr`.

### R4 theme: `TWiLightMenu-27.25.1/romsel_r4theme/`

```sh
make -C TWiLightMenu-27.25.1/romsel_r4theme -j1 \
  DEVKITARM="$REPO_ROOT/bootstrap/portable-ds-rules" \
  LEGACY_DEVKITARM="$COMPILER_DIR" LEGACY_LIBNDS="$SDK_DIR" LIBDIRS="$SDK_DIR"
```

Output: `TWiLightMenu-27.25.1/romsel_r4theme/romsel_r4theme.nds`.

Rename/copy it to `sd:/_nds/TWiLightMenu/r4menu.srldr`.

### Acekard theme: `TWiLightMenu-27.25.1/romsel_aktheme/`

```sh
make -C TWiLightMenu-27.25.1/romsel_aktheme -j1 \
  DEVKITARM="$REPO_ROOT/bootstrap/portable-ds-rules" \
  LEGACY_DEVKITARM="$COMPILER_DIR" LEGACY_LIBNDS="$SDK_DIR" LIBDIRS="$SDK_DIR"
```

Output: `TWiLightMenu-27.25.1/romsel_aktheme/romsel_aktheme.nds`.

Rename/copy it to `sd:/_nds/TWiLightMenu/akmenu.srldr`.

Build the themes sequentially: they share bootloader/bootstub staging inputs. `-j1` keeps their legacy staging order deterministic. Build each theme as a complete target rather than copying an old ARM7 or ARM9 object tree into it.

`TWiLightMenu-27.25.1/universal/` is built through these theme targets. It does not need a separate application build. This source snapshot contains the modified theme targets; unrelated upstream applications are outside this build list.

## 2. 3DS companion: `3ds/`

Open a **fresh devkitPro MSYS2 shell** so the legacy compiler/rule overrides are not inherited. Use the current 3DS SDK:

```sh
cd /c/Users/YOUR_NAME/.../SD-Transporter
export DEVKITPRO=/opt/devkitpro
export DEVKITARM="$DEVKITPRO/devkitARM"
export PATH="$DEVKITARM/bin:$DEVKITPRO/tools/bin:$PATH"
make -C 3ds -j4
```

Outputs: `3ds/pokewalker3ds.3dsx`, `3ds/pokewalker3ds.smdh` and a debugging ELF. Copy the `.3dsx` and `.smdh` to `sd:/3ds/pokewalker3ds/`.

For a CIA, put makerom on PATH:

```sh
make -C 3ds cia
# Or select its executable explicitly:
make -C 3ds cia MAKEROM=C:/Tools/makerom.exe
```

Output: `3ds/pokewalker3ds.cia`. Its title ID is `000400000F7E1200`.

The original `banner.bin` is included. Optional regeneration uses bannertool via `make -C 3ds banner`; it is not required for a normal build. See [3ds/README.md](3ds/README.md).

## 3. NDS companion: `nds/`

This application uses the **current Calico-based SDK**, not the legacy SDK above. The provided Python script can run directly in **PowerShell** after installing the current `nds-dev` package group:

```powershell
Set-Location C:/Users/YOUR_NAME/.../SD-Transporter
$env:DEVKITPRO = 'C:/devkitPro'
$env:DEVKITARM = 'C:/devkitPro/devkitARM'
python nds/build.py
```

Output: `nds/Pokewalker-NDS.nds`. Launch it through TWiLight Menu++ or a compatible flashcart with working SD/DLDI access.

The script compiles its local `source/` and `compat/` files and links the installed libnds/libfat/DSWifi/Calico libraries, using Calico's standard `ds7_maine.elf`. It does not require the 3DS SDK.

## 4. Android companion: `android/`

Install Android Studio or the command-line Android SDK, JDK 17 or newer, and **Gradle 9.3.1**. Install SDK Platform 36, Build Tools 36, NDK **29.0.14206865**, and CMake **3.22.1** through SDK Manager. Accept the SDK licenses.

The project specifies Android Gradle Plugin **9.1.1**. It has no bundled Gradle wrapper. Use a matching installed Gradle distribution, or configure Android Studio to use one when importing `android/`.

In PowerShell, confirm the tools and configure your SDK path:

```powershell
Set-Location C:/Users/YOUR_NAME/.../SD-Transporter
java -version
gradle --version
$env:ANDROID_HOME = 'C:/Users/YOUR_NAME/AppData/Local/Android/Sdk'
Set-Location android
gradle assembleDebug
```

Alternatively create `android/local.properties` containing your actual SDK location:

```properties
sdk.dir=C:/Users/YOUR_NAME/AppData/Local/Android/Sdk
```

Output: `android/app/build/outputs/apk/debug/app-debug.apk`.

The CMake project compiles `android/app/src/main/cpp/bridge.c` with `shared/walkerfile.c` and `shared/species.c`, producing ARM64, ARMv7 and x86-64 native libraries. Keep `shared/` beside `android/`; do not move only the Android folder elsewhere. Android Studio can also build the imported project through its normal Build menu.

For release builds, configure your own signing key and use the appropriate Gradle release task. Keep signing keys and passwords outside version control. A locally generated debug key differs from the distributed test APK's key: return the walker before uninstalling/replacing an incompatible-signed installation, since uninstall removes private phone state. Same-key APK updates preserve that state.

The distributed APK was built using official SDK/NDK command-line tools. The Gradle project is supplied as the portable build route; that route has not been exercised for this snapshot. No private release/debug signing key is included in this repository.

## 5. Shared code and clean builds

`shared/` has no independent build command. Android compiles it through CMake. The console companions currently carry their own corresponding `walkerfile`/`species` sources; changes to common behavior must be synchronized before building all platforms.

The root ignore rules keep normal compiler output, packages, local SDK paths, Android caches and signing material out of Git. Build products are distributed separately from source commits.

After changing SDK/compiler versions, clean that component before rebuilding. Do not copy old `.o`, `.elf` or generated graphics objects into a new toolchain build. Compilation verifies the build, not console booting, wireless behavior or physical step delivery; test on the target devices before publishing a release.


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

## iPhone source and Windows installation

The `ios/` target requires macOS and Xcode to compile. Windows cannot compile this native iOS target using the installed devkitPro/Android tools. Keep `shared/` next to `ios/`, open `ios/Pokewalker.xcodeproj` on macOS, or run `sh ios/tools/build-ipa.sh` there to generate an unsigned IPA. The optional `ios/ci/build-ios.yml` template enables a manual macOS CI build only if the owner installs it as a GitHub workflow; no remote workflow has been enabled or run.

Windows users can sign/install the resulting IPA with AltServer/AltStore Classic. Follow [official Windows setup](https://faq.altstore.io/altstore-classic/how-to-install-altstore-windows). Free-account apps generally need refreshing every seven days. Return/export an active walker before uninstalling or changing signing identity. See [ios/README.md](ios/README.md) for dependencies, limitations and pending device checks. No compiled iOS app is included in this snapshot.

For an Android production AAB, see [android/play-store/RELEASE-BUILD.md](android/play-store/RELEASE-BUILD.md). A private test APK is not a signed Play release bundle; production policy requirements remain in [the readiness report](android/play-store/README.md).
