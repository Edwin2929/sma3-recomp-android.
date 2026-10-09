#!/usr/bin/env python3
"""Public synthetic tests of the validator, NOT evidence of actual game execution."""
import copy
import hashlib
import json
import struct
import subprocess
import sys
import tempfile
import unittest
from contextlib import contextmanager
from pathlib import Path
from PIL import Image
from compare_panorama_sessions import compare, compare_state
from make_level_fixture import sections
from panorama_session_checks import (action_digest, check_car_cycle, validate_actions,
                                     validate_expectations, validate_trace)
from run_panorama_session import RAM_START, RAM_END, execute_session

HERE = Path(__file__).resolve().parent
PLAN = json.loads((HERE / 'panorama-car-actions.json').read_text())
EXPECT = json.loads((HERE / 'car-cycle-expectations.json').read_text())


class FakeRunner:
    """Input-aware protocol stub; contains no ROM/BIOS or game implementation."""
    def __init__(self, recovery=2000, reentry=None, brief=False, stuck=False, bad_frame=None):
        self.elapsed = 0
        self.x = 208
        self.recovery, self.reentry = recovery, reentry
        self.brief, self.stuck, self.bad_frame = brief, stuck, bad_frame
        self.commands = []

    def mode(self):
        if self.elapsed < 670:
            return 0
        if self.elapsed < self.recovery:
            return 2
        if self.reentry is not None and (self.elapsed == self.reentry or
                                        (self.elapsed > self.reentry and not self.brief)):
            return 2
        return 0

    def __call__(self, **args):
        self.commands.append(dict(args))
        if args['cmd'] == 'run_frames':
            for _ in range(args['n']):
                self.elapsed += 1
                if self.elapsed == self.recovery:
                    self.x = 200
                elif args['keyinput'] == 1007 and not (self.stuck and self.elapsed > self.recovery):
                    self.x += 1
            return dict(ok=True, frame=5000 + self.elapsed)
        if args['cmd'] != 'read_iwram':
            raise AssertionError('Unexpected command: ' + str(args))
        if (args['addr'], args['len']) != (0x03000000 + RAM_START, RAM_END - RAM_START):
            raise AssertionError('Unexpected observation range')
        ram = bytearray(RAM_END - RAM_START)
        for offset, value, fmt in ((0x6288, 18, 'H'), (0x4cb8, 119, 'H'),
                                   (0x6b05, 10 if self.elapsed < 60 or self.elapsed == self.bad_frame else 13, 'B'),
                                   (0x6db2, self.mode(), 'H')):
            struct.pack_into('<' + fmt, ram, offset - RAM_START, value)
        struct.pack_into('<iiii', ram, 0x6d80 - RAM_START, self.x * 256, 1408 * 256, 0, 0)
        return dict(ok=True, data=ram.hex())


def simulate(plan=PLAN, **kwargs):
    fake = FakeRunner(**kwargs)
    evidence = dict(session=[], trace=[], captures=[])
    def capture(name, row, elapsed, with_state):
        result = dict(elapsed_frame=elapsed, frame=row['frame'], image=name + '.png')
        if with_state:
            result['state'] = name + '.state'
        return result
    execute_session(fake, plan, capture, evidence, True, 600)
    return fake, evidence


def state_bytes(row, wide=False):
    bus = bytearray(395416)
    base = 0x40000
    for offset, value, fmt in ((0x6288, row['level_id'], 'H'), (0x4cb8, row['sublevel_id'], 'H'),
                               (0x6b05, row['game_state'], 'B'), (0x6db2, row['transformation'], 'H')):
        struct.pack_into('<' + fmt, bus, base + offset, value)
    struct.pack_into('<iiii', bus, base + 0x6d80, *(int(v * 256) for v in row['player_xy_velocity']))
    bus[395310] = 7 if wide else 0  # The ONLY permitted inactive-RTC difference.
    data = {b'CPU0': b'fake CPU', b'BUS0': bus, b'PPU0': bytes([wide]) * 8,
            b'IO_0': b'fake I/O', b'AUD0': b'fake AUD', b'SAV0': b'fake SAV', b'META': b'fake MET'}
    result = bytearray(b'GBAS' + struct.pack('<I', 2) + b'7352d2bd064d9ebaec579e264228aa21c7345b80' + struct.pack('<I', len(data)))
    for tag, value in data.items():
        result += tag + struct.pack('<I', len(value)) + value
    return result


def dump(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')


@contextmanager
def restore(*paths):
    saved = {path: path.read_bytes() for path in paths}
    try:
        yield
    finally:
        for path, data in saved.items():
            path.write_bytes(data)


class InputAndCycleTests(unittest.TestCase):
    def test_reference_bytes_and_evidence_unchanged(self):
        for name, expected in (
            ('panorama-car-actions-reference.json', '85b0ec9ecebabccd477267ed891b37c2b09fe9e5'),
            ('widescreen-car.json', '367ff788ef54d39188bf6ae33d1eb51b1dadb012')):
            data = (HERE / name).read_bytes()
            actual = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
            self.assertEqual(expected, actual)
        old = json.loads((HERE / 'panorama-car-actions-reference.json').read_text())
        self.assertEqual((len(old), sum(a['n'] for a in old)), (38, 3360))
        self.assertEqual(PLAN[:7], old[:7])
        self.assertEqual(PLAN[7]['keys'], 1007)
        self.assertEqual(sum(a['n'] for a in PLAN), 5250)
        self.assertEqual(action_digest(PLAN), EXPECT['actions_sha256'])

    def test_action_schema_rejects_bad_inputs(self):
        cases = [None, {}, [], [None], [3], [{'n': True, 'keys': 1023}],
                 [{'n': 601, 'keys': 1023}], [{'n': 0, 'keys': 1023}],
                 [{'n': 1, 'keys': False}], [{'n': 1, 'keys': 1024}],
                 [{'n': 1, 'keys': 1023, 'extra': 0}],
                 [{'n': 1, 'keys': 1023, 'until_transformation': 0}],
                 [{'n': 1, 'keys': 1023, 'from_transformation': 0, 'until_transformation': 0}],
                 [{'n': 600, 'keys': 1023}] * 11]
        validate_actions(PLAN)
        for value in cases:
            with self.subTest(value=value), self.assertRaises(ValueError):
                validate_actions(value)

    def test_contract_cannot_be_weakened(self):
        for key, value in [('final_idle_frames', 1), ('escape_min_dx_pixels', 0),
                           ('gameplay_start_frame', 2000), ('sublevel_id', 0)]:
            with self.subTest(key=key), self.assertRaises(ValueError):
                validate_expectations(dict(EXPECT, **{key: value}))

    def test_complete_cycle_and_immediate_escape(self):
        fake, evidence = simulate()
        result = check_car_cycle(PLAN, evidence['session'], evidence['trace'], EXPECT)
        self.assertTrue(result['cycle_pass'])
        self.assertEqual(result['transformation_sequence'], [0, 2, 0])
        self.assertEqual(result['stable_normal_idle_frames'], 1200)
        self.assertEqual(result['final_dx_from_recovery'], 90)
        self.assertEqual(len(evidence['trace']), 3291)
        self.assertEqual(evidence['trace'][2001]['keys'], 1007)
        self.assertEqual(evidence['session'][8]['action']['n'], 440)
        self.assertEqual(set(c['cmd'] for c in fake.commands), {'run_frames', 'read_iwram'})
        self.assertTrue(all(c['n'] == 1 for c in fake.commands if c['cmd'] == 'run_frames'))

    def test_timeout_and_early_recovery_fail(self):
        for recovery, message in [(99999, 'timed out'), (1500, 'expected starting')]:
            with self.subTest(recovery=recovery), self.assertRaisesRegex(ValueError, message):
                simulate(recovery=recovery)

    def test_reentry_even_one_frame_is_rejected(self):
        for brief in (False, True):
            _, evidence = simulate(reentry=2301, brief=brief)
            result = check_car_cycle(PLAN, evidence['session'], evidence['trace'], EXPECT)
            self.assertFalse(result['cycle_pass'])
            self.assertEqual(result['final_transformation'], 0 if brief else 2)

    def test_no_escape_and_intermediate_death_are_rejected(self):
        for kwargs in ({'stuck': True}, {'bad_frame': 2301}):
            _, evidence = simulate(**kwargs)
            self.assertFalse(check_car_cycle(PLAN, evidence['session'], evidence['trace'], EXPECT)['cycle_pass'])

    def test_insufficient_final_idle_is_rejected(self):
        plan = copy.deepcopy(PLAN)
        plan[-1]['n'] = 599
        _, evidence = simulate(plan)
        expect = dict(EXPECT, actions_sha256=action_digest(plan))
        result = check_car_cycle(plan, evidence['session'], evidence['trace'], expect)
        self.assertEqual(result['stable_normal_idle_frames'], 1199)
        self.assertFalse(result['cycle_pass'])

    def test_missing_frame_input_mismatch_reset_and_wrong_checkpoint(self):
        _, base = simulate()
        for failure in ('missing', 'keys', 'reset', 'checkpoint', 'plan'):
            evidence = copy.deepcopy(base)
            plan = copy.deepcopy(PLAN)
            if failure == 'missing': evidence['trace'].pop(2301)
            if failure == 'keys': evidence['trace'][2301]['keys'] = 1007
            if failure == 'reset': evidence['trace'][2301]['frame'] = 0
            if failure == 'checkpoint': evidence['session'][-1]['transformation'] = 2
            if failure == 'plan': plan[0]['n'] -= 1
            with self.subTest(failure=failure), self.assertRaises(ValueError):
                validate_trace(plan, evidence['session'], evidence['trace'])

    def test_legacy_runner_keeps_batched_inputs(self):
        old = json.loads((HERE / 'panorama-car-actions-reference.json').read_text())
        fake = FakeRunner(reentry=2360)
        evidence = dict(session=[], trace=[], captures=[])
        shots = []
        execute_session(fake, old, lambda *args: shots.append(args), evidence)
        self.assertEqual(len(shots), 38)
        self.assertEqual(evidence['trace'], [])
        self.assertEqual([r['action'] for r in evidence['session']], old)
        self.assertEqual([c['n'] for c in fake.commands if c['cmd'] == 'run_frames'], [a['n'] for a in old])


class DifferentialTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.root = Path(cls.tmp.name)
        cls.control, cls.candidate = cls.root / 'control', cls.root / 'candidate'
        _, cls.evidence = simulate()
        for wide, folder in ((False, cls.control), (True, cls.candidate)):
            folder.mkdir()
            for name in ('session', 'trace', 'captures'):
                dump(folder / (name + '.json'), cls.evidence[name])
            dump(folder / 'actions.json', PLAN)
            dump(folder / 'run-status.json', {'execution_complete': True})
            dump(folder / 'run-settings.json', dict(width=356 if wide else 240, trace_every_frame=True,
                capture_start=600, asset_sha256={k: '0' * 64 for k in ('rom', 'bios', 'state', 'runner', 'config')}))
            (folder / 'run.log').write_text('[sma3:wide] synthetic test\ndispatch_misses=0 interpreted_insns=0 healed_native=0\n')
            for shot in cls.evidence['captures']:
                Image.new('RGB', (356 if wide else 240, 160), (1, 2, 3)).save(folder / shot['image'])
                row = cls.evidence['trace'][shot['elapsed_frame']]
                (folder / shot['state']).write_bytes(state_bytes(row, wide))
            (folder / 'final.state').write_bytes(state_bytes(cls.evidence['trace'][-1], wide))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_equivalent_images_states_trace_and_cycle(self):
        result = compare(self.control, self.candidate, EXPECT)
        self.assertTrue(result['complete_cycle_pass'])
        self.assertEqual(result['frames'], 3290)
        self.assertEqual(result['raw_gap_frames'], 0)
        self.assertEqual(result['compared_state_snapshots'], len(self.evidence['captures']) + 1)

    def test_central_image_mismatch(self):
        path = self.candidate / self.evidence['captures'][0]['image']
        with restore(path):
            with Image.open(path) as source:
                image = source.copy()
            image.putpixel((58, 0), (9, 9, 9)); image.save(path)
            with self.assertRaisesRegex(ValueError, 'Central image'):
                compare(self.control, self.candidate, EXPECT)

    def test_intermediate_guest_state_mismatch(self):
        path = self.candidate / self.evidence['captures'][0]['state']
        with restore(path):
            data = bytearray(path.read_bytes()); data[sections(bytes(data))[b'CPU0'][0]] ^= 1
            path.write_bytes(data)
            with self.assertRaisesRegex(ValueError, 'State differs'):
                compare(self.control, self.candidate, EXPECT)

    def test_matching_but_wrong_final_mode_in_saved_states(self):
        paths = [folder / 'final.state' for folder in (self.control, self.candidate)]
        with restore(*paths):
            for path in paths:
                data = bytearray(path.read_bytes())
                struct.pack_into('<H', data, sections(bytes(data))[b'BUS0'][0] + 0x40000 + 0x6db2, 2)
                path.write_bytes(data)
            with self.assertRaisesRegex(ValueError, 'Serialized game state'):
                compare(self.control, self.candidate, EXPECT)

    def test_active_rtc_and_nonclock_bus_difference_rejected(self):
        path = self.candidate / 'final.state'
        for offset in (395277, 395309, 395318):
            with self.subTest(offset=offset), restore(path):
                data = bytearray(path.read_bytes()); data[sections(bytes(data))[b'BUS0'][0] + offset] ^= 1
                path.write_bytes(data)
                with self.assertRaises(ValueError):
                    compare_state(self.control / 'final.state', path)

    def test_missing_transition_capture_on_both_sides(self):
        paths = [folder / 'captures.json' for folder in (self.control, self.candidate)]
        with restore(*paths):
            for path in paths:
                dump(path, [x for x in self.evidence['captures'] if x['elapsed_frame'] != 2000])
            with self.assertRaisesRegex(ValueError, 'capture'):
                compare(self.control, self.candidate, EXPECT)

    def test_input_provenance_and_incomplete_execution(self):
        for name, value in [('run-status', {'execution_complete': False}),
                            ('run-settings', {'width': 356, 'trace_every_frame': False})]:
            path = self.candidate / (name + '.json')
            with self.subTest(name=name), restore(path):
                dump(path, value)
                with self.assertRaises(ValueError):
                    compare(self.control, self.candidate, EXPECT)

    def test_raw_visible_and_incoherent_gaps_cannot_qualify(self):
        path = self.candidate / 'run.log'
        for coherent, unresolved, visible in ((1, 1, 0), (1, 1, 1), (0, 0, 0)):
            with self.subTest(values=(coherent, unresolved, visible)), restore(path):
                with path.open('a') as out:
                    out.write(f'[sma3:object-gap] frame=7301 coherent={coherent} unresolved={unresolved} visible_unresolved={visible}\n')
                self.assertFalse(compare(self.control, self.candidate, EXPECT)['complete_cycle_pass'])

    def test_missing_malformed_instrumentation_and_static_failures(self):
        for folder, text in ((self.candidate, 'dispatch_misses=0 interpreted_insns=0 healed_native=0'),
                             (self.candidate, '[sma3:wide]\ndispatch_misses=0 interpreted_insns=0 healed_native=0\n[sma3:object-gap] malformed'),
                             (self.control, 'dispatch_misses=1 interpreted_insns=0 healed_native=0')):
            path = folder / 'run.log'
            with self.subTest(text=text), restore(path):
                path.write_text(text)
                with self.assertRaises(ValueError):
                    compare(self.control, self.candidate, EXPECT)

    def test_legacy_samples_cannot_certify_complete_cycle(self):
        with tempfile.TemporaryDirectory() as temp:
            folders = [Path(temp) / name for name in ('control', 'candidate')]
            for folder in folders:
                folder.mkdir(); dump(folder / 'session.json', self.evidence['session'])
            with self.assertRaisesRegex(ValueError, 'per-frame'):
                compare(*folders, EXPECT)

    def test_cli_protects_existing_report_and_emits_real_failure(self):
        with tempfile.TemporaryDirectory() as temp:
            report = Path(temp) / 'report.json'
            report.write_text('do not overwrite')
            command = [sys.executable, str(HERE / 'compare_panorama_sessions.py'), 'missing-control', 'missing-candidate', str(report)]
            result = subprocess.run(command, capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(report.read_text(), 'do not overwrite')
            report.unlink()
            result = subprocess.run(command, capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse(json.loads(report.read_text())['differential_pass'])


if __name__ == '__main__':
    unittest.main()
