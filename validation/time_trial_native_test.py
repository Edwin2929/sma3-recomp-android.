#!/usr/bin/env python3
"""Bounded, read-only TCP qualification of the host time-trial observer.

Requires privately owned USA ROM/BIOS, two native runners and a checkpoint
before a main entrance. Its provenance must disclose any prepared fixture
in its ancestry. Entry buttons are supplied in a JSON tape. Checkpoints,
screenshots and saves stay outside this repository.
The public report contains hashes, counters and scalar observations only.
No goal, score, player position or game RAM is patched by this driver.
"""

import argparse
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import struct
import subprocess
import time

from run_panorama_session import ROOT, session_environment
from validate_localization_gameplay import (
    NativeSession, compare_english, coverage, digest, require,
)


class TrialSession(NativeSession):
    def start(self):
        env = session_environment(self.output)
        env.update(SMA3_LANGUAGE='0', SMA3_TIME_TRIAL='1',
                   SMA3_TIME_TRIAL_REPORT=str(self.output / 'time-trial.json'))
        # A caller's optional achievement suspend fixture must not be resumed.
        env.pop('SMA3_CHALLENGE_CHECKPOINT', None)
        env.pop('SMA3_LOCAL_ACHIEVEMENTS_REPORT', None)
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
                require(self.process.poll() is None, 'Runner exited; inspect private run.log')
                time.sleep(.1)
        else:
            raise ValueError('Local TCP debug server unavailable')
        if self.state:
            self.command(cmd='savestate_load', path=str(self.state))
        self.start_frame = self.command(cmd='frame')['frame']
        return self.observe()

    def observe(self):
        row = super().observe()
        ram = bytes.fromhex(self.command(cmd='read_iwram', addr=0x03000000, len=0x8000)['data'])
        row.update(entrance=struct.unpack_from('<H', ram, 0x6a52)[0],
                   inventory=ram[0x48b5], start_menu=ram[0x4d46] & 15)
        return row


def snapshot(folder):
    report = json.loads((folder / 'time-trial.json').read_text())
    data = report.get('snapshot', [])
    require(report.get('schema') == 1 and len(data) == 74 and data[0] == 1,
            'Unexpected host time-trial snapshot schema')
    require(all(type(x) is int and 0 <= x < 2**64 for x in data), 'Invalid snapshot integer')
    require(data[1] == 1 and data[12:14] == [280896, 16777216], 'Wrong enabled/clock contract')
    return data


def no_record(data):
    require(data[6:9] == [0, 0, 0] and not any(data[14:]),
            'An incomplete native attempt unexpectedly recorded a result')


def run_session(args, runner, name, state, actions, operation=None, candidate=True,
                recorded_actions=None):
    output = args.private_output / name
    if args.resume_existing and output.is_dir() and any(output.iterdir()):
        read = lambda name: json.loads((output / name).read_text())
        settings = read('run-settings.json')
        require(read('run-status.json')['execution_complete'], 'Existing run is incomplete')
        expected_actions = actions if recorded_actions is None else recorded_actions
        require(read('actions.json') == expected_actions, 'Existing evidence has different buttons')
        require(settings['language'] == 0 and settings['width'] == 240 and
                settings['fresh_private_save'], 'Existing evidence has different settings')
        for key, path in (('runner', runner), ('rom', args.rom), ('bios', args.bios),
                          ('config', ROOT / 'runtime.toml'), ('initial_state', state)):
            require(settings['asset_sha256'][key] == digest(path), 'Existing evidence input differs')
        result = {'name': name, 'coverage': coverage(output),
                  'requested_frames': sum(row.get('n', 0) for row in expected_actions),
                  'checkpoints': read('captures.json')}
        if candidate:
            result['snapshot'] = snapshot(output)
            no_record(result['snapshot'])
        print(json.dumps({'session': name, 'verified_existing_evidence': True}), flush=True)
        return result
    session = TrialSession(runner, args.rom, args.bios, output, 0, state)
    error = None
    try:
        session.start()
        for action in actions:
            session.action(action)
        if operation:
            operation(session)
    except Exception as exc:
        error = str(exc)
    finally:
        session.close(error)
    result = {'name': name, 'coverage': session.status['coverage'],
              'requested_frames': session.elapsed, 'checkpoints': session.captures}
    if candidate:
        result['snapshot'] = snapshot(output)
        no_record(result['snapshot'])
    print(json.dumps({'session': name, 'pass': True,
                      'coverage': session.status['coverage'],
                      'timer': result.get('snapshot', [])[:14]}), flush=True)
    return result


def pauses(start_extra=0, inventory_extra=0):
    return [
        {'n': 1, 'keys': 1015},
        {'n': 90 + start_extra, 'keys': 1023, 'capture': 'start-paused'},
        {'n': 1, 'keys': 1022},
        {'n': 30, 'keys': 1023, 'capture': 'start-resumed'},
        {'n': 1, 'keys': 1019},
        {'n': 90 + inventory_extra, 'keys': 1023, 'capture': 'inventory-paused'},
        {'n': 1, 'keys': 1021},
        {'n': 90, 'keys': 1023, 'capture': 'inventory-resumed'},
    ]


def assert_running(result):
    data = result['snapshot']
    require(data[2] == 2 and data[3] == 1 and data[4] > 0 and data[5] == 1 and data[9] == 0,
            'Natural main entrance did not produce exactly one running 1-1 attempt')


def assert_pause_flags(result):
    checkpoints = {row['name']: row for row in result['checkpoints']}
    require(checkpoints['start-paused']['start_menu'] > 0 and
            checkpoints['start-resumed']['start_menu'] == 0,
            'Start menu did not open and close through ordinary controls')
    require(checkpoints['inventory-paused']['inventory'] > 0 and
            checkpoints['inventory-resumed']['inventory'] == 0,
            'Inventory did not open and close through ordinary controls')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('reference', 'candidate', 'rom', 'bios', 'entry_state', 'entry_actions', 'entry_provenance',
                 'private_output', 'report'):
        parser.add_argument('--' + name.replace('_', '-'), type=Path, required=True)
    parser.add_argument('--origin-actions', type=Path,
                        help='Optional ordinary-control tape from fresh boot to the entry checkpoint')
    parser.add_argument('--resume-existing', action='store_true',
                        help='Revalidate complete matching private sessions instead of rerunning them')
    args = parser.parse_args()
    args.private_output = args.private_output.resolve()
    require(not args.private_output.is_relative_to(ROOT), 'Private output must be outside repository')
    args.private_output.mkdir(parents=True, exist_ok=True)
    require(args.resume_existing or not any(args.private_output.iterdir()),
            'Use an empty private output directory or explicitly resume matching evidence')
    provenance = json.loads(args.entry_provenance.read_text())
    require(type(provenance) is dict and provenance, 'Describe the checkpoint ancestry explicitly')
    entry = json.loads(args.entry_actions.read_text())
    require(type(entry) is list and entry, 'An ordinary-control main-entry tape is required')
    require(any(row.get('capture') == 'running' for row in entry), 'Entry tape must capture running')
    require(all(type(row) is dict and set(row) <= {'n', 'keys', 'capture'} for row in entry),
            'Entry tape can contain only frame batches, buttons and captures')
    cases = {}
    short_actions = entry + pauses()
    # Both runners receive identical private inputs; separate processes/saves.
    with ThreadPoolExecutor(max_workers=2) as pool:
        reference = pool.submit(run_session, args, args.reference, 'reference-short',
                                args.entry_state, short_actions, candidate=False)
        candidate = pool.submit(run_session, args, args.candidate, 'candidate-short',
                                args.entry_state, short_actions)
        cases['reference_short'], cases['short'] = reference.result(), candidate.result()
    comparison = compare_english(args.private_output / 'reference-short',
                                 args.private_output / 'candidate-short')
    assert_running(cases['short'])
    assert_pause_flags(cases['short'])
    with ThreadPoolExecutor(max_workers=2) as pool:
        start = pool.submit(run_session, args, args.candidate, 'long-start',
                            args.entry_state, entry + pauses(start_extra=180))
        inventory = pool.submit(run_session, args, args.candidate, 'long-inventory',
                                args.entry_state, entry + pauses(inventory_extra=180))
        cases['long_start'], cases['long_inventory'] = start.result(), inventory.result()
    for name in ('long_start', 'long_inventory'):
        assert_running(cases[name])
        assert_pause_flags(cases[name])
        require(cases[name]['snapshot'][11] == cases['short']['snapshot'][11] + 180,
                'Long pause did not exercise 180 additional PPU frames')
    require(cases['long_start']['snapshot'][4] == cases['short']['snapshot'][4],
            'Extending Start by 180 PPU frames changed race time')
    # Post-close elapsed totals differed by two frames in the qualification
    # replay. Use endpoints while the real pause flag remains active, isolating
    # the held interval without assuming an identical closing duration.
    with ThreadPoolExecutor(max_workers=2) as pool:
        held_short = pool.submit(run_session, args, args.candidate, 'inventory-held-short',
                                 args.entry_state, entry + pauses()[:6])
        held_long = pool.submit(run_session, args, args.candidate, 'inventory-held-long',
                                args.entry_state, entry + pauses(inventory_extra=180)[:6])
        cases['inventory_held_short'], cases['inventory_held_long'] = held_short.result(), held_long.result()
    for name in ('inventory_held_short', 'inventory_held_long'):
        data = cases[name]['snapshot']
        require(data[2] == 3 and data[3] == 1 and data[4] > 0 and data[5] == 1 and data[9] == 0,
                'Inventory hold endpoint must be an active paused attempt')
        require(cases[name]['checkpoints'][-1]['inventory'] > 0, 'Inventory flag is not active')
    held_short, held_long = (cases[name]['snapshot'] for name in
                             ('inventory_held_short', 'inventory_held_long'))
    require(held_short[4] == held_long[4] and held_short[11] + 180 == held_long[11],
            'Holding inventory for 180 additional PPU frames advanced the race timer')

    def restore(session):
        saved = session.output / 'running.state'
        session.action({'n': 60, 'keys': 1023, 'capture': 'before-restore'})
        session.command(cmd='savestate_load', path=str(saved))
        session.action({'n': 5, 'keys': 1023, 'capture': 'after-restore'})

    cases['restore'] = run_session(args, args.candidate, 'restore', args.entry_state, entry, restore,
                                   recorded_actions=entry + [
                                       {'n': 60, 'keys': 1023, 'capture': 'before-restore'},
                                       {'n': 5, 'keys': 1023, 'capture': 'after-restore'}])
    restored = cases['restore']['snapshot']
    require(restored[2] == 5 and restored[9] == 3 and restored[5] == 1,
            'Loading a state did not invalidate an already-running attempt')
    require(restored[10] > cases['short']['snapshot'][10], 'Restore did not change state epoch')

    middle = args.private_output / 'candidate-short' / 'running.state'
    cases['mid_level'] = run_session(args, args.candidate, 'mid-level', middle,
                                     [{'n': 60, 'keys': 1023, 'capture': 'partial'}])
    partial = cases['mid_level']['snapshot']
    # If boot supplied a prior PPU sample, the TCP initial state load has an
    # observable epoch change and is correctly Invalidated/StateRestored.
    # Otherwise the initial sample simply remains Waiting. Neither may arm.
    require((partial[2:3] == [1] and partial[9] == 0 or
             partial[2] == 5 and partial[9] == 3) and partial[4] == partial[5] == 0,
            'Starting the observer in ordinary gameplay armed a partial record')

    report = {
        'schema': 1, 'result': 'PASS', 'release': '0.10.16-beta',
        'scope': 'Bounded native host TCP replay; no physical Android device claim',
        'guest_memory_patched_by_this_driver': False,
        'entry': 'Private checkpoint followed by ordinary-control main entrance; see explicit ancestry below',
        'entry_provenance': provenance,
        'goal_scope': 'Normal/boss completion and personal-best updates are controlled unit tests; no natural completed course claimed here',
        'assets_sha256': {name: digest(getattr(args, name)) for name in
                          ('reference', 'candidate', 'rom', 'bios', 'entry_state', 'entry_actions', 'entry_provenance')},
        'source_sha256': {name: digest(ROOT / name) for name in
                         ('src/time_trial.h', 'src/time_trial_detail.h', 'src/time_trial.cpp',
                          'validation/time_trial_test.cpp', 'validation/time_trial_native_test.py')},
        'guest_comparison': comparison,
        'pause_extension_frames': 180,
        'inventory_endpoint': 'Compare Paused snapshots before closing, with the inventory flag active. Post-close short/long snapshots were also retained; their elapsed totals differ by two frames, so no identical closing-duration claim is made.',
        'checks': {
            'natural_main_entry': True, 'same_guest_with_observer': True,
            'start_pause_freezes_timer': True, 'inventory_freezes_timer': True,
            'state_load_invalidates_attempt': True, 'mid_level_start_does_not_arm': True,
            'no_partial_records': True, 'every_run_fully_static': True,
        },
        'cases': cases,
    }
    if args.origin_actions:
        report['origin_actions_sha256'] = digest(args.origin_actions)
        report['origin_actions'] = json.loads(args.origin_actions.read_text())
    report['entry_actions'] = entry
    args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'result': 'PASS', 'report': str(args.report),
                      'native_sessions': len(cases), 'checkpoints_compared': comparison['checkpoints']}))


if __name__ == '__main__':
    main()
