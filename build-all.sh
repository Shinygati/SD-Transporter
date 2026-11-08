#!/usr/bin/env bash
# Build SD Transporter: nds-bootstrap (with the virtual walker), the
# real-cartridge launcher, and the NDS and 3DS companion apps.
#
# Run from WSL (Ubuntu) in the repository folder, with Docker available:
#   cd /mnt/c/Users/scar_/Documents/GitHub/SD-Transporter
#   bash build-all.sh
#
# Outputs (copied to ./out):
#   nds-bootstrap-nightly.nds         bootstrap/retail/bin/nds-bootstrap.nds
#   Virtual-Pokewalker-Cartridge.nds  TWiLightMenu-27.25.1/slot1launch/
#   Pokewalker-NDS.nds                nds/
#   pokewalker3ds.3dsx                3ds/
# The Android app is built separately in Android Studio (see the end).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
OLD=devkitpro/devkitarm:20241104   # GCC 14.2 + legacy libnds: bootstrap, walker, cartridge launcher
NEW=devkitpro/devkitarm:latest     # current toolchain: NDS and 3DS apps

dock() { # dock IMAGE WORKDIR COMMAND
  docker run --rm -v "$ROOT":/src -w "/src/$2" "$1" bash -c "$3"
}

echo "== Pulling build images (first run only takes a while)"
docker pull -q "$OLD" >/dev/null
docker pull -q "$NEW" >/dev/null

echo "== Walker payloads"
dock "$OLD" bootstrap 'LEGACY_DEVKITARM=/opt/devkitpro/devkitARM python3 build_walker.py'

echo "== nds-bootstrap"
chmod +x "$ROOT/bootstrap/lzss"
dock "$OLD" bootstrap '
  export DEVKITPRO=/opt/devkitpro PATH=/opt/devkitpro/devkitARM/bin:/opt/devkitpro/tools/bin:$PATH
  make -C retail -j"$(nproc)" DEVKITARM=$PWD/portable-ds-rules \
    LEGACY_DEVKITARM=/opt/devkitpro/devkitARM LEGACY_LIBNDS=/opt/devkitpro/libnds \
    LIBDIRS=/opt/devkitpro/libnds PYTHON=python3 GIT_VER=sdlang2 LZSS=$PWD/lzss all'

echo "== Real-cartridge launcher"
dock "$OLD" TWiLightMenu-27.25.1/slot1launch '
  export DEVKITPRO=/opt/devkitpro DEVKITARM=/opt/devkitpro/devkitARM LIBNDS=/opt/devkitpro/libnds
  export PATH=/opt/devkitpro/tools/bin:$PATH
  python3 build_cartwalker.py'

echo "== NDS companion"
dock "$NEW" nds 'python3 build.py'

echo "== 3DS companion"
# Old dependency files from a Windows build contain "C:\..." paths that make
# can't parse ("target pattern contains no '%'"), so start from a clean folder.
rm -rf "$ROOT/3ds/build"
dock "$NEW" 3ds 'make -j"$(nproc)"'

mkdir -p "$ROOT/out"
cp "$ROOT/bootstrap/retail/bin/nds-bootstrap.nds"                       "$ROOT/out/nds-bootstrap-nightly.nds"
cp "$ROOT/TWiLightMenu-27.25.1/slot1launch/Virtual-Pokewalker-Cartridge.nds" "$ROOT/out/"
cp "$ROOT/nds/Pokewalker-NDS.nds"                                       "$ROOT/out/"
cp "$ROOT/3ds/pokewalker3ds.3dsx"                                       "$ROOT/out/"

echo
echo "Done. Files are in: $ROOT/out"
ls -la "$ROOT/out"
cat <<'EOF'

Android app: open the "android" folder in Android Studio, then
Build > Generate Signed App Bundle or APK > APK, choose your existing
Pokewalker-Android.jks keystore (alias "pokewalker"), and build "release".
Always use that same keystore so phones accept the update.
EOF