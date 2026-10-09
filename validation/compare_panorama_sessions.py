#!/usr/bin/env python3
"""Compare private sessions. Publish aggregates only; never overwrite old evidence."""
import argparse
import json
import re
import struct
from itertools import groupby
from pathlib import Path
from PIL import Image
from make_level_fixture import sections
from panorama_session_checks import (check_car_cycle, require, required_capture_frames,
                                     validate_trace)


def read_json(path):
    return json.loads(path.read_text())


def compare_state(left, right, expected=None):
    a, b = left.read_bytes(), right.read_bytes()
    sa, sb = sections(a), sections(b)
    required = {b'CPU0', b'BUS0', b'PPU0', b'IO_0', b'AUD0', b'SAV0', b'META'}
    require(sa.keys() == sb.keys() and required <= sa.keys(), 'Missing or different state sections')
    exact = []
    for tag in sa:
        x, n = sa[tag]
        y, m = sb[tag]
        require(n == m, f'State section size differs: {tag!r}')
        aa, bb = a[x:x + n], b[y:y + m]
        if tag == b'PPU0':
            continue  # Viewport-specific; central pixels are compared separately.
        if tag == b'BUS0':
            require(n == 395416 and aa[395277] == bb[395277] == 0,
                    'Unexpected bus layout or active RTC')
            # Pinned engine writes wall-clock seconds even when RTC is inactive.
            aa = aa[:395310] + bytes(8) + aa[395318:]
            bb = bb[:395310] + bytes(8) + bb[395318:]
        require(aa == bb, f'State differs: {tag!r}')
        exact.append(tag.decode())
    if expected is not None:
        base = sa[b'BUS0'][0] + 0x40000
        actual = dict(level_id=struct.unpack_from('<H', a, base + 0x6288)[0],
                      sublevel_id=struct.unpack_from('<H', a, base + 0x4cb8)[0],
                      game_state=a[base + 0x6b05],
                      transformation=struct.unpack_from('<H', a, base + 0x6db2)[0],
                      player_xy_velocity=[v / 256 for v in struct.unpack_from('<iiii', a, base + 0x6d80)])
        require(all(expected[k] == v for k, v in actual.items()),
                'Serialized game state differs from the observed checkpoint')
    return exact


def compare_image(left, right):
    with Image.open(left) as source, Image.open(right) as target:
        a, b = source.convert('RGB'), target.convert('RGB')
        require(a.size == (240, 160) and b.size == (356, 160), 'Unexpected viewport dimensions')
        require(a.tobytes() == b.crop((58, 0, 298, 160)).tobytes(), 'Central image differs')


def local_file(folder, name, suffix):
    require(type(name) is str and Path(name).name == name and name.endswith(suffix),
            'Invalid checkpoint filename')
    return folder / name


def compare(control, candidate, expectations=None):
    left, right = (read_json(d / 'session.json') for d in (control, candidate))
    require(left == right and left, 'Session trajectory differs')
    has_trace = [(d / 'trace.json').exists() for d in (control, candidate)]
    require(has_trace[0] == has_trace[1], 'Missing trace on one side')
    require(expectations is None or all(has_trace), 'Complete-cycle checks require per-frame traces')
    centers, state_count = [], 0
    cycle = None
    trace = None
    if all(has_trace):
        actions, other_actions = (read_json(d / 'actions.json') for d in (control, candidate))
        trace, other_trace = (read_json(d / 'trace.json') for d in (control, candidate))
        require(actions == other_actions and trace == other_trace, 'Per-frame game state or inputs differ')
        validate_trace(actions, left, trace)
        settings, other_settings = (read_json(d / 'run-settings.json') for d in (control, candidate))
        require(settings['width'] == 240 and other_settings['width'] == 356, 'Wrong control/candidate widths')
        settings, other_settings = dict(settings), dict(other_settings)
        settings.pop('width'); other_settings.pop('width')
        require(settings == other_settings and settings['trace_every_frame'] is True,
                'Different starting assets, native build, configuration or capture settings')
        require(set(settings['asset_sha256']) == {'rom', 'bios', 'state', 'runner', 'config'},
                'Missing input provenance')
        for folder in (control, candidate):
            require(read_json(folder / 'run-status.json')['execution_complete'] is True,
                    'Session execution did not complete')
        start = settings['capture_start']
        require(type(start) is int and 0 <= start < len(trace), 'Invalid capture start')
        if expectations is not None:
            require(start == expectations['gameplay_start_frame'], 'Capture warm-up differs from contract')
            cycle = check_car_cycle(actions, left, trace, expectations)
        shots, other_shots = (read_json(d / 'captures.json') for d in (control, candidate))
        require(shots == other_shots, 'Capture schedules differ')
        frames = [shot['elapsed_frame'] for shot in shots]
        require(frames == sorted(set(frames)) and set(frames) ==
                required_capture_frames(left, trace, start),
                'Missing, duplicate or unscheduled periodic/transition/action capture')
        for shot in shots:
            observed = trace[shot['elapsed_frame']]
            require(shot['frame'] == observed['frame'], 'Capture frame differs from trace')
            compare_image(local_file(control, shot['image'], '.png'),
                          local_file(candidate, shot['image'], '.png'))
            centers.append(True)
            compare_state(local_file(control, shot['state'], '.state'),
                          local_file(candidate, shot['state'], '.state'), observed)
            state_count += 1
    else:
        for index in range(len(left)):
            compare_image(control / f'step-{index:03}.png', candidate / f'step-{index:03}.png')
            centers.append(True)
    exact = compare_state(control / 'final.state', candidate / 'final.state',
                          trace[-1] if trace else None)
    state_count += 1
    for folder in (control, candidate):
        log = (folder / 'run.log').read_text()
        counters = re.findall(r'dispatch_misses=(\d+) interpreted_insns=(\d+) healed_native=(\d+)', log)
        require(counters and all(c == ('0', '0', '0') for c in counters),
                'Missing or nonzero static completion counters')
    log = (candidate / 'run.log').read_text()
    require('[sma3:wide]' in log, 'Missing widescreen instrumentation')
    gaps = re.findall(r'\[sma3:object-gap\] frame=(\d+) coherent=(\d+) unresolved=(\d+) visible_unresolved=(\d+)', log)
    require(len(gaps) == log.count('[sma3:object-gap]'), 'Incomplete gap instrumentation')
    require(all(g[1] in ('0', '1') and int(g[3]) <= int(g[2]) for g in gaps), 'Invalid gap counters')
    visible = sum(int(g[3]) > 0 for g in gaps)
    incoherent = sum(int(g[1]) != 1 for g in gaps)
    modes = [x['transformation'] for x in (trace or left) if 'transformation' in x]
    result = dict(level_id=left[0]['level_id'], sublevel_ids=sorted({x['sublevel_id'] for x in left}),
                  frames=sum(x['action']['n'] for x in left), central_images_equal=centers,
                  sampled_game_states=sorted({x['game_state'] for x in left}),
                  supported_gameplay_only=all(x['game_state'] == 13 for x in left),
                  sampled_transformations=sorted(set(modes)),
                  transformation_sequence=[key for key, _ in groupby(modes)],
                  final_transformation=left[-1].get('transformation'),
                  compared_state_sections=exact, compared_state_snapshots=state_count,
                  bus_exception='Inactive RTC wall-clock seconds only (8 bytes)',
                  viewport_state_compared=False, visible_unresolved_frames=visible,
                  incoherent_frames=incoherent, raw_gap_frames=len(gaps),
                  differential_pass=True, coverage_pass=not (visible or incoherent),
                  cycle_pass=None, complete_cycle_pass=None, synthetic_entry=True,
                  observation_interval_frames=1 if trace else None,
                  scope='Bounded session only; does not qualify full levels, bosses, all transformations or Android')
    if cycle is not None:
        result.update(cycle)
        result['complete_cycle_pass'] = cycle['cycle_pass'] and result['coverage_pass'] and not gaps
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('control', type=Path)
    p.add_argument('candidate', type=Path)
    p.add_argument('output', type=Path)
    p.add_argument('--expectations', type=Path)
    a = p.parse_args()
    if a.output.exists():
        p.error('Use a new report path; historical evidence must not be overwritten')
    try:
        expectations = read_json(a.expectations) if a.expectations else None
        result = compare(a.control, a.candidate, expectations)
        passed = result['coverage_pass'] and (expectations is None or result['complete_cycle_pass'])
    except (OSError, ValueError, KeyError, TypeError, IndexError, struct.error) as error:
        result = dict(status='failed', error=str(error), differential_pass=False, complete_cycle_pass=False)
        passed = False
    with a.output.open('x') as output:
        output.write(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))
    if not passed:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
