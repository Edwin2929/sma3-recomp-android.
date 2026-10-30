#!/usr/bin/env python3
"""Qualify panorama edge decoration against an identical native game replay.

ROM, BIOS, states, screenshots and logs stay in a private output directory.
Only summary.json contains publishable aggregate evidence. This exercises
native guest rendering on Linux, not an Android device or an FPS measurement.
"""

import argparse
import json
import math
import os
from pathlib import Path
import signal
import struct
import subprocess
import sys

from PIL import Image, ImageChops

from compare_panorama_sessions import compare_state, local_file
from make_level_fixture import sections
from panorama_session_checks import (required_capture_frames, validate_actions,
                                     validate_trace)
from validate_yoshi_color_gameplay import digest, read_json, require


ROOT = Path(__file__).resolve().parents[1]


def check_session(args, directory, binary):
    settings = read_json(directory / 'run-settings.json')
    expected = {name: digest(path) for name, path in (
        ('rom', args.rom), ('bios', args.bios), ('state', args.state),
        ('runner', binary), ('config', ROOT / 'runtime.toml'))}
    require(settings['asset_sha256'] == expected, 'Replay input identity mismatch')
    require(settings['width'] == args.width and settings['trace_every_frame'] is True
            and settings['capture_start'] == 0, 'Replay geometry or tracing mismatch')
    require(settings.get('observation_profile') is None,
            'This checker expects the ordinary per-frame observation profile')
    actions = read_json(directory / 'actions.json')
    require(actions == read_json(args.actions), 'Replay action tape mismatch')
    status = read_json(directory / 'run-status.json')
    require(status.get('execution_complete') is True, 'Incomplete native replay')
    coverage = read_json(directory / 'native-coverage.json')
    require(coverage.get('coverage') == 'FULLY_STATIC', 'Replay is not fully native')
    for key in ('distinct_misses', 'interpreted_insns', 'healed_native', 'failed'):
        require(coverage.get(key) == 0, 'Nonzero native diagnostic: ' + key)
    session = read_json(directory / 'session.json')
    trace = read_json(directory / 'trace.json')
    validate_trace(actions, session, trace)
    captures = read_json(directory / 'captures.json')
    require(type(captures) is list and captures, 'Missing sampled captures')
    frames = [shot['elapsed_frame'] for shot in captures]
    require(frames == sorted(set(frames)) and
            set(frames) == required_capture_frames(session, trace, 0),
            'Missing, duplicate or unscheduled capture')
    for shot in captures:
        elapsed = shot['elapsed_frame']
        require(type(elapsed) is int and 0 < elapsed < len(trace) and
                shot['frame'] == trace[elapsed]['frame'], 'Capture disagrees with trace')
        require(local_file(directory, shot['image'], '.png').is_file() and
                local_file(directory, shot['state'], '.state').is_file(),
                'Missing capture image or state')
    return trace, captures


def run(args, name, binary):
    directory = args.output / name
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith(('SMA3_', 'GBARECOMP_'))}
    command = [sys.executable, str(ROOT / 'validation/run_panorama_session.py'),
               '--runner', str(binary), '--rom', str(args.rom), '--bios', str(args.bios),
               '--state', str(args.state), '--actions', str(args.actions),
               '--width', str(args.width), '--output', str(directory), '--trace-every-frame']
    # Own the diagnostic script and its native child in a new process group.
    # A timeout must not leave a runner or localhost debug server behind.
    with (args.output / (name + '.log')).open('w') as log:
        process = subprocess.Popen(command, env=environment, stdout=log,
                                   stderr=subprocess.STDOUT, start_new_session=True)
        try:
            code = process.wait(timeout=args.timeout)
        except BaseException:
            if process.poll() is None:
                os.killpg(process.pid, signal.SIGTERM)
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL)
                    process.wait()
            raise
    require(code == 0, name + ': replay failed; see private log')
    check_session(args, directory, binary)
    return directory


def compare_ppu(left, right):
    a, b = left.read_bytes(), right.read_bytes()
    sa, sb = sections(a)[b'PPU0'], sections(b)[b'PPU0']
    require(sa[1] == sb[1] == 115223, 'Unexpected pinned-engine PPU state layout')
    # Serialization stores the native 240x160 crop even for a panoramic latch.
    # Both the 23-byte timing header and that crop must be unchanged.
    require(a[sa[0]:sa[0] + 23] == b[sb[0]:sb[0] + 23], 'PPU timing changed')
    require(a[sa[0] + 23:sa[0] + sa[1]] == b[sb[0] + 23:sb[0] + sb[1]],
            'Serialized native framebuffer changed')


def compare_images(left, right, width, *, protected=False, changed_side='both'):
    require(type(width) is int and 240 <= width <= 356, 'Unsupported logical width')
    require(changed_side in ('left', 'right', 'both'), 'Invalid permitted changed side')
    with Image.open(left) as source, Image.open(right) as target:
        a, b = source.convert('RGB'), target.convert('RGB')
        require(a.size == b.size == (width, 160), 'Unexpected framebuffer dimensions')
        offset = (width - 240) // 2
        native = (offset, 0, offset + 240, 160)
        require(a.crop(native).tobytes() == b.crop(native).tobytes(),
                'Original 240x160 center changed')
        aa, bb = a.tobytes(), b.tobytes()
        counts = {'left': 0, 'right': 0}
        for y in range(160):
            for x in range(width):
                at = (y * width + x) * 3
                if aa[at:at + 3] == bb[at:at + 3]:
                    continue
                side = 'left' if x < offset else 'right'
                require(changed_side in ('both', side), 'Pixels changed on the unrequested edge')
                counts[side] += 1
        total = counts['left'] + counts['right']
        require(not protected or total == 0, 'Protected presentation changed')
        bounds = ImageChops.difference(a, b).getbbox()
        return {'changed_pixels': total, 'left_changed_pixels': counts['left'],
                'right_changed_pixels': counts['right'],
                'bounds': list(bounds) if bounds else None,
                'native_center_exact': True, 'protected': protected}


def compare(args, baseline, candidate):
    trace, captures = check_session(args, baseline, args.baseline)
    other_trace, other_captures = check_session(args, candidate, args.candidate)
    require(trace == other_trace, 'Per-frame trajectory or input changed')
    require(read_json(baseline / 'session.json') == read_json(candidate / 'session.json'),
            'Action endpoints changed')
    require(captures == other_captures, 'Capture schedule changed')
    explicit = set(args.protected_frame)
    require(explicit <= {c['elapsed_frame'] for c in captures},
            'A requested protected frame was not captured')
    results = []
    for shot in captures:
        elapsed = shot['elapsed_frame']
        state_a = local_file(baseline, shot['state'], '.state')
        state_b = local_file(candidate, shot['state'], '.state')
        compare_state(state_a, state_b, trace[elapsed])
        compare_ppu(state_a, state_b)
        # Leave the first transition latches to explicit frame review: the
        # captured picture may have been composed under the preceding state.
        stable_non_gameplay = elapsed >= 3 and all(
            row['game_state'] != 13 for row in trace[elapsed - 3:elapsed + 1])
        protected = args.width == 240 or elapsed in explicit or stable_non_gameplay
        delta = compare_images(local_file(baseline, shot['image'], '.png'),
                               local_file(candidate, shot['image'], '.png'), args.width,
                               protected=protected, changed_side=args.changed_side)
        results.append({'elapsed_frame': elapsed, 'guest_frame': shot['frame'],
                        'game_state': trace[elapsed]['game_state'], **delta})
    exact = compare_state(baseline / 'final.state', candidate / 'final.state', trace[-1])
    compare_ppu(baseline / 'final.state', candidate / 'final.state')
    changed = sum(row['changed_pixels'] for row in results)
    require(not args.require_change or changed > 0,
            'No edge decoration was visible in the sampled images')
    return {'trace_rows_equal': len(trace), 'guest_frames': len(trace) - 1,
            'sampled_captures': len(captures), 'compared_states': len(captures) + 1,
            'guest_sections_equal_except_inactive_rtc': exact,
            'ppu_timing_equal': True, 'serialized_native_framebuffers_equal': True,
            'native_centers_equal': True, 'changed_margin_pixels': changed,
            'protected_captures': sum(row['protected'] for row in results),
            'captures': results, 'final_player_xy_velocity': trace[-1]['player_xy_velocity'],
            'final_game_state': trace[-1]['game_state']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('baseline', 'candidate', 'rom', 'bios', 'state', 'actions', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--width', type=int, choices=(240, 347, 356), default=356)
    parser.add_argument('--reuse-baseline', type=Path,
                        help='Reuse completed reference only after verifying every input hash')
    parser.add_argument('--protected-frame', type=int, action='append', default=[],
                        help='Captured elapsed frame that must retain every pixel')
    parser.add_argument('--changed-side', choices=('left', 'right', 'both'), default='both')
    parser.add_argument('--require-change', action='store_true',
                        help='Reject a wide edge case that never visibly decorates any capture')
    parser.add_argument('--timeout', type=float, default=180,
                        help='Maximum seconds for each native replay, including startup')
    args = parser.parse_args()
    try:
        require(math.isfinite(args.timeout) and 0 < args.timeout <= 3600,
                'Timeout must be finite, positive and at most one hour')
        require(all(n > 0 for n in args.protected_frame) and
                len(args.protected_frame) == len(set(args.protected_frame)),
                'Protected frame numbers must be unique positive integers')
        require(not args.require_change or args.width > 240,
                'Original 240px mode must never change')
        for name in ('baseline', 'candidate', 'rom', 'bios', 'state', 'actions'):
            path = getattr(args, name).resolve()
            require(path.is_file(), 'Missing input: ' + name)
            setattr(args, name, path)
        validate_actions(read_json(args.actions))
        args.output = args.output.resolve()
        args.output.mkdir(parents=True, exist_ok=True)
        require(not any(args.output.iterdir()), 'Use a new empty private output directory')
        if args.reuse_baseline:
            baseline = args.reuse_baseline.resolve()
            check_session(args, baseline, args.baseline)
        else:
            baseline = run(args, 'baseline', args.baseline)
        candidate = run(args, 'candidate', args.candidate)
        result = compare(args, baseline, candidate)
        report = {'result': 'PASS', 'width': args.width,
                  'baseline_reused': bool(args.reuse_baseline),
                  'require_change': args.require_change, 'changed_side': args.changed_side,
                  'explicit_protected_frames': sorted(args.protected_frame),
                  'input_sha256': {name: digest(getattr(args, name)) for name in
                      ('baseline', 'candidate', 'rom', 'bios', 'state', 'actions')},
                  'comparison': result,
                  'limits': ['Linux native replay; no physical Android, full-game or FPS claim.',
                             'Only the pinned inactive RTC wall-clock field (8 bytes) is excluded from state comparison.',
                             'Native center and serialized PPU image are exact; only sampled panorama margin pixels may change.',
                             'Snapshots do not certify every unsampled output frame; the complete per-frame guest trace is compared.',
                             'Private entry fixtures can be synthetic; their provenance must be documented separately.']}
        (args.output / 'summary.json').write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps({'result': 'PASS', 'width': args.width,
                          'trace_rows': result['trace_rows_equal'],
                          'captures': result['sampled_captures'],
                          'changed_margin_pixels': result['changed_margin_pixels'],
                          'protected_captures': result['protected_captures']}))
    except (OSError, ValueError, KeyError, TypeError, IndexError, struct.error,
            subprocess.SubprocessError) as error:
        parser.exit(1, 'FAIL ' + str(error) + '\n')


if __name__ == '__main__':
    main()
