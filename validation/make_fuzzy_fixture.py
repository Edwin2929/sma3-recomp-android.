#!/usr/bin/env python3
"""Create a private entry near the original Fuzzy generator in level 1-7.

This changes entrance selection only. The original game initializes the level,
loads its authored sprites, and triggers the effect through real contact.
Neither Fuzzy counters nor enemy data are patched. Never publish the state.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
from make_level_fixture import sections


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    raw = args.source.read_bytes()
    try:
        start, size = sections(raw)[b"BUS0"]
        if size < 0x48000:
            raise ValueError("Truncated bus memory")
        iwram = start + 0x40000
        if struct.unpack_from("<I", raw, iwram + 0x7240)[0] != 0x0300220C:
            raise ValueError("Expected initialized gameplay memory")
        if raw[iwram + 0x6B05] != 13:
            raise ValueError("Expected normal-gameplay source")
    except (KeyError, ValueError) as exc:
        parser.error(str(exc))
    data = bytearray(raw)
    changes = ((0x6288, 6, "H"), (0x6A52, 1, "H"), (0x6B05, 10, "B"), (0x224A, 0, "H"))
    for offset, value, fmt in changes:
        struct.pack_into("<" + fmt, data, iwram + offset, value)
    data[start + 0x1B000:start + 0x1B006] = bytes((6, 130, 120, 2, 0x77, 4))
    with args.output.open("xb") as stream:
        stream.write(data)
    print(json.dumps({
        "synthetic_entry": True, "level_id": 6, "sublevel_id": 6,
        "source_sha256": hashlib.sha256(raw).hexdigest(),
        "fixture_sha256": hashlib.sha256(data).hexdigest(),
        "changed_iwram_addresses": [f"{0x03000000 + offset:08X}" for offset, _, _ in changes],
        "changed_ewram_range": "0201B000..0201B005",
        "scope": "Entrance setup only; contact and recovery must be observed during execution.",
    }, indent=2))


if __name__ == "__main__":
    main()
