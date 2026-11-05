"""Let the bundled TWiLight settings show the full Nightly build label."""

import argparse
from hashlib import sha256
from pathlib import Path
from struct import unpack_from


# v27.25.1 settings builds from the upstream console SD packages. Offsets
# are Thumb addresses for movs r2, #7 in the inlined Nightly version readers.
# The matching SettingsGUI::bsVerText entries hold 25 bytes each.
NIGHTLY_READERS = {
    "82d10e0380e77c0d15042303dfb557da36d5729bdeec18b5b391ac17b02ead84": (
        0x0200CEC2, 0x0200D1D2, 0x0200F27A, 0x0200F706, 0x0201065A,
        0x02024EAA, 0x02025024, 0x02025138, 0x0202520E,
    ),
    "be7072157b11beb0127d7e47c9db2d7f45d042ad9ea5883c7b8a82e8919c196f": (
        0x0200CD3A, 0x0200D04A, 0x0200F59C, 0x0200FA2E, 0x02010A4E,
        0x0202474E, 0x020248C8, 0x020249DC, 0x02024AB2,
    ),
}


def brand_settings(original: bytes) -> bytes:
    readers = NIGHTLY_READERS.get(sha256(original).hexdigest())
    if readers is None:
        raise ValueError("Unsupported settings.srldr build. Use the bundled v27.25.1 input.")
    arm9_offset, _, arm9_address, arm9_size = unpack_from("<4I", original, 0x20)
    patched = bytearray(original)
    for address in readers:
        offset = arm9_offset + address - arm9_address
        if not arm9_offset <= offset < arm9_offset + arm9_size - 1:
            raise ValueError("Nightly reader is outside ARM9.")
        if original[offset:offset + 2] != bytes((7, 0x22)):
            raise ValueError("Nightly reader instruction did not match.")
        patched[offset] = 24
    return bytes(patched)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("original", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.write_bytes(brand_settings(args.original.read_bytes()))
