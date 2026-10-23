#!/usr/bin/env python3
"""Extract PRIVATE frozen mask fixtures from the validated castle session.

These fixtures contain game OAM and OBJ texture bytes. Keep the output private;
publish only source code and aggregate results. They visualize the native mask
on a synthetic background, not the complete scene or a captured PNG.

Example:
  python validation/prepare_window_mask_fixtures.py \
    --sessions /private/castle-session-356 --rom /private/game.gba \
    --output /private/new-window-fixtures

The output directory must not exist. Input states and ROM are never modified.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

from make_level_fixture import sections


FRAMES = (6420, 6480, 6501, 6540, 6600, 6660, 6720, 6780, 6801)
ROM_SHA256 = 'a8bb331a6f0ff7af4387df9ac8566ae395863c2f79a5a597528a64c8bff57941'
SCOPE = ('Frozen native OAM/OBJ textures; exact coherent state provenance. '
         'Synthetic background, not PNG replay or complete gameplay rendering.')


def require(condition, message):
    if not condition:
        raise ValueError(message)


def state_parts(path):
    data = path.read_bytes()
    table = sections(data)
    require(b'BUS0' in table and b'IO_0' in table, 'Missing memory or I/O section')
    start, size = table[b'BUS0']
    require(size == 395416, 'Unexpected pinned-engine bus layout')
    bus = data[start:start + size]
    start, size = table[b'IO_0']
    require(size >= 1024, 'Truncated I/O page')
    io = data[start:start + 1024]
    return data, bus, io, bus[0x48400:0x60400], bus[0x60400:0x60800]


def triples(data, index):
    offset = index * 8
    require(index >= 0 and offset + 6 <= len(data), 'Attribute index out of bounds')
    return struct.unpack_from('<HHH', data, offset)


def prepare(sessions, rom, frame):
    path = sessions / f'frame-{frame:05d}.state'
    original, bus, io, vram, oam = state_parts(path)

    def read(address, width=2, signed=False):
        require(width in (1, 2, 4), 'Unsupported read width')
        if 0x03000000 <= address < 0x03008000:
            offset, data = address - 0x03000000 + 0x40000, bus
            limit = 0x48000
        elif 0x02000000 <= address < 0x02040000:
            offset, data = address - 0x02000000, bus
            limit = 0x40000
        elif 0x08000000 <= address < 0x08400000:
            offset, data = address - 0x08000000, rom
            limit = len(rom)
        else:
            raise ValueError('Unsupported read address')
        require(0 <= offset and offset + width <= min(limit, len(data)),
                'Read crosses a memory region boundary')
        return int.from_bytes(data[offset:offset + width], 'little', signed=signed)

    require(int.from_bytes(io[:2], 'little') == 0x9f00,
            'Not the supported destination profile')
    require(read(0x0300702c, 4) == 0x03002460 and
            read(0x03007240, 4) == 0x0300220c and read(0x03004ba6) == 0x1c,
            'Gameplay profile mismatch')
    require(read(0x03006d68, 1) == 0 and read(0x03006af7, 1) == 0,
            'Unexpected copy branch')
    count = read(0x03003cf0, 1)
    require(count == 11, 'Window prefix count changed')
    staging = bus[0x45a00:0x46200]
    copied = bus[0x2badc:0x2badc + 1024]
    active = [(i, triples(staging, i)) for i in range(256) if staging[i * 8] != 160]
    require(len(active) + count <= 128, 'Compaction truncation')
    require(all(triples(oam, count + i) == attrs for i, (_, attrs) in enumerate(active)),
            'Normal submission is incoherent')
    require(all(triples(oam, i) == triples(copied, i) for i in range(count)),
            'Window buffer/display mismatch')
    require(all(((triples(oam, i)[0] >> 10) & 3) != 2 for i in range(count, 128)),
            'Unexpected window outside prefix')

    positions = [-2147483648] * 128
    proof = []
    for index in range(count):
        a0, a1, a2 = triples(oam, index)
        require((a0 >> 10) & 3 == 2, 'Not a window copy')
        candidates = []
        # Search every staging slot, including Y=160 slots omitted from normal
        # compaction but retained by the separately copied window prefix.
        for source in range(256):
            s0, s1, s2 = triples(staging, source)
            if (s0 | 0x800, s1, s2 & 0xf3ff) != (a0, a1, a2):
                continue
            x = read(0x0202c8b0 + source * 4, signed=True)
            y = read(0x0202c8b2 + source * 4, signed=True)
            if (x & 511) == (s1 & 511) and (y & 255) == (s0 & 255):
                candidates.append(dict(source=source, x=x, method='exact_metadata_xy'))
            if s0 & 0x300 != 0x300:
                continue
            # These verified producers resize the third component after its
            # generic coordinate metadata has been written. Its final affine
            # box begins eight pixels earlier on each axis.
            for slot in range(24):
                owner = 0x03002460 + slot * 0xb0
                if (read(owner + 0x32) not in (0x76, 0x77) or
                        read(owner + 0x34) % 4 or read(owner + 0x34) // 4 + 2 != source):
                    continue
                require(read(owner + 0x20, signed=True) == x and
                        read(owner + 0x22, signed=True) == y, 'Owner/metadata disagreement')
                require(read(owner + 0x24) == 8 and read(owner + 0x2a) >> 11 == 3 and
                        read(owner + 0x94, 1) < 128 and read(owner + 0x62) == 0,
                        'Triplet owner guards')
                graphic = read(owner + 0x3c)
                require(graphic < 32, 'Graphic bound')
                require((s0 & 0x3ff) == (0x300 | ((y - 8) & 255)) and
                        (s1 & 0xc1ff) == (0x4000 | ((x - 8) & 511)),
                        'Triplet bounding box differs from owned transform')
                require((s2 & 1023) == read(0x08171e88 + graphic * 2),
                        'Triplet dynamic graphic differs')
                candidates.append(dict(source=source, x=x - 8,
                                       method='owned_triplet_affine_bbox_minus8', owner=hex(owner)))
        require(candidates, f'No provenance for frame {frame}, window {index}')
        full_x = {candidate['x'] for candidate in candidates}
        require(len(full_x) == 1, 'Ambiguous full X')
        positions[index] = next(iter(full_x))
        require(-32768 <= positions[index] <= 32767, 'Full X outside signed 16-bit range')
        require((positions[index] & 511) == (a1 & 511), 'Full X bits disagree')
        proof.append(dict(index=index, full_x=positions[index],
                          attrs=[f'{a:04x}' for a in (a0, a1, a2)], candidates=candidates))

    # Only object texture bytes, OAM, and proven host coordinates enter the
    # fixture. The synthetic background is constructed by the C++ test.
    data = (b'OWM1' + struct.pack('<I', int.from_bytes(io[:2], 'little') & 0x40) + oam +
            vram[0x10000:0x18000] + struct.pack('<128i', *positions))
    require(len(data) == 34312, 'Unexpected fixture size')
    report = dict(frame=frame, state_sha256=hashlib.sha256(original).hexdigest(),
                  input_sha256=hashlib.sha256(data).hexdigest(),
                  normal_submission_entries=len(active), window_copies=count, positions=proof)
    return data, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sessions', type=Path, required=True)
    parser.add_argument('--rom', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        require(not args.output.exists(), 'Output directory already exists')
        rom = args.rom.read_bytes()
        require(len(rom) == 4 * 1024 * 1024 and hashlib.sha256(rom).hexdigest() == ROM_SHA256,
                'Unsupported ROM identity')
        prepared = [prepare(args.sessions, rom, frame) for frame in FRAMES]
        # Validate everything before creating any output. Exclusive creation
        # also rejects a destination created after the check above.
        args.output.mkdir(parents=True, exist_ok=False)
        for frame, (data, _) in zip(FRAMES, prepared):
            with (args.output / f'native-mask-{frame:05d}.bin').open('xb') as target:
                target.write(data)
        reports = [report for _, report in prepared]
        with (args.output / 'native-mask-provenance.json').open('x') as target:
            target.write(json.dumps(dict(scope=SCOPE, states=reports), indent=2) + '\n')
        print(json.dumps(dict(states=len(reports),
                              window_copies=sum(item['window_copies'] for item in reports))))
    except (OSError, ValueError) as error:
        parser.error(str(error))


if __name__ == '__main__':
    main()
