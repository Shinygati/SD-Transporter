"""Regenerate the console shell assets with Pillow and devkitPro tex3ds."""

import argparse
from pathlib import Path
import struct
import subprocess
import tempfile

from PIL import Image


def main():
    shared = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tex3ds", default="tex3ds")
    parser.add_argument(
        "--source", type=Path,
        default=shared.parent / "android/app/src/main/res/drawable-nodpi/walker_shell.png",
    )
    args = parser.parse_args()

    with Image.open(args.source) as source:
        shell = source.convert("RGBA")
    ds = shell.resize((184, 184), Image.Resampling.LANCZOS)
    pixels = [
        (0x8000 | (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10)) if a >= 128 else 0
        for r, g, b, a in struct.iter_unpack("4B", ds.tobytes())
    ]
    (shared / "walker-shell-nds.bin").write_bytes(struct.pack(f"<{len(pixels)}H", *pixels))

    with tempfile.TemporaryDirectory() as temporary:
        png = Path(temporary) / "shell.png"
        shell.resize((232, 232), Image.Resampling.LANCZOS).save(png)
        subprocess.run(
            [args.tex3ds, "-f", "rgba8", "-z", "auto",
             "-o", str(shared / "walker-shell-3ds.t3x"), str(png)],
            check=True,
        )


if __name__ == "__main__":
    main()
