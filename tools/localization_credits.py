"""Build translated credit packets and an immutable copy of their font.

The original credit renderer uses a different 128-character font for entries
00..23; it subtracts 0x80 from character indices. The main-font accent codes
are therefore unsafe there. Entries 24..26 use the existing main font.
No executable or original ROM resource is modified by this generator.
"""
from dataclasses import dataclass
import hashlib
from pathlib import Path
import re
import struct
import unicodedata

ROM_SHA1 = '7352d2bd064d9ebaec579e264228aa21c7345b80'
FONT_ADDRESS = 0x0816D509
WIDTHS_ADDRESS = 0x0816D489
RESOURCE_BASE = 0x09D00000
FONT_BYTES = 128 * 12
RESOURCE_POINTERS = (
    (0x08033D80, FONT_ADDRESS, RESOURCE_BASE),
    (0x08033E58, WIDTHS_ADDRESS, RESOURCE_BASE + FONT_BYTES),
)
TRANSLATED_IDS = (
    '00', '01', '04', '06', '09', '0E', '11', '13', '16', '18',
    '19', '1A', '1C', '1E', '21', '22', '23', '24', '25', '26',
)
HEART_LEFT = r'\{heartL0}\{heartL1}'
HEART_RIGHT = r'\{heartR0}\{heartR1}'


@dataclass
class CreditsResult:
    records: dict
    resources: bytes
    pointers: tuple
    report: dict


def _require(condition, message):
    if not condition:
        raise ValueError(message)


def _label_address(data, name):
    match = re.search(r'^' + re.escape(name) + r':\s*;\s*([0-9A-F]{8})$', data, re.M)
    _require(match is not None, 'Missing source label: ' + name)
    return int(match[1], 16)


def _original_rows(source):
    rows = []
    for line in source.splitlines()[1:]:
        if not line.startswith('.strn'):
            continue
        match = re.fullmatch(
            r'\.strn 0xFF,0x01,0x([0-9A-F]{2}),0xFF,0x02,0x([0-9A-F]{2}),'
            r'0xFF,0x03,0x([0-9A-F]{2})(?:,"([^"]*)")?', line)
        _require(match is not None, 'Unexpected credits row: ' + line)
        rows.append((int(match[1], 16), int(match[2], 16),
                     int(match[3], 16), match[4] or ''))
    _require(len(rows) == 2, 'Credits packet must retain exactly two rows')
    return rows


def read_translations(path):
    translated = {}
    for line_number, line in enumerate(Path(path).read_text().splitlines(), 1):
        fields = line.split('\t')
        _require(len(fields) == 3, f'credits.tsv:{line_number}: expected ID, ES, PT')
        key, *texts = fields
        _require(key in TRANSLATED_IDS and key not in translated,
                 'Unknown or repeated credit ID: ' + key)
        languages = [text.split('|') for text in texts]
        _require(all(len(rows) == 2 for rows in languages),
                 'Each credit translation needs two explicit rows: ' + key)
        _require(all('\\' not in row and '@{' not in row for rows in languages for row in rows),
                 'Translations cannot introduce renderer commands: ' + key)
        translated[key] = languages
    _require(set(translated) == set(TRANSLATED_IDS), 'Missing translated credit IDs')
    return translated


def _accented_glyph(character, original_font, encode):
    decomposed = unicodedata.normalize('NFD', character)
    _require(len(decomposed) == 2, 'Unsupported credit character: ' + character)
    base, accent = decomposed
    _require(base in 'aceinou', 'Accent needs a reviewed credit glyph: ' + character)
    encoded = encode(base)
    _require(len(encoded) == 1 and encoded[0] >= 0xA0, 'Invalid base glyph')
    start = (encoded[0] - 0x80) * 12
    glyph = bytearray(original_font[start:start + 12])
    if accent == '\u0327' and base == 'c':
        # This handwritten font reaches the last row. Shorten two repeated
        # middle stem rows to keep a visible cedilla inside its 12-row cell.
        _require(glyph[8] == glyph[9] and glyph[8] != 0, 'Unexpected c stem shape')
        glyph = glyph[:8] + glyph[10:] + bytearray((0x10, 0x30))
    else:
        _require(accent in ('\u0300', '\u0301', '\u0302', '\u0303'),
                 'Unsupported credit accent: ' + character)
        if base == 'i':
            glyph[:4] = b'\0' * 4  # The accent replaces the dot.
        _require(not any(glyph[:3]), 'Credit accent has no headroom: ' + character)
        marks = {
            '\u0300': (0x30, 0x18), '\u0301': (0x0C, 0x18),
            '\u0302': (0x18, 0x24), '\u0303': (0x36, 0x6C),
        }
        glyph[1], glyph[2] = marks[accent]
    _require(len(glyph) == 12, 'Credit glyph cell changed size')
    return bytes(glyph), encoded[0]


def build_credits(rom, disasm, translation_path, sources, encode, packet_asm):
    _require(hashlib.sha1(rom).hexdigest() == ROM_SHA1, 'Unsupported SMA3 ROM')
    data = (Path(disasm) / 'asm/Data.asm').read_text()
    _require(_label_address(data, 'Credits_CharWidths') == WIDTHS_ADDRESS,
             'Credits widths address changed')
    _require(_label_address(data, 'Font_credits_1bpp_Graphics') == FONT_ADDRESS,
             'Credits font address changed')
    width_section = data.split('Credits_CharWidths:', 1)[1].split('Font_credits_', 1)[0]
    expected_widths = bytes(int(value, 16) for line in width_section.splitlines()
                            if line.startswith('.d8')
                            for value in re.findall(r'0x([0-9A-F]{2})', line))
    original_widths = rom[WIDTHS_ADDRESS - 0x08000000:WIDTHS_ADDRESS - 0x08000000 + 128]
    _require(len(expected_widths) == 128 and expected_widths == original_widths,
             'Credits width table differs from source')
    original_font = rom[FONT_ADDRESS - 0x08000000:FONT_ADDRESS - 0x08000000 + FONT_BYTES]
    _require(len(original_font) == FONT_BYTES, 'Truncated credits font')
    for slot, original, _ in RESOURCE_POINTERS:
        _require(struct.unpack_from('<I', rom, slot - 0x08000000)[0] == original,
                 'Credits resource literal changed')
        found = [0x08000000 + offset for offset in range(0, len(rom) - 3, 4)
                 if struct.unpack_from('<I', rom, offset)[0] == original]
        _require(found == [slot], 'Unexpected additional credits resource reference')

    source_rows = {}
    original_characters = set()
    for index in range(0x27):
        key = f'{index:02X}'
        address, source = sources['CreditsText' + key]
        packet = packet_asm(source)
        offset = address - 0x08000000
        _require(rom[offset:offset + len(packet)] == packet, 'Credits source mismatch: ' + key)
        source_rows[key] = _original_rows(source)
        if index < 0x24:
            for _, _, _, text in source_rows[key]:
                original_characters.update(encode(text))
    _require(all(code >= 0xA0 for code in original_characters),
             'An original credit uses a character assigned to the extension')

    translated = read_translations(translation_path)
    accents = sorted({character for key, languages in translated.items() if int(key, 16) < 0x24
                      for rows in languages for row in rows for character in row
                      if len(encode(character)) == 1 and encode(character)[0] < 0x80})
    _require(len(accents) <= 32, 'Credits font extension exhausted')
    font = bytearray(original_font)
    widths = bytearray(original_widths)
    mapping = {}
    glyph_report = []
    for character, code in zip(accents, range(0x80, 0xA0)):
        glyph, base = _accented_glyph(character, original_font, encode)
        mapping[character] = code
        font[(code - 0x80) * 12:(code - 0x80 + 1) * 12] = glyph
        widths[code - 0x80] = original_widths[base - 0x80]
        ink_right = max((x + 1 for row in glyph for x in range(8)
                         if row & (0x80 >> x)), default=0)
        _require(ink_right <= widths[code - 0x80],
                 'Credit accent extends beyond its character advance: ' + character)
        glyph_report.append({'character': character, 'code': code,
                             'base_code': base, 'advance': widths[code - 0x80],
                             'ink_right': ink_right})
    for code in original_characters:
        start = (code - 0x80) * 12
        _require(font[start:start + 12] == original_font[start:start + 12]
                 and widths[code - 0x80] == original_widths[code - 0x80],
                 'Original English credit glyph changed')

    main_widths = rom[0x2F62CC:0x2F63CC]
    records = {}
    layout_report = []
    for key, languages in translated.items():
        special_font = int(key, 16) < 0x24
        payloads = []
        for language, rows in zip(('es-419', 'pt-BR'), languages):
            packet = bytearray()
            for row_index, (text, source_row) in enumerate(zip(rows, source_rows[key])):
                control, y, _, original_text = source_row
                _require(bool(text) == bool(original_text), 'Credit row occupancy changed: ' + key)
                if original_text.startswith(HEART_LEFT):
                    _require(original_text.endswith(HEART_RIGHT), 'Unmatched credits decoration')
                    text = HEART_LEFT + text + HEART_RIGHT
                if special_font:
                    # Encode character by character while keeping the existing
                    # multi-byte table spellings for the four heart fragments.
                    marked = text
                    for character, code in mapping.items():
                        marked = marked.replace(character, f'\uE000{code:02x}\uE001')
                    encoded = bytearray()
                    for part in re.split(r'(\uE000[0-9a-f]{2}\uE001)', marked):
                        if part.startswith('\uE000'):
                            encoded.append(int(part[1:3], 16))
                        else:
                            encoded.extend(encode(part))
                    _require(all(code >= 0x80 for code in encoded), 'Unsafe credit character index')
                    width = sum(widths[code - 0x80] for code in encoded)
                else:
                    encoded = encode(text)
                    width = sum(main_widths[code] for code in encoded)
                # The credits strip is 240 pixels wide. Keep at least four
                # pixels at both sides for its wider handwritten font; the
                # later main-font narrative retains the eight-pixel margin.
                limit = 232 if special_font else 224
                _require(width <= limit, f'Credit row too wide: {key} {language} row {row_index}: {width}')
                x = (240 - width) // 2
                packet += bytes((255, 1, control, 255, 2, y, 255, 3, x)) + encoded
                layout_report.append({'id': key, 'language': language, 'row': row_index,
                                      'width': width, 'x': x, 'y': y,
                                      'font': 'credits' if special_font else 'main'})
            packet += b'\xff\xff'
            payloads.append(bytes(packet))
        records['CreditsText' + key] = payloads

    return CreditsResult(records, bytes(font + widths), RESOURCE_POINTERS, {
        'translated_packets': len(records), 'verified_original_packets': 39,
        'original_name_and_blank_packets_unchanged': 19,
        'source_font_sha256': hashlib.sha256(original_font).hexdigest(),
        'source_widths_sha256': hashlib.sha256(original_widths).hexdigest(),
        'resource_base': hex(RESOURCE_BASE), 'resource_bytes': len(font + widths),
        'original_english_glyphs_preserved': len(original_characters),
        'glyphs': glyph_report, 'rows': layout_report,
        'method': 'Copy font and widths into a virtual bank; extend codes unused by all original credit packets. Original ROM glyph resources remain unchanged.',
    })
