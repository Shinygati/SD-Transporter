#!/bin/sh
set -eu
ios_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_root="$ios_root/build-local"
mkdir -p "$build_root"
xcodebuild -project "$ios_root/Pokewalker.xcodeproj" -scheme Pokewalker \
  -configuration Release -sdk iphoneos -destination 'generic/platform=iOS' \
  -derivedDataPath "$build_root/DerivedData" CODE_SIGNING_ALLOWED=NO build
app="$build_root/DerivedData/Build/Products/Release-iphoneos/Pokewalker.app"
test -d "$app"
python3 - "$app" "$build_root/Pokewalker-unsigned.ipa" <<'PY'
from pathlib import Path
import sys,zipfile
app=Path(sys.argv[1]);destination=Path(sys.argv[2])
with zipfile.ZipFile(destination,'w',zipfile.ZIP_DEFLATED) as z:
    for p in sorted(app.rglob('*')):
        if p.is_file():z.write(p,'Payload/Pokewalker.app/'+p.relative_to(app).as_posix())
print(destination)
PY
