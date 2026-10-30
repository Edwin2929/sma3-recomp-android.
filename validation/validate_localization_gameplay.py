#!/usr/bin/env python3
"""Bounded TCP localization replay; images, save data and GBAS stay PRIVATE.

The native runner executes the ordinary game code. This driver only supplies
buttons, observes memory, and saves checkpoints. No game memory is patched.
"""
import argparse
import hashlib
import json
import re
import socket
import struct
import subprocess
import sys
import time
from pathlib import Path

from PIL import Image

from make_level_fixture import sections
from run_panorama_session import ROOT, session_environment

ASSET_SHA1 = {
    'rom': '7352d2bd064d9ebaec579e264228aa21c7345b80',
    'bios': '300c20df6731a33952ded8c436f7f186d25d3492',
}
REQUIRED_SECTIONS = {b'CPU0', b'BUS0', b'PPU0', b'IO_0', b'AUD0', b'SAV0', b'META'}
INVENTORY_STATE_SHA256 = 'f810aff15a8bda2b1187bcc8ca3ceac71793b0c0ec8e1c7b682d5421b3b823d4'

# Recorded from ordinary controls on the supported USA ROM. The file route
# starts with an empty, isolated save and enters/exits both management menus;
# no populated user file is copied or erased. The inventory route waits for
# the verified gameplay checkpoint's ongoing animation before pressing Select.
FILE_ACTIONS = [
    {'n': 300, 'keys': 1023}, {'n': 1, 'keys': 1015}, {'n': 90, 'keys': 1023},
    {'n': 1, 'keys': 1022}, {'n': 90, 'keys': 1023}, {'n': 1, 'keys': 1022},
    {'n': 240, 'keys': 1023}, {'n': 1, 'keys': 1015},
    {'n': 90, 'keys': 1023, 'capture': 'files'},
    *[action for _ in range(3) for action in
      ({'n': 1, 'keys': 1007}, {'n': 30, 'keys': 1023})],
    {'n': 1, 'keys': 1022}, {'n': 90, 'keys': 1023, 'capture': 'copy'},
    {'n': 1, 'keys': 1021}, {'n': 90, 'keys': 1023, 'capture': 'files-after-copy'},
    {'n': 1, 'keys': 895}, {'n': 30, 'keys': 1023},
    {'n': 1, 'keys': 1022}, {'n': 90, 'keys': 1023, 'capture': 'erase'},
    {'n': 1, 'keys': 1021}, {'n': 90, 'keys': 1023, 'capture': 'files-after-erase'},
]
INVENTORY_ACTIONS = [
    {'n': 92, 'keys': 1023, 'capture': 'gameplay'},
    {'n': 1, 'keys': 1019}, {'n': 90, 'keys': 1023, 'capture': 'inventory'},
    {'n': 1, 'keys': 1021}, {'n': 90, 'keys': 1023, 'capture': 'gameplay-resumed'},
]


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def coverage(folder):
    report = json.loads((folder / 'native-coverage.json').read_text())
    counters = re.findall(r'dispatch_misses=(\d+) interpreted_insns=(\d+) healed_native=(\d+)',
                          (folder / 'run.log').read_text())
    require(report['coverage'] == 'FULLY_STATIC', 'Native run was not FULLY_STATIC')
    require(all(report[key] == 0 for key in
                ('distinct_misses', 'interpreted_insns', 'healed_native', 'failed')),
            'Nonzero native coverage counter')
    require(report['misses'] == [] and counters and
            all(row == ('0', '0', '0') for row in counters),
            'Missing or nonzero native completion counters')
    fragment = folder / 'native-misses.toml.frag'
    require(not fragment.exists() or not fragment.read_text().strip(),
            'Unexpected native miss fragment')
    return {key: report[key] for key in ('coverage', 'distinct_misses',
                                        'interpreted_insns', 'healed_native', 'failed')}


class NativeSession:
    def __init__(self, runner, rom, bios, output, language, state=None):
        self.runner, self.rom, self.bios = (x.resolve() for x in (runner, rom, bios))
        self.output, self.language = output.resolve(), language
        self.state = state.resolve() if state else None
        self.config = ROOT / 'runtime.toml'
        for name in ('rom', 'bios'):
            require(hashlib.sha1(getattr(self, name).read_bytes()).hexdigest() == ASSET_SHA1[name],
                    'Unsupported private asset: ' + name)
        require(self.runner.is_file(), 'Missing native runner')
        require(language in (0, 1, 2), 'Language must be 0, 1 or 2')
        require(not self.output.is_relative_to(ROOT), 'Private evidence must be outside the public repository')
        self.output.mkdir(parents=True, exist_ok=True)
        require(not any(self.output.iterdir()), 'Use a new empty private output directory')
        if self.state:
            require(REQUIRED_SECTIONS <= sections(self.state.read_bytes()).keys(),
                    'Incomplete initial GBAS state')
        with socket.socket() as sock:
            sock.bind(('127.0.0.1', 0))
            self.port = sock.getsockname()[1]
        self.process = None
        self.log = None
        self.elapsed = 0
        self.actions = []
        self.captures = []
        self.status = {'execution_complete': False}

    def command(self, **arguments):
        with socket.create_connection(('127.0.0.1', self.port), timeout=45) as sock:
            sock.sendall((json.dumps(arguments) + '\n').encode())
            with sock.makefile('rb') as stream:
                reply = json.loads(stream.readline())
        require(reply.get('ok'), f'Native command failed: {reply}')
        return reply

    def start(self):
        env = session_environment(self.output)
        env['SMA3_LANGUAGE'] = str(self.language)
        self.log = (self.output / 'run.log').open('w')
        args = [self.runner.name, '--config', str(self.config), '--rom', str(self.rom),
                '--bios', str(self.bios), '--tcp', str(self.port), '--view-width', '240',
                '--save-path', str(self.output / 'fresh.sav')]
        self.process = subprocess.Popen(args, executable=str(self.runner), cwd=self.output,
                                        env=env, stdout=self.log, stderr=subprocess.STDOUT)
        for _ in range(100):
            try:
                self.command(cmd='ping')
                break
            except OSError:
                require(self.process.poll() is None, 'Native runner exited; inspect private run.log')
                time.sleep(.1)
        else:
            raise ValueError('Local TCP debug server unavailable')
        if self.state:
            self.command(cmd='savestate_load', path=str(self.state))
        self.start_frame = self.command(cmd='frame')['frame']
        return self.observe()

    def observe(self):
        data = bytes.fromhex(self.command(cmd='read_iwram', addr=0x03000000, len=0x8000)['data'])
        return {'frame': self.command(cmd='frame')['frame'],
                'elapsed_frames_requested': self.elapsed,
                'game_state': data[0x6b05],
                'substate': struct.unpack_from('<H', data, 0x4828)[0],
                'level_id': struct.unpack_from('<H', data, 0x6288)[0],
                'sublevel_id': struct.unpack_from('<H', data, 0x4cb8)[0],
                'red_coins': struct.unpack_from('<H', data, 0x6ad6)[0],
                'flowers': struct.unpack_from('<H', data, 0x6a9a)[0],
                'star_tenths': struct.unpack_from('<H', data, 0x6ace)[0]}

    def capture(self, name):
        require(re.fullmatch(r'[a-z0-9][a-z0-9_-]{0,60}', name), 'Invalid capture name')
        require(all(row['name'] != name for row in self.captures), 'Duplicate capture name')
        row = dict(self.observe(), name=name)
        shot = self.command(cmd='screenshot')
        require((shot['w'], shot['h']) == (240, 160), 'Unexpected screenshot size')
        Image.frombytes('RGB', (240, 160), bytes.fromhex(shot['data'])).save(self.output / (name + '.png'))
        self.command(cmd='savestate_save', path=str(self.output / (name + '.state')))
        self.captures.append(row)
        return row

    def action(self, action):
        require(type(action) is dict and set(action) <= {'n', 'keys', 'capture'}, 'Invalid action')
        n, keys = action.get('n', 0), action.get('keys', 1023)
        require(type(n) is int and 0 <= n <= 3600, 'Frame count must be 0..3600')
        require(type(keys) is int and 0 <= keys <= 1023, 'Invalid KEYINPUT mask')
        if n:
            reply = self.command(cmd='run_frames', n=n, keyinput=keys)
            require(reply['frames'] == n, 'Incomplete native frame batch')
            self.elapsed += n
        self.actions.append(dict(action))
        return self.capture(action['capture']) if 'capture' in action else self.observe()

    def close(self, error=None):
        try:
            if self.process is not None and self.process.poll() is None:
                self.command(cmd='quit')
                self.process.wait(timeout=10)
            require(self.process is not None and self.process.returncode == 0,
                    'Native runner did not exit cleanly')
            if self.log:
                self.log.flush()
            self.status['coverage'] = coverage(self.output)
            self.status['execution_complete'] = error is None
        except (OSError, ValueError, KeyError, subprocess.SubprocessError) as exc:
            error = str(exc) if error is None else error + '; shutdown: ' + str(exc)
        finally:
            if self.process is not None and self.process.poll() is None:
                self.process.terminate()
                try:
                    self.process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    self.process.kill()
                    self.process.wait()
            if self.log:
                self.log.close()
            if error:
                self.status['error'] = error
            provenance = {name: digest(getattr(self, name)) for name in ('runner', 'rom', 'bios', 'config')}
            if self.state:
                provenance['initial_state'] = digest(self.state)
            settings = {'language': self.language, 'width': 240,
                        'fresh_private_save': True,
                        'entry': 'verified_private_state' if self.state else 'fresh_boot',
                        'asset_sha256': provenance}
            for name, value in (('run-settings.json', settings), ('actions.json', self.actions),
                                ('captures.json', self.captures), ('run-status.json', self.status)):
                (self.output / name).write_text(json.dumps(value, indent=2) + '\n')
        require(error is None, error)


def compare_state(left, right):
    a, b = left.read_bytes(), right.read_bytes()
    sa, sb = sections(a), sections(b)
    require(sa.keys() == sb.keys() and REQUIRED_SECTIONS <= sa.keys(), 'Different GBAS section tables')
    require(a[:52] == b[:52], 'Different GBAS identities or versions')
    exact = []
    for tag in sa:
        x, n = sa[tag]
        y, m = sb[tag]
        require(n == m, f'Different section lengths: {tag.decode()}')
        aa, bb = a[x:x + n], b[y:y + m]
        if tag == b'BUS0':
            require(n == 395416 and aa[395277] == bb[395277] == 0,
                    'Unexpected bus layout or active RTC')
            # The pinned engine serializes host seconds even for an inactive RTC.
            aa = aa[:395310] + bytes(8) + aa[395318:]
            bb = bb[:395310] + bytes(8) + bb[395318:]
        require(aa == bb, f'State section differs: {tag.decode()}')
        exact.append(tag.decode())
    return exact


def compare_english(control, candidate):
    read = lambda folder, name: json.loads((folder / name).read_text())
    settings, other = (read(x, 'run-settings.json') for x in (control, candidate))
    require(settings['language'] == other['language'] == 0, 'English comparison requires language 0')
    require(settings['width'] == other['width'] == 240, 'Both viewports must be 240 pixels')
    for key in ('entry', 'fresh_private_save'):
        require(settings[key] == other[key], 'Different entry conditions')
    for key in set(settings['asset_sha256']) | set(other['asset_sha256']):
        if key != 'runner':
            require(settings['asset_sha256'].get(key) == other['asset_sha256'].get(key),
                    'Different private input: ' + key)
    require(read(control, 'actions.json') == read(candidate, 'actions.json'), 'Different input tapes')
    captures, others = (read(x, 'captures.json') for x in (control, candidate))
    require(captures and captures == others, 'Different checkpoint schedule or observations')
    reports = []
    for folder in (control, candidate):
        require(read(folder, 'run-status.json')['execution_complete'], 'Incomplete run')
        reports.append(coverage(folder))
    state_sections = None
    for row in captures:
        name = row['name']
        with Image.open(control / (name + '.png')) as a, Image.open(candidate / (name + '.png')) as b:
            require(a.size == b.size == (240, 160) and
                    a.convert('RGB').tobytes() == b.convert('RGB').tobytes(),
                    'English screenshot differs: ' + name)
        state_sections = compare_state(control / (name + '.state'), candidate / (name + '.state'))
    return {'english_regression_pass': True, 'checkpoints': len(captures),
            'checkpoint_names': [row['name'] for row in captures],
            'full_images_equal': True, 'full_state_sections_equal': state_sections,
            'ppu_included': True, 'exception': '8 inactive RTC wall-clock bytes in BUS0 only',
            'coverage': reports,
            'runner_sha256': [x['asset_sha256']['runner'] for x in (settings, other)]}


def read_bus(path):
    data = path.read_bytes()
    start, size = sections(data)[b'BUS0']
    require(size == 395416, 'Unexpected serialized bus layout')
    return data[start:start + size]


def qualify_resources(folder, rom_path, disasm):
    """Check resources actually consumed by the native BIOS/game, not ROM reads.

    File headers must be present in VRAM with the active screen's tilemap.
    Inventory labels must be in both the guest EWRAM buffer and its VRAM copy.
    Every unmodified table cell (including dynamic number fields) and every
    screenshot pixel outside the label cells must equal the English control.
    """
    sys.path.insert(0, str(ROOT / 'tools'))
    from localization_graphics import build_graphics, lz77_decode, PAUSE_TABLES
    rom = rom_path.read_bytes()
    generated = build_graphics(rom, disasm, ROOT)
    original_graphics = lz77_decode(rom, 0x1A42D8)[0]
    graphics = [original_graphics, *(lz77_decode(blob)[0] for blob in generated['blobs'])]
    layouts = (
        ('inventory_total', 0x15746, tuple(range(18))),
        ('inventory_best', 0x15506, (0, *range(5, 15))),
        ('inventory_world', 0x15446, tuple(range(5))),
    )
    widgets = {'files': 0, 'copy': 1, 'files-after-copy': 0,
               'erase': 2, 'files-after-erase': 0}
    files_report, inventory_report = [], []
    english_inventory = read_bus(folder / 'candidate-0-inventory/inventory.state')
    english_image = Image.open(folder / 'candidate-0-inventory/inventory.png').convert('RGB').tobytes()
    for variant, language in (('reference', 0), ('candidate', 0), ('candidate', 1), ('candidate', 2)):
        prefix = f'{variant}-{language}'
        for name, widget in widgets.items():
            bus = read_bus(folder / (prefix + '-files') / (name + '.state'))
            vram = bus[0x48400:0x60400]
            require(vram[0xE000:0xE000 + 3584] == graphics[language],
                    f'Wrong native decompressed header resource: {prefix}/{name}')
            expected = struct.unpack_from('<45H', rom, 0x199370 + widget * 90)
            actual = [word for row in range(3)
                      for word in struct.unpack_from('<15H', vram, 0x184A + row * 64)]
            require(actual == [word | ((8 + widget) << 12) for word in expected],
                    f'Wrong active file-menu widget: {prefix}/{name}')
            english = read_bus(folder / 'candidate-0-files' / (name + '.state'))
            require(bus[0x48000:0x48400] == english[0x48000:0x48400], 'File palette changed')
            files_report.append({'run': prefix + '-files', 'checkpoint': name,
                                 'decoded_resource_bytes_equal': 3584,
                                 'active_widget': widget, 'widget_tiles_equal': 45,
                                 'palette_bytes_equal': 1024})
        bus = read_bus(folder / (prefix + '-inventory') / 'inventory.state')
        require(bus[0x15400:0x15B00] == bus[0x55400:0x55B00],
                'Inventory guest tilemap did not reach VRAM 0600D000')
        require(bus[0x48000:0x48400] == english_inventory[0x48000:0x48400],
                'Inventory palette changed')
        verified_cells, unchanged_cells = 0, 0
        allowed_pixels = set()
        for (name, address, indices), (table_name, original, size, *_) in zip(layouts, PAUSE_TABLES):
            require(name == table_name and size == 24, 'Inventory table contract changed')
            if language == 0:
                codes = rom[original - 0x08000000:original - 0x08000000 + 24]
            else:
                pointer = next(row for row in generated['pointers'] if row[1] == original)
                offset = pointer[language + 1] - generated['bases'][language - 1]
                codes = generated['blobs'][language - 1][offset:offset + 24]
            for line, font in ((0, 0x19436A), (0x40, 0x1943FA)):
                for index in range(24):
                    at = address + line + index * 2
                    actual = struct.unpack_from('<H', bus, at)[0]
                    if index in indices:
                        expected = (struct.unpack_from('<H', rom, font + (codes[index] & ~1))[0] + 0x80) & 0xFFFF
                        require(actual == expected, f'Wrong native inventory glyph: {prefix}/{name}/{index}')
                        verified_cells += 1
                        tile = (at - 0x15400) // 2
                        x, y = tile % 32 * 8, tile // 32 * 8
                        allowed_pixels.update((y + dy) * 240 + x + dx
                                              for dy in range(8) for dx in range(8))
                    else:
                        require(bus[at:at + 2] == english_inventory[at:at + 2],
                                f'Inventory number/unchanged cell moved: {prefix}/{name}/{index}')
                        unchanged_cells += 1
        with Image.open(folder / (prefix + '-inventory') / 'inventory.png') as shot:
            actual_image = shot.convert('RGB').tobytes()
            require(shot.size == (240, 160), 'Unexpected inventory screenshot size')
        changed = [i for i in range(240 * 160)
                   if actual_image[i * 3:i * 3 + 3] != english_image[i * 3:i * 3 + 3]]
        require(all(i in allowed_pixels for i in changed), 'Inventory pixels changed outside labels')
        require(bool(changed) == bool(language), 'Missing translated inventory pixels')
        inventory_report.append({'run': prefix + '-inventory',
                                 'native_label_tiles_verified': verified_cells,
                                 'numeric_and_other_table_tiles_equal': unchanged_cells,
                                 'ewram_to_vram_tilemap_bytes_equal': 0x700,
                                 'palette_bytes_equal': 1024,
                                 'changed_label_pixels': len(changed),
                                 'unchanged_pixels_outside_label_cells': 240 * 160 - len(allowed_pixels)})
    return {'file_checkpoints': files_report, 'inventory_checkpoints': inventory_report,
            'native_resource_pass': True, 'numeric_fields_preserved': True,
            'palette_entries_preserved': True}


def qualify_suite(folder, rom, disasm):
    runs = []
    for variant, language in (('reference', 0), ('candidate', 0), ('candidate', 1), ('candidate', 2)):
        for scenario, actions in (('files', FILE_ACTIONS), ('inventory', INVENTORY_ACTIONS)):
            name = f'{variant}-{language}-{scenario}'
            path = folder / name
            read = lambda filename: json.loads((path / filename).read_text())
            settings, status = read('run-settings.json'), read('run-status.json')
            require(settings['language'] == language and settings['width'] == 240,
                    'Wrong session language or width')
            require(settings['fresh_private_save'] and status['execution_complete'], 'Incomplete/private-save run')
            require(read('actions.json') == actions, 'Session differs from the reviewed input tape')
            require(settings['entry'] == ('fresh_boot' if scenario == 'files' else 'verified_private_state'),
                    'Wrong session entry')
            if scenario == 'inventory':
                require(settings['asset_sha256']['initial_state'] == INVENTORY_STATE_SHA256,
                        'Unknown inventory checkpoint provenance')
            captures = read('captures.json')
            require([row['name'] for row in captures] == [a['capture'] for a in actions if 'capture' in a],
                    'Missing checkpoints')
            require(all(row['game_state'] == (8 if scenario == 'files' else 13) for row in captures),
                    'Unexpected game state at a checkpoint')
            runs.append({'name': name, 'language': language, 'scenario': scenario,
                         'requested_frames': sum(a['n'] for a in actions), 'captures': len(captures),
                         'asset_sha256': settings['asset_sha256'], 'coverage': coverage(path)})
    for key in ('rom', 'bios', 'config'):
        require(len({run['asset_sha256'][key] for run in runs}) == 1,
                'Inconsistent suite input: ' + key)
    require(runs[0]['asset_sha256']['rom'] == digest(rom), 'Qualification ROM differs from the executed ROM')
    for variant in ('reference', 'candidate'):
        require(len({run['asset_sha256']['runner'] for run in runs if run['name'].startswith(variant)}) == 1,
                'Mixed native builds in one variant')
    return {'status': 'passed', 'native_sessions': runs,
            'native_run_count': len(runs), 'private_capture_count': sum(run['captures'] for run in runs),
            'requested_frame_intervals': sum(run['requested_frames'] for run in runs),
            'english_regression': {name: compare_english(folder / ('reference-0-' + name),
                                                       folder / ('candidate-0-' + name))
                                   for name in ('files', 'inventory')},
            'resources': qualify_resources(folder, rom, disasm),
            'actions_sha256': {name: hashlib.sha256(json.dumps(tape, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
                               for name, tape in (('files', FILE_ACTIONS), ('inventory', INVENTORY_ACTIONS))},
            'validator_sha256': digest(Path(__file__)),
            'scope': 'File/copy/erase navigation with empty private save; inventory opened/closed from verified private GBAS. No populated-file deletion, full campaign, physical Android or performance qualification.'}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    commands = p.add_subparsers(dest='operation', required=True)
    run = commands.add_parser('run', help='Private deterministic native session')
    for name in ('runner', 'rom', 'bios', 'output'):
        run.add_argument('--' + name, type=Path, required=True)
    run.add_argument('--language', type=int, choices=(0, 1, 2), required=True)
    run.add_argument('--state', type=Path, help='Optional verified private starting GBAS, read only')
    actions = run.add_mutually_exclusive_group(required=True)
    actions.add_argument('--actions', type=Path, help='JSON input tape; see recorded actions.json')
    actions.add_argument('--scenario', choices=('files', 'inventory'), help='Reviewed bounded input tape')
    actions.add_argument('--interactive', action='store_true', help='Read action JSON lines until quit/EOF')
    compare = commands.add_parser('compare-english', help='Full-state and image regression, including PPU')
    compare.add_argument('control', type=Path)
    compare.add_argument('candidate', type=Path)
    compare.add_argument('report', type=Path)
    qualify = commands.add_parser('qualify', help='Check a completed eight-run suite and write public aggregates')
    qualify.add_argument('folder', type=Path)
    for name in ('rom', 'disasm', 'report'):
        qualify.add_argument('--' + name, type=Path, required=True)
    suite = commands.add_parser('suite', help='Run and qualify all eight sessions sequentially')
    for name in ('reference', 'candidate', 'rom', 'bios', 'state', 'disasm', 'output', 'report'):
        suite.add_argument('--' + name, type=Path, required=True)
    a = p.parse_args()
    if a.operation in ('suite', 'qualify'):
        require(not a.report.exists(), 'Use a new report path')
        if a.operation == 'suite':
            require(digest(a.state) == INVENTORY_STATE_SHA256, 'Use the verified private inventory checkpoint')
            require(not a.output.exists(), 'Use a new private suite output directory')
            for variant, language in (('reference', 0), ('candidate', 0), ('candidate', 1), ('candidate', 2)):
                for scenario, tape in (('files', FILE_ACTIONS), ('inventory', INVENTORY_ACTIONS)):
                    name = f'{variant}-{language}-{scenario}'
                    session = NativeSession(getattr(a, variant), a.rom, a.bios, a.output / name,
                                            language, a.state if scenario == 'inventory' else None)
                    error = None
                    try:
                        session.start()
                        for action in tape:
                            session.action(action)
                    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as exc:
                        error = str(exc)
                    finally:
                        session.close(error)
                    print(json.dumps({'run': name, **session.status}), flush=True)
            folder = a.output
        else:
            folder = a.folder
        result = qualify_suite(folder, a.rom, a.disasm)
        with a.report.open('x') as output:
            output.write(json.dumps(result, indent=2) + '\n')
        print(json.dumps({'status': result['status'], 'native_runs': result['native_run_count'],
                          'private_captures': result['private_capture_count'],
                          'native_resource_pass': result['resources']['native_resource_pass']}))
        return
    if a.operation == 'compare-english':
        require(not a.report.exists(), 'Use a new report path')
        result = compare_english(a.control, a.candidate)
        a.report.write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps(result))
        return
    session = NativeSession(a.runner, a.rom, a.bios, a.output, a.language, a.state)
    error = None
    try:
        print(json.dumps(session.start()), flush=True)
        if a.interactive:
            for line in sys.stdin:
                if line.strip() == 'quit':
                    break
                print(json.dumps(session.action(json.loads(line))), flush=True)
        else:
            if a.scenario:
                require((a.state is None) == (a.scenario == 'files'), 'Wrong scenario entry')
                if a.state:
                    require(digest(a.state) == INVENTORY_STATE_SHA256, 'Unknown inventory checkpoint')
                tape = FILE_ACTIONS if a.scenario == 'files' else INVENTORY_ACTIONS
            else:
                tape = json.loads(a.actions.read_text())
            require(type(tape) is list and tape, 'Input tape must be a nonempty list')
            for action in tape:
                print(json.dumps(session.action(action)), flush=True)
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as exc:
        error = str(exc)
    finally:
        session.close(error)
    print(json.dumps(session.status), flush=True)


if __name__ == '__main__':
    main()
