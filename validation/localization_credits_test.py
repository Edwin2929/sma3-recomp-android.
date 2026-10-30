#!/usr/bin/env python3
"""Validate generated credit packets/font against private verified inputs.

Run the localization generator first. Optional previews render the actual
generated glyph cells for inspection; they are not native gameplay captures.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from localization_credits import (FONT_ADDRESS, FONT_BYTES, RESOURCE_BASE,
    RESOURCE_POINTERS, TRANSLATED_IDS, WIDTHS_ADDRESS, read_translations)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', type=Path, required=True)
    parser.add_argument('--disasm', type=Path, required=True)
    parser.add_argument('--header', type=Path, default=ROOT / 'src/localization_data.h')
    parser.add_argument('--legacy-header', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--preview', type=Path)
    args = parser.parse_args()
    checks = 0

    def check(ok, message):
        nonlocal checks
        checks += 1
        if not ok:
            raise AssertionError(message)

    rom = args.rom.read_bytes()
    check(hashlib.sha1(rom).hexdigest() == '7352d2bd064d9ebaec579e264228aa21c7345b80', 'ROM revision')
    header = args.header.read_text()
    coverage = json.loads((ROOT / 'localization/coverage.json').read_text())

    def byte_array(text, name):
        match = re.search(r'uint8_t ' + name + r'\[\] = \{([^}]*)\};', text, re.S)
        check(match is not None, 'Missing generated array: ' + name)
        return bytes(int(value) for value in re.findall(r'\d+', match[1]))

    def pointer_array(text, name):
        match = re.search(r'\b' + name + r'\[\] = \{(.*?)\};', text, re.S)
        check(match is not None, 'Missing pointer array: ' + name)
        return [tuple(int(value, 16) for value in row.split(','))
                for row in re.findall(r'\{([^}]+)\}', match[1])]

    resources = byte_array(header, 'credits_resources')
    check(len(resources) == FONT_BYTES + 128, 'Credit resource size')
    font, widths = resources[:FONT_BYTES], resources[FONT_BYTES:]
    original_font = rom[FONT_ADDRESS - 0x08000000:FONT_ADDRESS - 0x08000000 + FONT_BYTES]
    original_widths = rom[WIDTHS_ADDRESS - 0x08000000:WIDTHS_ADDRESS - 0x08000000 + 128]
    check(tuple(pointer_array(header, 'credits_resource_pointers')) == RESOURCE_POINTERS,
          'Only the two verified resource literals are redirected')
    for slot, original, target in RESOURCE_POINTERS:
        check(struct.unpack_from('<I', rom, slot - 0x08000000)[0] == original, 'Literal source')
        check(RESOURCE_BASE <= target < RESOURCE_BASE + len(resources), 'Literal target range')

    extensions = {g['code']: g for g in coverage['credits']['glyphs']}
    changed_glyphs = set()
    for code in range(0x80, 0x100):
        start = (code - 0x80) * 12
        glyph = font[start:start + 12]
        original = original_font[start:start + 12]
        if glyph != original:
            changed_glyphs.add(code)
        if code not in extensions:
            check(glyph == original, 'Unassigned original glyph preserved')
            check(widths[code - 0x80] == original_widths[code - 0x80], 'Unassigned advance preserved')
        else:
            ink_right = max((x + 1 for row in glyph for x in range(8) if row & (0x80 >> x)), default=0)
            check(0 < ink_right <= widths[code - 0x80], 'Accent fits its advance without overlap')
            check(ink_right == extensions[code]['ink_right'], 'Reported accent extent')
    check(changed_glyphs == set(extensions), 'Only assigned extension glyphs change')

    mapping = {}
    for line in (args.disasm / 'sma3char.tbl').read_text().splitlines():
        if '=' in line:
            key, value = line.split('=', 1)
            mapping[value] = bytes.fromhex(key)
    mapping.update({'ã': b'\x60', 'õ': b'\x61', 'Ã': b'\x62', 'Õ': b'\x63'})
    mapping['.'] = mapping[r'\{. }']
    mapping[','] = mapping[r'\{, }']

    def encode(text):
        output = bytearray()
        while text:
            token = next((token for token in sorted(mapping, key=len, reverse=True) if text.startswith(token)), None)
            check(token is not None, 'Source character has an encoding')
            output += mapping[token]
            text = text[len(token):]
        return bytes(output)

    source = (args.disasm / 'asm/Text/Credits.asm').read_text()
    originals = {}
    for match in re.finditer(r'^(CreditsText[0-9A-F]{2}):\s*; ([0-9A-F]{8})\n(.*?)(?=^CreditsText|\Z)', source, re.M | re.S):
        packet = bytearray()
        for line in match[3].splitlines():
            for token in re.finditer(r'"([^"]*)"|0x([0-9A-Fa-f]{2})', line):
                packet += encode(token[1]) if token[1] is not None else bytes((int(token[2], 16),))
        address = int(match[2], 16)
        check(rom[address - 0x08000000:address - 0x08000000 + len(packet)] == packet, 'Original source packet')
        originals[match[1]] = (address, bytes(packet))
    check(len(originals) == 39, 'All original credit packets checked')

    def rows(packet):
        cursor = 0
        result = []
        for row in range(2):
            check(packet[cursor:cursor + 2] == b'\xff\x01', 'Row command 01')
            control = packet[cursor + 2]
            check(packet[cursor + 3:cursor + 5] == b'\xff\x02', 'Row command 02')
            y = packet[cursor + 5]
            check(packet[cursor + 6:cursor + 8] == b'\xff\x03', 'Row command 03')
            x = packet[cursor + 8]
            cursor += 9
            end = packet.index(255, cursor)
            result.append((control, y, x, packet[cursor:end]))
            cursor = end
        check(packet[cursor:] == b'\xff\xff', 'Single packet terminator')
        return result

    banks = [byte_array(header, 'spanish_text'), byte_array(header, 'portuguese_text')]
    pointers = pointer_array(header, 'text_pointers')
    pointer_targets = {row[1]: row[2:] for row in pointers}
    translated = read_translations(ROOT / 'localization/credits.tsv')
    check(translated['1A'][0][0] == translated['1A'][1][0] == 'SRD', 'SRD attribution retained')
    check(translated['23'][0][0] == translated['23'][1][0] == 'NINTENDO.', 'Nintendo attribution retained')
    original_used = set()
    rendered = []
    for label, (address, original) in originals.items():
        original_rows = rows(original)
        if int(label[-2:], 16) < 0x24:
            original_used.update(code for row in original_rows for code in row[3])
        if label[-2:] not in TRANSLATED_IDS:
            check(address not in pointer_targets, 'Name/blank packet keeps its original pointer')
            continue
        check(address in pointer_targets, 'Translated packet has a verified pointer')
        for language, target in enumerate(pointer_targets[address]):
            offset = target - (0x09E00000 + language * 0x100000)
            end = banks[language].index(b'\xff\xff', offset) + 2
            packet_rows = rows(banks[language][offset:end])
            special = int(label[-2:], 16) < 0x24
            for old, new in zip(original_rows, packet_rows):
                check(old[:2] == new[:2], 'Native row commands and positions retained')
                check(bool(old[3]) == bool(new[3]), 'Native blank rows retained')
                advance = sum(widths[code - 0x80] if special else rom[0x2F62CC + code] for code in new[3])
                check(new[2] == (240 - advance) // 2, 'Row centered using the selected font')
                check(new[2] >= (4 if special else 8) and new[2] + advance <= 236,
                      'Row advances stay inside the native 240-pixel strip')
            rendered.append((label, language, special, packet_rows))
    check(len(rendered) == 40, 'Twenty translated packets in both languages')
    check(all(code >= 0xA0 for code in original_used), 'Original English never uses extension slots')
    check(len(original_used) == coverage['credits']['original_english_glyphs_preserved'], 'Original glyph count')

    legacy = None
    if args.legacy_header:
        raw = args.legacy_header.read_bytes()
        check(hashlib.sha256(raw).hexdigest() == 'a41582d5a4502b18923d152044c7be7f6c8ac58950a1fee2c2fdb54705de4a3b',
              'Legacy header matches the recorded 0.10.14 build input')
        old = raw.decode()
        old_pointers = pointer_array(old, 'text_pointers')
        current = {row[0]: row for row in pointers}
        for row in old_pointers:
            check(current.get(row[0]) == row, 'Legacy pointer and packet address preserved')
        prefix_sizes = []
        for name, current_bank in zip(('spanish_text', 'portuguese_text'), banks):
            prefix = byte_array(old, name)
            check(current_bank[:len(prefix)] == prefix, 'Legacy message bank prefix remains byte-identical')
            prefix_sizes.append(len(prefix))
        check(byte_array(old, 'portuguese_glyphs') == byte_array(header, 'portuguese_glyphs'), 'Legacy Portuguese glyphs')
        legacy = {'pointer_slots_identical': len(old_pointers), 'bank_prefix_bytes_identical': prefix_sizes}

    text = (ROOT / 'localization/credits.tsv').read_text()
    with tempfile.TemporaryDirectory() as folder:
        bad = Path(folder) / 'credits.tsv'
        for damaged in (text + text.splitlines()[0] + '\n', '\n'.join(text.splitlines()[1:]) + '\n',
                        text.replace('Créditos|', r'\{heartL0}Créditos|', 1)):
            bad.write_text(damaged)
            try:
                read_translations(bad)
            except ValueError:
                check(True, 'Malformed translation rejected')
            else:
                check(False, 'Malformed translation was accepted')

    if args.preview:
        from PIL import Image, ImageDraw
        # Pixel-for-pixel 8x12 glyph plotting; magnification only for inspection.
        main_font = bytearray(rom[0x2F63CC:0x2F63CC + 256 * 12])
        main_font[0x60 * 12:0x64 * 12] = byte_array(header, 'portuguese_glyphs')
        canvas = Image.new('RGB', (496, len(TRANSLATED_IDS) * 47 + 26), '#eeeeee')
        draw = ImageDraw.Draw(canvas)
        draw.text((8, 4), 'Español · paquetes generados', fill='black')
        draw.text((256, 4), 'Português · pacotes gerados', fill='black')
        for label, language, special, packet_rows in rendered:
            group = TRANSLATED_IDS.index(label[-2:])
            left, top = language * 248 + 4, 22 + group * 47
            draw.text((left, top), label[-2:], fill='#555555')
            for _, y, x, encoded in packet_rows:
                for code in encoded:
                    start = (code - 0x80) * 12 if special else code * 12
                    glyph = font[start:start + 12] if special else main_font[start:start + 12]
                    for gy, bits in enumerate(glyph):
                        for gx in range(8):
                            if bits & (0x80 >> gx):
                                check(0 <= x + gx < 240, 'Rendered glyph pixel stays within strip')
                                draw.point((left + x + gx, top + 13 + y * 2 + gy), fill='black')
                    x += widths[code - 0x80] if special else rom[0x2F62CC + code]
        args.preview.parent.mkdir(parents=True, exist_ok=True)
        canvas.resize((992, canvas.height * 2), Image.Resampling.NEAREST).save(args.preview)

    report = {'result': 'PASS', 'scope': 'Generator/font/resource validation; not native gameplay or physical Android',
              'checks': checks, 'original_packets': 39, 'translated_packets_per_language': 20,
              'translated_rows_checked': 80, 'accent_glyphs': len(extensions),
              'english_glyphs_identical': len(original_used), 'resource_bytes': len(resources),
              'legacy_compatibility': legacy,
              'preview_scope': 'Direct glyph-cell rendering of generated packets, not a gameplay screenshot'}
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
