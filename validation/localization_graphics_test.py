#!/usr/bin/env python3
"""Check translated UI resources using private, verified ROM/disassembly inputs.

No native gameplay is executed. The checks cover resource shape, untouched
pixels, border rows, four actual Thumb literal loads and numeric field holes.
"""
import argparse
import hashlib
import json
from pathlib import Path
import random
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import localization_graphics as graphics


def reference_decode(data):
    """Independent bit-mask walk used to check the generator's encoded output."""
    size = int.from_bytes(data[:4], 'little') >> 8
    source = iter(data[4:])
    result = []
    while len(result) < size:
        control = next(source)
        mask = 128
        while mask and len(result) < size:
            first = next(source)
            if control & mask:
                second = next(source)
                back = ((first & 15) * 256 + second) + 1
                count = first // 16 + 3
                for _ in range(count):
                    result.append(result[len(result) - back])
            else:
                result.append(first)
            mask //= 2
    if len(result) != size:
        raise AssertionError('Reference output size differs')
    return bytes(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', required=True, type=Path)
    parser.add_argument('--disasm', required=True, type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    rom = args.rom.read_bytes()
    result = graphics.build_graphics(rom, args.disasm, ROOT)
    repeat = graphics.build_graphics(rom, args.disasm, ROOT)
    assert result == repeat
    checks = 1
    # Known literal and overlapping-reference streams establish decoder behavior.
    assert graphics.lz77_decode(b'\x10\x03\x00\x00\x00ABC')[0] == b'ABC'
    assert graphics.lz77_decode(b'\x10\x06\x00\x00\x40A\x20\x00')[0] == b'AAAAAA'
    checks += 2
    for malformed in (b'', b'\x11\x01\0\0\0A', b'\x10\x01\0\0\0',
                      b'\x10\x03\0\0\x80\0\0', b'\x10\x02\0\0\x40A\0\0'):
        try:
            graphics.lz77_decode(malformed)
        except ValueError:
            checks += 1
        else:
            raise AssertionError('Malformed LZ77 accepted')
    rng = random.Random(0x015)
    cases = [bytes([n]) * n for n in (1, 2, 3, 17, 18, 19, 255)]
    cases += [bytes(rng.randrange(256) for _ in range(n))
              for n in (2, 31, 256, 4095, 4096, 4127)]
    cases += [bytes(range(256)) * 20, b'ABCD' * 2048]
    for data in cases:
        encoded = graphics.lz77_encode(data)
        assert len(encoded) % 4 == 0
        assert graphics.lz77_decode(encoded)[0] == data == reference_decode(encoded)
        checks += 3
    original, _ = graphics.lz77_decode(rom, graphics.FILE_GRAPHICS - 0x08000000)
    allowed = set()
    for _, upper, lower in graphics.FILE_REGIONS:
        for row, tiles in enumerate((upper, lower)):
            for tile in tiles:
                for y in range(8):
                    if row * 8 + y < 14:
                        allowed.update(tile * 32 + y * 4 + byte for byte in range(4))
    resources = 0
    exact_untouched_bytes = 0
    preserved_border_pixels = 0
    for language, base in enumerate(graphics.GRAPHICS_BASES):
        bank = result['blobs'][language]
        for pointer in result['pointers']:
            slot, expected, es, pt = pointer
            target = (es, pt)[language]
            assert target % 4 == 0 and base <= target < base + len(bank)
            assert int.from_bytes(rom[slot - 0x08000000:slot - 0x08000000 + 4], 'little') == expected
            checks += 2
            if expected == graphics.FILE_GRAPHICS:
                localized, _ = graphics.lz77_decode(bank, target - base)
                assert localized == reference_decode(bank[target - base:])
                for i, (before, after) in enumerate(zip(original, localized)):
                    if i not in allowed:
                        assert before == after
                        exact_untouched_bytes += 1
                for _, _, lower in graphics.FILE_REGIONS:
                    for tile in lower:
                        for y in (6, 7):
                            for x in range(8):
                                assert graphics.pixel(localized, tile, x, y) == 12
                                preserved_border_pixels += 1
                checks += 1
            else:
                source = rom[expected - 0x08000000:expected - 0x08000000 + 24]
                translated = bank[target - base:target - base + 24]
                assert len(translated) == len(source) == 24 and translated[18:] == source[18:]
                if expected == 0x08194880:
                    assert translated[1:5] == source[1:5]
                if expected == 0x08194898:
                    assert translated[5:] == source[5:]
                checks += 1
            resources += 1
    corrupted = bytearray(rom)
    corrupted[0] ^= 1
    try:
        graphics.build_graphics(corrupted, args.disasm, ROOT)
    except ValueError:
        checks += 1
    else:
        raise AssertionError('Unverified ROM accepted')
    report = {key: value for key, value in result.items() if key not in ('blobs', 'pointers')}
    report.update(result='PASS', checks=checks, resources_compared=resources,
                  exact_untouched_graphics_bytes=exact_untouched_bytes,
                  preserved_border_pixels=preserved_border_pixels,
                  determinism=True, compression_cases=len(cases),
                  pointer_slots=result['pointer_slots'],
                  rom_sha1=hashlib.sha1(rom).hexdigest(),
                  source_sha256={str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                      for p in (ROOT / 'tools/localization_graphics.py',
                                ROOT / 'localization/graphics.tsv')},
                  limits=['Resource-level checks only; native UI replay and physical Android remain separate.'])
    if args.output:
        args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({key: report[key] for key in ('result', 'checks', 'labels_per_language',
          'pointer_slots', 'bytes', 'resources_compared', 'exact_untouched_graphics_bytes',
          'preserved_border_pixels')}))


if __name__ == '__main__':
    main()
