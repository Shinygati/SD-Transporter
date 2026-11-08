#!/usr/bin/env bash
# Build SD Transporter and assemble the release folders.
#
# Run from WSL (Ubuntu) in the repository folder, with Docker available:
#   cd /mnt/c/Users/scar_/Documents/GitHub/SD-Transporter
#   bash build-all.sh
#
# Builds nds-bootstrap (with the virtual walker), the real-cartridge launcher,
# the NDS and 3DS companion apps and the three modified TWiLight Menu++ themes,
# then lays everything out per setup in out/SD-Transporter-v<version>/ and zips
# it. The official TWiLight Menu++ packages are downloaded once into
# .release-cache/. The signed Android APK is built too when the Android SDK
# (ANDROID_HOME) and your keystore are available (see the end).
#
# Needs in WSL: docker, python3, curl, zip and 7z
#   sudo apt install p7zip-full zip
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
OLD=devkitpro/devkitarm:20241104   # GCC 14.2 + legacy libnds: bootstrap, walker, launcher, themes
NEW=devkitpro/devkitarm:latest     # current toolchain: NDS and 3DS apps
TWL_VERSION=v27.25.1
TWL_URL=https://github.com/DS-Homebrew/TWiLightMenu/releases/download/$TWL_VERSION
CACHE="$ROOT/.release-cache"

for tool in docker python3 curl zip 7z; do
  command -v "$tool" >/dev/null || { echo "Missing '$tool'. Install it first (see the top of this script)."; exit 1; }
done

dock() { # dock IMAGE WORKDIR COMMAND
  docker run --rm -v "$ROOT":/src -w "/src/$2" "$1" bash -c "$3"
}
LEGACY='export DEVKITPRO=/opt/devkitpro PATH=/opt/devkitpro/devkitARM/bin:/opt/devkitpro/tools/bin:$PATH'

VERSION=$(sed -n 's/.*VER_NUMBER "\(.*\)".*/\1/p' "$ROOT/bootstrap/retail/arm9/include/version.h")
[ -n "$VERSION" ] || { echo "Could not read the version from version.h"; exit 1; }
echo "SD Transporter $VERSION"

echo "== Pulling build images (first run only takes a while)"
docker pull -q "$OLD" >/dev/null
docker pull -q "$NEW" >/dev/null

# ---------------------------------------------------------------- builds
echo "== Walker payloads"
dock "$OLD" bootstrap 'LEGACY_DEVKITARM=/opt/devkitpro/devkitARM python3 build_walker.py'

echo "== nds-bootstrap"
chmod +x "$ROOT/bootstrap/lzss"
dock "$OLD" bootstrap "$LEGACY"'
  make -C retail -j"$(nproc)" DEVKITARM=$PWD/portable-ds-rules \
    LEGACY_DEVKITARM=/opt/devkitpro/devkitARM LEGACY_LIBNDS=/opt/devkitpro/libnds \
    LIBDIRS=/opt/devkitpro/libnds PYTHON=python3 GIT_VER=sdlang2 LZSS=$PWD/lzss all'

echo "== Real-cartridge launcher"
dock "$OLD" TWiLightMenu-27.25.1/slot1launch '
  export DEVKITPRO=/opt/devkitpro DEVKITARM=/opt/devkitpro/devkitARM LIBNDS=/opt/devkitpro/libnds
  export PATH=/opt/devkitpro/tools/bin:$PATH
  python3 build_cartwalker.py'

echo "== TWiLight Menu++ themes (virtual Slot-1/Slot-2 options)"
for theme in romsel_aktheme romsel_dsimenutheme romsel_r4theme; do
  echo "   $theme"
  # Not parallel: each theme builds its loader before the menu that embeds it.
  dock "$OLD" "TWiLightMenu-27.25.1/$theme" "$LEGACY"'
    make DEVKITARM=/src/bootstrap/portable-ds-rules LEGACY_DEVKITARM=/opt/devkitpro/devkitARM \
      LIBNDS=/opt/devkitpro/libnds >/dev/null'
done

echo "== NDS companion"
dock "$NEW" nds 'python3 build.py'

echo "== 3DS companion"
# Old dependency files from a Windows build contain "C:\..." paths that make
# can't parse ("target pattern contains no '%'"), so start from a clean folder.
rm -rf "$ROOT/3ds/build"
dock "$NEW" 3ds 'make -j"$(nproc)"'

# ------------------------------------------------- official TWiLight files
echo "== TWiLight Menu++ $TWL_VERSION packages"
mkdir -p "$CACHE"
for pkg in DSi 3DS Flashcard; do
  if [ ! -d "$CACHE/$pkg" ]; then
    [ -f "$CACHE/TWiLightMenu-$pkg.7z" ] || curl -fsSL --retry 3 -o "$CACHE/TWiLightMenu-$pkg.7z" "$TWL_URL/TWiLightMenu-$pkg.7z"
    7z x -y -o"$CACHE/$pkg.tmp" "$CACHE/TWiLightMenu-$pkg.7z" >/dev/null
    mv "$CACHE/$pkg.tmp" "$CACHE/$pkg"
  fi
done

# The settings menu shows the full "Nightly" label for the custom bootstrap.
python3 "$ROOT/bootstrap/brand_twilight_settings.py" \
  "$CACHE/3DS/_nds/TWiLightMenu/settings.srldr" "$CACHE/settings.srldr"

# ------------------------------------------------------------- assemble
OUT="$ROOT/out"
REL="$OUT/SD-Transporter-v$VERSION"
rm -rf "$REL" "$REL.zip"
mkdir -p "$REL"

BOOTSTRAP="$ROOT/bootstrap/retail/bin/nds-bootstrap.nds"
CART="$ROOT/TWiLightMenu-27.25.1/slot1launch/Virtual-Pokewalker-Cartridge.nds"
NDSAPP="$ROOT/nds/Pokewalker-NDS.nds"
TWL="$ROOT/TWiLightMenu-27.25.1"

twilight() { # twilight PACKAGE DEST : official files, then SD Transporter on top
  local src="$CACHE/$1" dst="$2"
  mkdir -p "$dst"
  cp -a "$src/." "$dst/"
  rm -f "$dst/version.txt" "$dst/Games supported with widescreen.txt"
  rm -rf "$dst/Autoboot" "$dst/Flashcart Loader"      # flashcarts: shipped separately
  if [ -f "$dst/TWiLight Menu.cia" ]; then mkdir -p "$dst/cias"; mv "$dst/TWiLight Menu.cia" "$dst/cias/"; fi
  cp "$TWL/romsel_aktheme/romsel_aktheme.nds"           "$dst/_nds/TWiLightMenu/akmenu.srldr"
  cp "$TWL/romsel_dsimenutheme/romsel_dsimenutheme.nds" "$dst/_nds/TWiLightMenu/dsimenu.srldr"
  cp "$TWL/romsel_r4theme/romsel_r4theme.nds"           "$dst/_nds/TWiLightMenu/r4menu.srldr"
  cp "$CACHE/settings.srldr"                            "$dst/_nds/TWiLightMenu/settings.srldr"
  cp "$BOOTSTRAP"                                       "$dst/_nds/nds-bootstrap-nightly.nds"
  cp "$ROOT/bootstrap/nightly-bootstrap.ver"            "$dst/_nds/nightly-bootstrap.ver"
  mkdir -p "$dst/roms/nds"
  cp "$NDSAPP" "$dst/roms/nds/"
}
app3ds() { mkdir -p "$1/3ds/pokewalker3ds"; cp "$ROOT/3ds/pokewalker3ds.3dsx" "$ROOT/3ds/pokewalker3ds.smdh" "$1/3ds/pokewalker3ds/"; }
bootfiles() { # flashcart autoboot/loader files, only needed by some carts
  local d="$1/Flashcart boot files (only if your flashcart needs them)"
  mkdir -p "$d"; cp -a "$CACHE/Flashcard/Autoboot" "$CACHE/Flashcard/Flashcart Loader" "$d/"
}

echo "== Release folders"
S="$REL/3DS - custom firmware";  twilight 3DS "$S/Copy to 3DS SD card"; app3ds "$S/Copy to 3DS SD card"
cp "$CART" "$S/Copy to 3DS SD card/roms/nds/"

S="$REL/DSi - custom firmware";  twilight DSi "$S/Copy to DSi SD card"
cp "$CART" "$S/Copy to DSi SD card/roms/nds/"

S="$REL/DS or DS Lite - flashcart"; twilight Flashcard "$S/Copy to flashcart microSD"; bootfiles "$S"

S="$REL/DSi - flashcart"; twilight Flashcard "$S/Copy to flashcart microSD"; bootfiles "$S"
D="$S/Copy to DSi SD card (needs DSi-mode SD access, e.g. DSpico)/roms/nds"
mkdir -p "$D"; cp "$NDSAPP" "$CART" "$D/"

S="$REL/3DS - flashcart"; twilight Flashcard "$S/Copy to flashcart microSD"; bootfiles "$S"
app3ds "$S/Copy to 3DS SD card"

# Guides: START HERE, HOW TO USE, and one README per setup folder.
cp -a "$ROOT/release/." "$REL/"
cp "$ROOT/CREDITS.md" "$ROOT/LICENSE" "$REL/"

# Android: build the signed release APK when this WSL has the tools and the
# signing variables that android/app/build.gradle reads; otherwise use an APK
# built in Android Studio. Without the variables, the keystore and password
# file are taken from the folder below (outside the repository; the password
# is read here and never printed or copied).
SIGNING_DIR="${SIGNING_DIR:-/mnt/c/Users/scar_/Documents/Android/SD-Transporter}"
if [ -z "${ANDROID_KEYSTORE_PATH:-}" ] && [ -f "$SIGNING_DIR/Pokewalker-Android.jks" ] && [ -f "$SIGNING_DIR/signing-password.txt" ]; then
  IFS= read -r signing_password < "$SIGNING_DIR/signing-password.txt" || true
  signing_password="${signing_password%$'\r'}"
  export ANDROID_KEYSTORE_PATH="$SIGNING_DIR/Pokewalker-Android.jks"
  export ANDROID_STORE_PASSWORD="$signing_password" ANDROID_KEY_PASSWORD="$signing_password"
  export ANDROID_KEY_ALIAS="${ANDROID_KEY_ALIAS:-pokewalker}"
  unset signing_password
fi
android_missing=""
[ -n "${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}" ] || android_missing="$android_missing ANDROID_HOME"
for v in ANDROID_KEYSTORE_PATH ANDROID_STORE_PASSWORD ANDROID_KEY_ALIAS ANDROID_KEY_PASSWORD; do
  [ -n "${!v:-}" ] || android_missing="$android_missing $v"
done
if [ -z "$android_missing" ]; then
  echo "== Android app (signed release)"
  [ -f "$ANDROID_KEYSTORE_PATH" ] || { echo "Keystore not found: $ANDROID_KEYSTORE_PATH"; exit 1; }
  rm -f "$ROOT/android/app/build/outputs/apk/release/app-release.apk"
  chmod +x "$ROOT/android/gradlew"; sed -i 's/\r$//' "$ROOT/android/gradlew"
  (cd "$ROOT/android" && ./gradlew --no-daemon -q assembleRelease)
  apksigner=$(ls -d "${ANDROID_HOME:-$ANDROID_SDK_ROOT}"/build-tools/*/apksigner 2>/dev/null | sort -V | tail -1)
  if [ -n "$apksigner" ]; then
    echo "   signed with:"; "$apksigner" verify --print-certs "$ROOT/android/app/build/outputs/apk/release/app-release.apk" 2>/dev/null \
      | grep -E "certificate (DN|SHA-256)" | sed 's/^/     /'
  fi
else
  echo "== Android app: skipped (missing:$android_missing)"
fi

APK=""
for candidate in "$ROOT/android/app/build/outputs/apk/release/app-release.apk" "$OUT"/*.apk; do
  [ -f "$candidate" ] && { APK="$candidate"; break; }
done
if [ -n "$APK" ]; then cp "$APK" "$REL/Android/SD-Transporter-Pokewalker-$VERSION.apk"
else echo "   (no signed APK found: the Android folder only has its README)"; fi

(cd "$REL" && find . -type f ! -name SHA256SUMS.txt -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS.txt)
(cd "$OUT" && zip -qr -9 "SD-Transporter-v$VERSION.zip" "SD-Transporter-v$VERSION")

echo
echo "Done:"
echo "  $REL"
echo "  $REL.zip"
[ -z "$android_missing" ] || cat <<'EOF'

The Android app was not built here. Either set ANDROID_HOME (and keep your
keystore folder in place, or export the four ANDROID_* signing variables)
before running this script, or build it in Android
Studio (Build > Generate Signed App Bundle or APK > APK, your existing
Pokewalker-Android.jks, alias "pokewalker", release) and run the script again.
Always sign with that same keystore so phones accept the update.
EOF
