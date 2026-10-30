#!/usr/bin/env python3
"""Qualify local-achievement observation without changing a native game replay.

Requires private ROM, BIOS, state and input tape. Output states and pictures
remain private; only the sanitized summary is suitable for publication.
This is a Linux native replay, not an Android-device or FPS measurement.
"""

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

from validate_yoshi_color_gameplay import compare, digest, read_json, require


ROOT = Path(__file__).resolve().parents[1]
FIELDS = ('schema', 'completed_levels', 'perfect_levels', 'transformations',
          'red_coins', 'flowers', 'stars', 'event_flags')


def check_session(args, directory, binary):
    settings = read_json(directory / 'run-settings.json')
    expected = {name: digest(path) for name, path in (
        ('rom', args.rom), ('bios', args.bios), ('state', args.state),
        ('runner', binary), ('config', ROOT / 'runtime.toml'))}
    require(settings['asset_sha256'] == expected, 'Replay input identity mismatch')
    require(settings['width'] == args.width and settings['trace_every_frame'] is True
            and settings['capture_start'] == 0, 'Replay geometry or tracing mismatch')
    require(read_json(directory / 'actions.json') == read_json(args.actions),
            'Replay action tape mismatch')
    status = read_json(directory / 'run-status.json')
    require(status['execution_complete'], 'Incomplete native replay')
    if args.fuzzy:
        recovery = status.get('fuzzy_recovery', {})
        require(recovery.get('pass') is True and recovery.get('no_restart_or_death')
                and recovery.get('effect_was_not_disabled'), 'Fuzzy recovery failed')
    coverage = read_json(directory / 'native-coverage.json')
    require(coverage.get('coverage') == 'FULLY_STATIC', 'Replay is not fully native')
    for key in ('distinct_misses', 'interpreted_insns', 'healed_native', 'failed'):
        require(coverage.get(key) == 0, 'Nonzero native diagnostic: ' + key)
    return status


def run(args, name, binary, achievements):
    directory = args.output / name
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith(('SMA3_', 'GBARECOMP_'))}
    if achievements:
        environment['SMA3_LOCAL_ACHIEVEMENTS_REPORT'] = str(
            directory / 'local-achievements.json')
    command = [sys.executable, str(ROOT / 'validation/run_panorama_session.py'),
               '--runner', str(binary), '--rom', str(args.rom), '--bios', str(args.bios),
               '--state', str(args.state), '--actions', str(args.actions),
               '--width', str(args.width), '--output', str(directory), '--trace-every-frame']
    if args.fuzzy:
        command += ['--fuzzy-status', '--expect-fuzzy-recovery']
    if args.score:
        command += ['--score-status']
    result = subprocess.run(command, env=environment, capture_output=True, text=True,
                            timeout=args.timeout)
    (args.output / (name + '.log')).write_text(result.stdout + result.stderr)
    require(result.returncode == 0, name + ': replay failed; see private log')
    check_session(args, directory, binary)
    return directory


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('baseline', 'candidate', 'rom', 'bios', 'state', 'actions', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--expected', type=Path, required=True,
                        help='Reviewed JSON object mapping all eight snapshot fields to exact values')
    parser.add_argument('--reuse-baseline', type=Path,
                        help='Reuse a completed reference after verifying all input identities')
    parser.add_argument('--width', type=int, choices=(240, 356), default=240)
    parser.add_argument('--fuzzy', action='store_true')
    parser.add_argument('--score', action='store_true', help='Verify a real score-tally confirmation')
    parser.add_argument('--transform-cycle', action='store_true')
    parser.add_argument('--timeout', type=float, default=240)
    args = parser.parse_args()
    for name in ('baseline', 'candidate', 'rom', 'bios', 'state', 'actions', 'output', 'expected'):
        setattr(args, name, getattr(args, name).resolve())
    expected = read_json(args.expected)
    require(set(expected) == set(FIELDS) and all(
        type(expected[key]) is int and 0 <= expected[key] < 2**64 for key in FIELDS),
        'Expected snapshot must define all eight unsigned integer fields')
    require(expected['schema'] == 1, 'Unknown expected snapshot schema')
    args.output.mkdir(parents=True, exist_ok=True)
    require(not any(args.output.iterdir()), 'Use a new empty output directory')
    if args.reuse_baseline:
        baseline = args.reuse_baseline.resolve()
        check_session(args, baseline, args.baseline)
    else:
        baseline = run(args, 'baseline', args.baseline, False)
    candidate = run(args, 'candidate', args.candidate, True)
    raw = read_json(candidate / 'local-achievements.json')['progress']
    require(type(raw) is list and len(raw) == 8 and all(
        type(value) is int and 0 <= value < 2**64 for value in raw),
        'Malformed native achievement snapshot')
    observed = dict(zip(FIELDS, raw))
    require(observed == expected, 'Achievement evidence mismatch: ' + json.dumps(observed))
    comparison = compare(baseline, candidate, exact_pixels=True, expect_color=False,
                         transform_cycle=args.transform_cycle)
    status = check_session(args, candidate, args.candidate)
    score_checks = None
    if args.score:
        rows = read_json(candidate / 'trace.json')
        confirmations = [(before, after) for before, after in zip(rows, rows[1:])
                         if before['game_state'] == after['game_state'] == 14 and
                         before['score_status']['substate'] in (0x3d, 0x3e) and
                         after['score_status']['substate'] in (0x3f, 0x40)]
        require(len(confirmations) == 1, 'Expected one real result confirmation')
        before, after = confirmations[0]
        require(before['score_status']['score'] == after['score_status']['score'] == 100,
                'The confirmed score is not 100')
        require(rows[-1]['frame'] - rows[0]['frame'] == len(rows) - 1,
                'Score replay left an unconsumed VBlank at its final checkpoint')
        score_checks = {'confirmed_score': 100, 'confirmation_guest_frame': after['frame'],
                        'confirmation_command_index': after['elapsed_frame'],
                        'nonunit_frame_advances': [b['frame'] - a['frame'] for a, b in zip(rows, rows[1:])
                                                  if b['frame'] != a['frame'] + 1]}
    report = {'result': 'PASS', 'width': args.width, 'observed': observed,
              'expected': expected, 'baseline_reused': bool(args.reuse_baseline),
              'comparison': comparison, 'fuzzy_recovery': status.get('fuzzy_recovery'),
              'score_checks': score_checks,
              'input_sha256': {name: digest(getattr(args, name)) for name in (
                  'baseline', 'candidate', 'rom', 'bios', 'state', 'actions', 'expected')},
              'limits': ['Native Linux replay; not Android UI or phone FPS.',
                         'Only the pinned inactive RTC wall-clock field is excluded from guest comparison.',
                         'All sampled pixels and PPU timing must match the reference exactly.',
                         'Private test inputs may use synthetic entry/position preparation; achievements are observed from executed game transitions.']}
    (args.output / 'summary.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'result': 'PASS', 'width': args.width, 'progress': observed,
                      'trace_rows': comparison['trace_rows_equal']}))


if __name__ == '__main__':
    main()
