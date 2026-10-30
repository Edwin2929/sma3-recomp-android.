"""Build private localized UI resources from a verified SMA3 USA ROM.

The original guest renderer, BIOS decompressor and numeric fields still run.
Only four verified Thumb LDR literal values select immutable resource copies.
This module stores no extracted Nintendo graphics in the public repository.
"""
from pathlib import Path
import hashlib
import re
import struct

ROM_SHA1 = '7352d2bd064d9ebaec579e264228aa21c7345b80'
GRAPHICS_BASES = (0x09C00000, 0x09C80000)
GRAPHICS_BANK_LIMIT = 0x80000
FILE_GRAPHICS = 0x081A42D8
FILE_GRAPHICS_SIZE = 3584
FILE_GRAPHICS_COMPRESSED_SHA256 = 'f7fdc9340408298ab621b972c21307548c1b92071ff1e803cf92a0201fbceccf'
FILE_GRAPHICS_SHA256 = '6608847af089d64027ca6a5f137f6b7266180b0a645113875b12c9b6feb0d191'
FONT_SHA256 = 'a0b4f26c60cc708afc3542e2054c4fce0814a2d2733e14f7ff66049445a18760'

# Name, original resource, length, SHA-256, Thumb LDR PC and literal slot.
PAUSE_TABLES = (
    ('inventory_total', 0x08194868, 24,
     '75f2f484136a53a00484b8b041ef4eb800eb5c7ae705641ec211cc503b74b70b',
     0x080E731C, 0x080E75C0, 'InvScr_TotalPointsChars'),
    ('inventory_best', 0x08194880, 24,
     'e9f2014a169913ab38b5ea42cdc9eee59523a3feb383ac868c234b903ba6fc4c',
     0x080E7346, 0x080E75D4, 'InvScr_WHighScoreChars'),
    ('inventory_world', 0x08194898, 24,
     '3e82666948e98396bae545a28141fb36da71de76799241f9c2d9f68a8d144c55',
     0x080E7370, 0x080E75E0, 'InvScr_WorldNumChars'),
)

# Tile indices within the decompressed resource. Both rows retain exactly
# the tilemap and geometry of Data08199370 (file/copy/erase header widgets).
FILE_REGIONS = (
    ('file_header', tuple(range(0x16, 0x1F)), tuple(range(0x26, 0x2F))),
    ('copy_header', (0x1F, 0x20, 0x21) + tuple(range(0x50, 0x58)),
     (0x2F, 0x30, 0x31) + tuple(range(0x60, 0x68))),
    ('erase_header', tuple(range(0x35, 0x40)), tuple(range(0x45, 0x50))),
)


def require(condition, message):
    if not condition:
        raise ValueError(message)


def lz77_decode(data, offset=0):
    """Return a bounded GBA type-10 payload and its compressed byte count."""
    require(0 <= offset <= len(data) - 4 and data[offset] == 0x10, 'Invalid LZ77 header')
    size = int.from_bytes(data[offset + 1:offset + 4], 'little')
    require(0 < size <= 0x20000, 'Invalid LZ77 output size')
    cursor = offset + 4
    output = bytearray()
    while len(output) < size:
        require(cursor < len(data), 'Truncated LZ77 flags')
        flags = data[cursor]
        cursor += 1
        for bit in range(7, -1, -1):
            if len(output) == size:
                break
            if flags & (1 << bit):
                require(cursor + 2 <= len(data), 'Truncated LZ77 reference')
                first, second = data[cursor:cursor + 2]
                cursor += 2
                length = (first >> 4) + 3
                distance = ((first & 15) << 8 | second) + 1
                require(distance <= len(output), 'Invalid LZ77 backward distance')
                require(len(output) + length <= size, 'LZ77 reference exceeds output size')
                for _ in range(length):
                    output.append(output[-distance])
            else:
                require(cursor < len(data), 'Truncated LZ77 literal')
                output.append(data[cursor])
                cursor += 1
    return bytes(output), cursor - offset


def lz77_encode(data):
    """Deterministic type-10 encoding; never changes the decoded resource size."""
    require(0 < len(data) <= 0x20000, 'Invalid LZ77 input size')
    output = bytearray(b'\x10' + len(data).to_bytes(3, 'little'))
    cursor = 0
    while cursor < len(data):
        flag_at = len(output)
        output.append(0)
        for bit in range(7, -1, -1):
            if cursor == len(data):
                break
            begin = max(0, cursor - 4096)
            window = data[begin:cursor]
            match = -1
            length = min(18, len(data) - cursor)
            while length >= 3:
                match = window.rfind(data[cursor:cursor + length])
                if match >= 0:
                    break
                length -= 1
            if match >= 0:
                distance = cursor - (begin + match) - 1
                output[flag_at] |= 1 << bit
                output.extend((((length - 3) << 4) | (distance >> 8), distance & 255))
                cursor += length
            else:
                output.append(data[cursor])
                cursor += 1
    # BIOS reads a word-aligned source; keep the final allocation aligned too.
    output.extend(b'\0' * (-len(output) % 4))
    return bytes(output)


def _bytes(rom, address, size):
    offset = address - 0x08000000
    require(0 <= offset <= len(rom) - size, 'Resource outside verified ROM')
    return rom[offset:offset + size]


def _check_literal(rom, disasm_text, pc, slot, resource, label):
    opcode = int.from_bytes(_bytes(rom, pc, 2), 'little')
    require(opcode & 0xF800 == 0x4800, 'Expected Thumb literal LDR')
    require(((pc + 4) & ~3) + (opcode & 255) * 4 == slot, 'LDR literal address differs')
    require(int.from_bytes(_bytes(rom, slot, 4), 'little') == resource, 'Literal resource differs')
    pattern = r'^ldr\s+r0,=' + re.escape(label) + r'\s*; ' + f'{pc:08X}' + r'(?:\s|$)'
    require(re.search(pattern, disasm_text, re.M) is not None, 'Disassembly LDR does not match ROM')


def _check_label(data_asm, label, address):
    pattern = r'^' + re.escape(label) + r':\s*; ' + f'{address:08X}' + r'\s*$'
    require(re.search(pattern, data_asm, re.M) is not None, 'Disassembly resource label differs')


def _glyph_table(path):
    result = {}
    for line in path.read_text().splitlines():
        if '=' in line:
            key, value = line.split('=', 1)
            code = bytes.fromhex(key)
            if len(code) == 1 and len(value) == 1:
                result[value] = code[0]
    return result


def _translations(path):
    result = {}
    for line in path.read_text().splitlines():
        key, es, pt = line.split('\t')
        require(key not in result and es and pt, 'Duplicate or empty graphical translation')
        result[key] = (es, pt)
    require(set(result) == {r[0] for r in FILE_REGIONS} | {r[0] for r in PAUSE_TABLES},
            'Graphical translation catalog differs from supported labels')
    return result


def pixel(data, tile, x, y):
    return data[tile * 32 + y * 4 + x // 2] >> ((x & 1) * 4) & 15


def _put_pixel(data, tile, x, y, value):
    address = tile * 32 + y * 4 + x // 2
    shift = (x & 1) * 4
    data[address] = (data[address] & ~(15 << shift)) | (value << shift)


def _draw_header(resource, upper, lower, text, codes, font, widths):
    tiles_wide = len(upper)
    require(tiles_wide == len(lower), 'Inconsistent graphical text rows')
    glyphs = [codes[c] for c in text]
    advance = sum(widths[c] for c in glyphs)
    require(0 < advance <= tiles_wide * 8 - 4, 'Graphical translation too wide')
    for y in range(14):
        row = upper if y < 8 else lower
        for tile in row:
            for x in range(8):
                # This widget uses palette index8 as its uniform fill and9
                # for all letter pixels. Its two-row index12 border is kept.
                require(pixel(resource, tile, x, y % 8) in (8, 9), 'Unexpected header pixel')
                _put_pixel(resource, tile, x, y % 8, 8)
    cursor = (tiles_wide * 8 - advance) // 2
    painted = 0
    for code in glyphs:
        for y, bits in enumerate(font[code * 12:(code + 1) * 12]):
            for x in range(8):
                if bits & (0x80 >> x):
                    target = cursor + x
                    require(0 <= target < tiles_wide * 8 and y < 14, 'Clipped graphical glyph')
                    row = upper if y < 8 else lower
                    _put_pixel(resource, row[target // 8], target % 8, y % 8, 9)
                    painted += 1
        cursor += widths[code]
    return {'text': text, 'advance_px': advance, 'available_px': tiles_wide * 8,
            'painted_pixels': painted, 'border_preserved': True}


def _pause_codes(text):
    require(all('A' <= c <= 'Z' or c == ' ' for c in text),
            'Inventory font only supports verified uppercase letters and spaces')
    return bytes(0x4E if c == ' ' else (ord(c) - ord('A')) * 2 for c in text)


def _pause_table(original, name, text):
    result = bytearray(original)
    encoded = _pause_codes(text)
    if name == 'inventory_total':
        require(len(encoded) <= 18, 'Total label overlaps score fields')
        result[:18] = encoded + bytes([0x36]) * (18 - len(encoded))
        # Retain the original score/counter holes and units after this label.
    elif name == 'inventory_best':
        require(len(encoded) <= 10, 'Best label overlaps score fields')
        result[0] = _pause_codes('M')[0]  # World -> Mundo, both ES and PT.
        result[5:15] = encoded + bytes([0x36]) * (10 - len(encoded))
        require(result[1:5] == original[1:5], 'World/level number holes changed')
    else:
        require(len(encoded) == 5, 'World label overlaps its following number')
        result[:5] = encoded
        require(result[5:] == original[5:], 'World number or score fields changed')
    require(len(result) == 24 and result[18:] == original[18:], 'Numeric tail changed')
    return bytes(result)


def build_graphics(rom, disasm, root):
    """Return two private banks, four verified pointer tuples and audit metadata."""
    disasm, root = Path(disasm), Path(root)
    require(hashlib.sha1(rom).hexdigest() == ROM_SHA1, 'Unsupported ROM revision')
    data_asm = (disasm / 'asm/Data.asm').read_text()
    title_asm = (disasm / 'asm/YITitle.asm').read_text()
    pause_asm = (disasm / 'asm/InventoryScreen.asm').read_text()
    translations = _translations(root / 'localization/graphics.tsv')
    codes = _glyph_table(disasm / 'sma3char.tbl')
    font = _bytes(rom, 0x082F63CC, 256 * 12)
    widths = _bytes(rom, 0x082F62CC, 256)
    require(hashlib.sha256(font).hexdigest() == FONT_SHA256, 'Main font differs')
    _check_label(data_asm, 'YITitle_L0_Graphics_LZ77', FILE_GRAPHICS)
    _check_literal(rom, title_asm, 0x080FE7BE, 0x080FE930,
                   FILE_GRAPHICS, 'YITitle_L0_Graphics_LZ77')
    original_graphics, consumed = lz77_decode(rom, FILE_GRAPHICS - 0x08000000)
    require(len(original_graphics) == FILE_GRAPHICS_SIZE, 'File graphic size differs')
    require(hashlib.sha256(original_graphics).hexdigest() == FILE_GRAPHICS_SHA256,
            'File graphic pixels differ')
    compressed = _bytes(rom, FILE_GRAPHICS, consumed)
    require(hashlib.sha256(compressed).hexdigest() == FILE_GRAPHICS_COMPRESSED_SHA256,
            'File graphic compressed stream differs')
    # Validate that the existing tilemap uses exactly these two rows, with no
    # flipped tiles, inside its three 15x3 header widgets.
    tilemap = struct.unpack('<135H', _bytes(rom, 0x08199370, 270))
    for widget, (_, upper, lower) in enumerate(FILE_REGIONS):
        for row, tiles in ((1, upper), (2, lower)):
            start = widget * 45 + row * 15 + (3 if widget == 0 else 2)
            actual = tilemap[start:start + len(tiles)]
            require(actual == tuple(0x300 + tile for tile in tiles), 'Header tilemap differs')
    banks = [bytearray(), bytearray()]
    pointers = []
    records = []

    def add(label, address, slot, payloads, details):
        targets = []
        for language, payload in enumerate(payloads):
            banks[language].extend(b'\0' * (-len(banks[language]) % 4))
            targets.append(GRAPHICS_BASES[language] + len(banks[language]))
            banks[language].extend(payload)
        pointers.append((slot, address, *targets))
        records.append({'label': label, 'original_address': hex(address),
                        'pointer_slot': hex(slot), 'bytes': [len(p) for p in payloads],
                        'sha256': [hashlib.sha256(p).hexdigest() for p in payloads],
                        **details})

    images = [bytearray(original_graphics), bytearray(original_graphics)]
    region_reports = []
    for key, upper, lower in FILE_REGIONS:
        report = {'label': key, 'upper_tiles': list(upper), 'lower_tiles': list(lower),
                  'languages': []}
        for language in range(2):
            report['languages'].append(_draw_header(images[language], upper, lower,
                translations[key][language], codes, font, widths))
        region_reports.append(report)
    allowed_bytes = {tile * 32 + (y % 8) * 4 + x // 2
                     for _, upper, lower in FILE_REGIONS
                     for y in range(14) for tile in (upper if y < 8 else lower)
                     for x in range(8)}
    for localized in images:
        require(all(a == b or i in allowed_bytes
                    for i, (a, b) in enumerate(zip(original_graphics, localized))),
                'Changed pixels outside graphical label regions')
    payloads = [lz77_encode(bytes(image)) for image in images]
    require(all(lz77_decode(p)[0] == bytes(im) for p, im in zip(payloads, images)),
            'Localized compression round trip failed')
    add('YITitle_L0_Graphics_LZ77', FILE_GRAPHICS, 0x080FE930, payloads,
        {'category': 'file_menu_headers', 'decoded_bytes': FILE_GRAPHICS_SIZE,
         'original_compressed_sha256': FILE_GRAPHICS_COMPRESSED_SHA256,
         'original_decoded_sha256': FILE_GRAPHICS_SHA256,
         'regions': region_reports, 'outside_regions_identical': True,
         'palette_entries_unchanged': True, 'writer': 'YITitle_LoadGraphics / 080FE7BE'})
    for name, address, size, expected, pc, slot, label in PAUSE_TABLES:
        _check_label(data_asm, label, address)
        _check_literal(rom, pause_asm, pc, slot, address, label)
        original = _bytes(rom, address, size)
        require(hashlib.sha256(original).hexdigest() == expected, 'Inventory character table differs')
        payloads = [_pause_table(original, name, text) for text in translations[name]]
        add(label, address, slot, payloads,
            {'category': 'inventory_headers', 'texts': list(translations[name]),
             'original_sha256': expected, 'numeric_fields_preserved': True,
             'writer': f'Sub080E71B8 / {pc:08X}'})
    require(all(len(b) < GRAPHICS_BANK_LIMIT for b in banks), 'Graphics bank exceeds its region')
    pointers.sort()
    require(len(pointers) == 4 and len({x[0] for x in pointers}) == 4, 'Unexpected graphics pointers')
    return {'blobs': [bytes(b) for b in banks], 'pointers': pointers, 'records': records,
            'labels_per_language': len(translations), 'pointer_slots': len(pointers),
            'bases': list(GRAPHICS_BASES), 'bytes': [len(b) for b in banks],
            'scope': 'Three file headers and three inventory labels; other graphical labels remain unchanged'}
