#!/usr/bin/env python3
"""Compare a real native game replay before/after the character-color mod.

Requires the user's verified ROM/BIOS, private savestate and action tape.
Every run uses the same input and a separate save. Captures and states stay
in a private output directory; only summary.json is suitable to publish.
This exercises guest rasterization on Linux, not Android UI or device FPS.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
from functools import lru_cache

from PIL import Image, ImageChops

from compare_panorama_sessions import compare_state
from make_level_fixture import sections


ROOT = Path(__file__).resolve().parents[1]


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_json(path):
    return json.loads(path.read_text())


def compare_ppu_timing(left, right):
    a, b = left.read_bytes(), right.read_bytes()
    sa, sb = sections(a)[b'PPU0'], sections(b)[b'PPU0']
    require(sa[1] == sb[1] == 115223, 'Unexpected pinned-engine PPU state layout')
    # The first 23 bytes hold scanline, dot/cycle, vcount, frame counter and
    # latch-valid flag. The remainder is the intentionally recolored image.
    require(a[sa[0]:sa[0] + 23] == b[sb[0]:sb[0] + 23], 'PPU timing changed')


def image_change(left, right):
    with Image.open(left) as source, Image.open(right) as target:
        a, b = source.convert('RGB'), target.convert('RGB')
        require(a.size == b.size, 'Framebuffer dimensions changed')
        box = ImageChops.difference(a, b).getbbox()
        left_bytes, right_bytes = a.tobytes(), b.tobytes()
        count = sum(left_bytes[index:index + 3] != right_bytes[index:index + 3]
                    for index in range(0, len(left_bytes), 3))
        return {'pixels': count, 'bounds': list(box) if box else None}


@lru_cache(maxsize=32768)
def brightness_pair(before, after):
    """Classify only the existing compositor's precise brighten/darken math.

    This is a differential attribution check, not a hardware oracle. The
    separate real-PPU test verifies that only selected OBJ receive the effect.
    Palette bit 15 is not retained in RGB888, so both low green bits are tried.
    """
    expand = lambda value: (value << 3) | (value >> 2)
    if any(expand(value >> 3) != value for value in before):
        return False
    for coefficient in range(1, 17):
        for green_low in (0, 1):
            parts = (before[0] >> 3, (before[1] >> 3) * 2 + green_low, before[2] >> 3)
            for bright in (False, True):
                result = []
                for channel, value in enumerate(parts):
                    limit = 63 if channel == 1 else 31
                    next_value = (value + ((limit - value) * coefficient + 8) // 16
                                  if bright else value - (value * coefficient + 7) // 16)
                    result.append(expand(next_value // 2 if channel == 1 else next_value))
                if tuple(result) == after:
                    return True
    return False


def verify_brightness_delta(left, right):
    with Image.open(left) as source, Image.open(right) as target:
        a, b = source.convert('RGB').tobytes(), target.convert('RGB').tobytes()
        for index in range(0, len(a), 3):
            before, after = tuple(a[index:index + 3]), tuple(b[index:index + 3])
            require(before == after or brightness_pair(before, after),
                    'Previous-release pixel delta is not explained by the verified OBJ brightness fix')


def run(args, name, binary, color):
    output = args.output / name
    env = {key: value for key, value in os.environ.items()
           if not key.startswith(('SMA3_', 'GBARECOMP_'))}
    if color is not None:
        env['SMA3_YOSHI_COLOR'] = color
    command = [sys.executable, str(ROOT / 'validation/run_panorama_session.py'),
               '--runner', str(binary), '--rom', str(args.rom), '--bios', str(args.bios),
               '--state', str(args.state), '--actions', str(args.actions),
               '--width', str(args.width), '--output', str(output), '--trace-every-frame']
    if args.fuzzy:
        command += ['--fuzzy-status', '--expect-fuzzy-recovery']
    result = subprocess.run(command, env=env, capture_output=True, text=True,
                            timeout=args.timeout)
    (args.output / (name + '.log')).write_text(result.stdout + result.stderr)
    require(result.returncode == 0, name + ': replay failed; see its private log')
    status = read_json(output / 'run-status.json')
    require(status['execution_complete'], name + ': incomplete replay')
    coverage = read_json(output / 'native-coverage.json')
    require(coverage.get('coverage') == 'FULLY_STATIC', name + ': execution is not fully native')
    for field in ('distinct_misses', 'interpreted_insns', 'healed_native', 'failed'):
        require(coverage.get(field) == 0, name + ': nonzero execution diagnostic ' + field)
    print('PASS replay ' + name, flush=True)
    return output, status


def compare(reference, candidate, *, exact_pixels, expect_color, transform_cycle, brightness_fix=False):
    trace = read_json(reference / 'trace.json')
    require(trace == read_json(candidate / 'trace.json'), 'Per-frame game trajectory changed')
    require(read_json(reference / 'session.json') == read_json(candidate / 'session.json'),
            'Action endpoints changed')
    captures = read_json(reference / 'captures.json')
    require(captures == read_json(candidate / 'captures.json') and captures,
            'Capture schedule changed or has no samples')
    states = 0
    changed = []
    stable_vehicle_captures = 0
    brightness_pixels = 0
    for capture in captures:
        state = capture['state']
        compare_state(reference / state, candidate / state)
        compare_ppu_timing(reference / state, candidate / state)
        states += 1
        delta = image_change(reference / capture['image'], candidate / capture['image'])
        elapsed = capture['elapsed_frame']
        if exact_pixels:
            if brightness_fix and delta['pixels']:
                verify_brightness_delta(reference / capture['image'], candidate / capture['image'])
                brightness_pixels += delta['pixels']
            else:
                require(delta['pixels'] == 0, 'Disabled/unsupported mod changed pixels')
        # The transformation transition can contain an already-latched normal
        # Yoshi frame. Require exact original art once it has been active for
        # at least 15 consecutive frames, independently of the mod's selector.
        stable_vehicle = elapsed >= 15 and all(
            row['transformation'] != 0 for row in trace[elapsed - 15:elapsed + 1])
        if transform_cycle and stable_vehicle:
            require(delta['pixels'] == 0, 'Vehicle art was recolored')
            stable_vehicle_captures += 1
        changed.append(dict(elapsed_frame=elapsed, transformation=trace[elapsed]['transformation'], **delta))
    exact_sections = compare_state(reference / 'final.state', candidate / 'final.state')
    compare_ppu_timing(reference / 'final.state', candidate / 'final.state')
    if expect_color:
        require(any(row['pixels'] > 0 for row in changed), 'Enabled color did not change any sampled image')
    if transform_cycle:
        require(trace[0]['transformation'] == trace[-1]['transformation'] == 0 and
                any(row['transformation'] != 0 for row in trace), 'No complete transformation cycle')
        require(stable_vehicle_captures > 0, 'No stable vehicle image sampled')
        if expect_color:
            require(any(row['pixels'] > 0 and row['elapsed_frame'] >
                        max(x['elapsed_frame'] for x in trace if x['transformation'] != 0)
                        for row in changed), 'Custom color did not return after transformation')
    return {'trace_rows_equal': len(trace), 'states_equal_except_pixels_inactive_rtc': states + 1,
            'exact_sections': exact_sections, 'ppu_timing_equal': True,
            'changed_captures': changed, 'stable_vehicle_captures': stable_vehicle_captures,
            'verified_previous_release_brightness_pixels': brightness_pixels}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('baseline', 'candidate', 'rom', 'bios', 'state', 'actions', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--width', type=int, choices=(240, 356), default=240)
    parser.add_argument('--color', action='append', default=[])
    parser.add_argument('--fuzzy', action='store_true', help='Require actual contact and natural effect recovery')
    parser.add_argument('--transform-cycle', action='store_true')
    parser.add_argument('--original-only', action='store_true', help='Fixture must retain all original pixels')
    parser.add_argument('--verify-obj-brightness-fix', action='store_true',
        help='Allow previous-release pixel differences only if every changed RGB matches the separately tested OBJ brightness correction')
    parser.add_argument('--timeout', type=float, default=180)
    args = parser.parse_args()
    try:
        for name in ('baseline', 'candidate', 'rom', 'bios', 'state', 'actions'):
            path = getattr(args, name).resolve()
            require(path.is_file(), 'Missing input: ' + name)
            setattr(args, name, path)
        colors = [color.removeprefix('#').upper() for color in args.color or ['A855F7']]
        require(all(re.fullmatch('[0-9A-F]{6}', color) for color in colors), 'Use six-digit RGB codes')
        require(len(colors) == len(set(colors)), 'Duplicate color case')
        args.output = args.output.resolve()
        args.output.mkdir(parents=True, exist_ok=True)
        require(not any(args.output.iterdir()), 'Use a new empty private output directory')
        reference, status = run(args, 'baseline', args.baseline, None)
        disabled, _ = run(args, 'disabled', args.candidate, None)
        results = {'disabled': compare(reference, disabled, exact_pixels=True,
            expect_color=False, transform_cycle=args.transform_cycle,
            brightness_fix=args.verify_obj_brightness_fix)}
        for color in colors:
            candidate, _ = run(args, 'color-' + color, args.candidate, color)
            # Isolate the mod itself from any independently verified renderer
            # correction: compare each color to this same build with it OFF.
            results[color] = compare(disabled, candidate, exact_pixels=args.original_only,
                expect_color=not args.original_only, transform_cycle=args.transform_cycle)
        report = {'result': 'PASS', 'width': args.width, 'colors': colors,
            'fuzzy_recovery': status.get('fuzzy_recovery'), 'transform_cycle': args.transform_cycle,
            'references': {'disabled': 'previous release', 'colors': 'candidate with mod disabled'},
            'verify_previous_release_obj_brightness_fix': args.verify_obj_brightness_fix,
            'input_sha256': {name: digest(getattr(args, name)) for name in
                ('baseline', 'candidate', 'rom', 'bios', 'state', 'actions')}, 'comparisons': results,
            'limits': ['Linux native replay; no Android UI, phone or FPS measurement.',
                       'PPU image bytes intentionally differ with color; timing must match.',
                       'Only the pinned inactive RTC wall-clock field is excluded from guest comparison.',
                       'Nonzero pixel changes prove rendering; sprite isolation has separate PPU tests.']}
        (args.output / 'summary.json').write_text(json.dumps(report, indent=2) + '\n')
        print('PASS guest-state and image comparisons', flush=True)
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        parser.exit(1, 'FAIL ' + str(error) + '\n')


if __name__ == '__main__':
    main()
