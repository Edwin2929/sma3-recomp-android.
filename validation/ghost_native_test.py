#!/usr/bin/env python3
"""Qualify a complete native 1-1 ghost from ordinary buttons, then reopen it.

The input checkpoint's ancestry must be disclosed. This driver loads it once
per process, retries through the game's main entrance, and never writes guest
RAM or loads an intermediate checkpoint. Route exploration is not evidence of
an uninterrupted record; only the full runs produced here are. ROM, BIOS,
screenshots, GBAS checkpoints, SRAM and ghost motion remain private.
"""

import argparse
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import struct
import subprocess
import time

from run_panorama_session import ROOT, session_environment
from time_trial_native_test import TrialSession, snapshot
from validate_localization_gameplay import compare_english, digest, require


class GhostSession(TrialSession):
    def __init__(self, *args, ghost_directory=None, bests=None, **kwargs):
        super().__init__(*args, **kwargs)
        self.ghost_directory, self.bests = ghost_directory, bests

    def start(self):
        env = session_environment(self.output)
        for name in tuple(env):
            if name.startswith('SMA3_TIME_TRIAL') or name in (
                    'SMA3_CHALLENGE_CHECKPOINT', 'SMA3_LOCAL_ACHIEVEMENTS_REPORT'):
                env.pop(name)
        env.update(SMA3_LANGUAGE='0', SMA3_IMAGE_MODE='0', SMA3_TIME_TRIAL='1',
                   SMA3_TIME_TRIAL_REPORT=str(self.output / 'time-trial.json'))
        if self.ghost_directory is not None:
            env.update(SMA3_TIME_TRIAL_GHOST_DIR=str(self.ghost_directory),
                       SMA3_TIME_TRIAL_GHOST_BESTS=str(self.bests),
                       SMA3_TIME_TRIAL_GHOST_REPORT=str(self.output / 'ghost.json'))
        self.log = (self.output / 'run.log').open('w')
        command = [self.runner.name, '--config', str(self.config), '--rom', str(self.rom),
                   '--bios', str(self.bios), '--tcp', str(self.port), '--view-width', '240',
                   '--save-path', str(self.output / 'fresh.sav')]
        self.process = subprocess.Popen(command, executable=str(self.runner), cwd=self.output,
                                        env=env, stdout=self.log, stderr=subprocess.STDOUT)
        for _ in range(100):
            try:
                self.command(cmd='ping')
                break
            except OSError:
                require(self.process.poll() is None, 'Runner exited; inspect the private log')
                time.sleep(.1)
        else:
            raise ValueError('Local TCP diagnostic server unavailable')
        # This is the only state load in each complete qualification process.
        self.command(cmd='savestate_load', path=str(self.state))
        self.start_frame = self.command(cmd='frame')['frame']
        return self.observe()

    def observe(self):
        row = super().observe()
        ram = bytes.fromhex(self.command(cmd='read_iwram', addr=0x03000000, len=0x8000)['data'])
        row['player_xy_velocity'] = [v / 256 for v in struct.unpack_from('<iiii', ram, 0x6d80)]
        row['transformation'] = struct.unpack_from('<H', ram, 0x6db2)[0]
        return row


def ghost_snapshot(folder):
    report = json.loads((folder / 'ghost.json').read_text())
    data = report.get('snapshot', [])
    require(report.get('schema') == 1 and len(data) == 72 and data[:3] == [1, 1, 1],
            'Unexpected enabled, synchronized ghost snapshot')
    require(all(type(value) is int and 0 <= value < 2**64 for value in data),
            'Invalid ghost snapshot word')
    return data


def run(args, runner, name, actions, ghost_directory=None, bests=None):
    output = args.private_output / name
    session = GhostSession(runner, args.rom, args.bios, output, 0, args.entry_state,
                           ghost_directory=ghost_directory, bests=bests)
    error = None
    try:
        session.start()
        for action in actions:
            session.action(action)
    except Exception as exc:
        error = str(exc)
    finally:
        session.close(error)
    result = {'name': name, 'coverage': session.status['coverage'],
              'requested_frames': session.elapsed, 'checkpoints': session.captures,
              'timer': snapshot(output), 'initial_loads': 1, 'intermediate_loads': 0}
    if ghost_directory is not None:
        result['ghost'] = ghost_snapshot(output)
    print(json.dumps({'session': name, 'coverage': result['coverage'],
                      'timer': result['timer'][:15], 'ghost': result.get('ghost', [])[:13]}),
          flush=True)
    return result


def files(directory):
    return [{'name': path.name, 'bytes': path.stat().st_size, 'sha256': digest(path)}
            for path in sorted(directory.iterdir()) if path.is_file()]


def complete(result):
    timer = result['timer']
    require(timer[2] == 4 and timer[3] == 1 and timer[4] > 0 and timer[5] == 1 and
            timer[6] == 1 and timer[7] == 1 and timer[8] == timer[4] and timer[9] == 0 and
            timer[14] == timer[4] and not any(timer[15:]),
            'The uninterrupted retry did not finish exactly one valid 1-1 attempt')
    require(result['checkpoints'][-1]['game_state'] == 14,
            'The guest did not reach its natural goal sequence')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('reference', 'candidate', 'rom', 'bios', 'entry_state', 'entry_provenance',
                 'private_output', 'report'):
        parser.add_argument('--' + name.replace('_', '-'), type=Path, required=True)
    parser.add_argument('--actions', type=Path, default=ROOT / 'validation/ghost-course-1-1.json')
    args = parser.parse_args()
    args.private_output = args.private_output.resolve()
    require(not args.private_output.is_relative_to(ROOT), 'Private evidence must stay outside the repo')
    args.private_output.mkdir(parents=True, exist_ok=True)
    require(not any(args.private_output.iterdir()), 'Use an empty private output directory')
    provenance = json.loads(args.entry_provenance.read_text())
    require(type(provenance) is dict and provenance, 'Explicit entry ancestry is required')
    require(provenance['checkpoint']['sha256'] == digest(args.entry_state),
            'Entry checkpoint differs from its disclosed ancestry')
    actions = json.loads(args.actions.read_text())
    require(type(actions) is list and len(actions) > 2 and
            all(type(row) is dict and set(row) <= {'n', 'keys', 'capture'} for row in actions),
            'The route must contain only button/frame batches and observations')
    require(actions[1].get('capture') == 'running', 'Expected the verified main-entry prefix')
    ghost_directory = args.private_output / 'ghost-store'
    ghost_directory.mkdir(mode=0o700)
    empty_bests = args.private_output / 'empty-bests.txt'
    empty_bests.write_text(' '.join(['0'] * 60) + '\n')
    empty_bests.chmod(0o600)
    with ThreadPoolExecutor(max_workers=2) as pool:
        reference_future = pool.submit(run, args, args.reference, 'reference-full', actions)
        candidate_future = pool.submit(run, args, args.candidate, 'candidate-full', actions,
                                       ghost_directory, empty_bests)
        reference, candidate = reference_future.result(), candidate_future.result()
    complete(reference)
    complete(candidate)
    comparison = compare_english(args.private_output / 'reference-full',
                                 args.private_output / 'candidate-full')
    require(reference['timer'] == candidate['timer'], 'Ghost recording changed the timer result')
    duration = candidate['timer'][4]
    require(candidate['ghost'][5] == 2 and candidate['ghost'][10] == 0 and
            candidate['ghost'][12] == duration and not any(candidate['ghost'][13:]),
            'The completed best did not persist as an eligible ghost')
    saved = files(ghost_directory)
    require(len(saved) == 1 and saved[0]['bytes'] > 0, 'Expected one complete private ghost file')
    bests = args.private_output / 'persisted-bests.txt'
    bests.write_text(' '.join(map(str, [duration] + [0] * 59)) + '\n')
    bests.chmod(0o600)
    # Reopen the real result in a new process. Delay this retry so the ghost
    # follows its saved clock while the player takes different ordinary inputs.
    delayed = actions[:2] + [{'n': 60, 'keys': 1023}] + [
        {key: value for key, value in action.items() if key != 'capture'}
        for action in actions[2:8]]
    delayed[-1]['capture'] = 'delayed-retry'
    (args.private_output / 'delayed-retry-actions.json').write_text(json.dumps(delayed, indent=2) + '\n')
    print(json.dumps({'saved_ghost_ready': str(ghost_directory),
                      'persisted_bests': str(bests), 'duration_frames': duration}), flush=True)
    reopened = run(args, args.candidate, 'candidate-reopened', delayed, ghost_directory, bests)
    timer, ghost = reopened['timer'], reopened['ghost']
    require(timer[2] == 2 and timer[3] == 1 and timer[5] == 1 and timer[6] == 0 and timer[9] == 0,
            'Reopened ordinary retry did not start a valid new attempt')
    require(ghost[5] == 2 and ghost[10] == 0 and ghost[11] == 1 and ghost[12] == duration,
            'The saved motion was not loaded and eligible after reopening')
    require(files(ghost_directory) == saved, 'The incomplete delayed retry changed the saved best')
    report = {
        'schema': 1, 'result': 'PASS', 'release': '0.10.17-beta',
        'scope': 'Native host TCP gameplay and persistence; separate SDL presentation evidence; no physical Android or performance claim',
        'guest_memory_patched_by_this_driver': False,
        'entry_provenance': provenance,
        'route_provenance': 'Exploration used private segment checkpoints. Both full qualification runs replay the entire concatenated ordinary-button tape from Try Again with no intermediate load. The retry initializes 1-1 naturally, recovers Baby Mario after real damage, and reaches the actual goal without another death.',
        'entry_provenance_limit_note': 'The older ancestry document describes its own incomplete 0.10.16 test. This report adds the completed retry below; it does not convert the prepared initial ancestry into a fresh-boot campaign.',
        'assets_sha256': {name: digest(getattr(args, name)) for name in
                          ('reference', 'candidate', 'rom', 'bios', 'entry_state',
                           'entry_provenance', 'actions')},
        'source_sha256': {name: digest(ROOT / name) for name in
                         ('src/time_trial.cpp', 'src/time_trial_ghost.cpp', 'src/time_trial_ghost_detail.h',
                          'src/time_trial_ghost_storage.h',
                          'src/ghost_visual.cpp', 'validation/ghost_native_test.py',
                          'validation/ghost-course-1-1.json')},
        'guest_comparison': comparison,
        'best_frames': duration,
        'best_seconds': duration * 280896 / 16777216,
        'saved_private_ghost_files': saved,
        'checks': {'full_course_real_goal': True, 'single_attempt_no_death': True,
                   'no_intermediate_state_load': True, 'same_guest_images_and_state': True,
                   'same_time_as_0_10_16': True, 'completed_ghost_saved': True,
                   'reopened_matching_best_eligible': True, 'partial_retry_preserved_best': True},
        'sessions': [reference, candidate, reopened],
        'presentation_note': 'Ghost sprites are host overlays and are absent from guest TCP screenshots; inspect the separately captured real SDL drawable for visibility and separation.'}
    args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'result': 'PASS', 'best_frames': duration, 'report': str(args.report)}), flush=True)


if __name__ == '__main__':
    main()
