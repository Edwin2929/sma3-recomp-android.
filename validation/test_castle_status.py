#!/usr/bin/env python3
"""Synthetic tests of optional castle health parsing, not native route evidence."""
import copy
import json
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
import run_panorama_session as runner
from castle_route_checks import (CASTLE_STATUS_PROFILE, GAME_POINTER, IWRAM_BASE,
                                 IWRAM_SIZE, parse_castle_status, validate_castle_status)
from compare_panorama_sessions import compare, compare_state
from make_level_fixture import sections
from panorama_session_checks import validate_trace
from test_car_cycle import EXPECT, dump, restore, state_bytes
from test_panorama_transitions import ACTIONS, write_pair


HEALTH = dict(game_pointer=GAME_POINTER, lives=3, deaths=0, inhibition=0, stars=109,
              entry_type=1, exit_index=0, baby_id=97, baby_status=5)


def iwram_bytes(status=None, game_state=13, row=None):
    status = dict(HEALTH, **(status or {}))
    row = row or dict(level_id=3, sublevel_id=3, transformation=0, player_xy_velocity=[8, 100, 0, 0])
    raw = bytearray(IWRAM_SIZE)
    for offset, value, fmt in ((0x7240, status['game_pointer'], 'I'),
                               (0x627E, status['lives'], 'h'), (0x634C, status['deaths'], 'H'),
                               (0x6351, status['inhibition'], 'B'), (0x6ACE, status['stars'], 'H'),
                               (0x6A52, status['entry_type'], 'H'), (0x6b05, game_state, 'B'),
                               (0x6288, row['level_id'], 'H'), (0x4cb8, row['sublevel_id'], 'H'),
                               (0x6db2, row['transformation'], 'H')):
        struct.pack_into('<' + fmt, raw, offset, value)
    base = status['game_pointer'] - IWRAM_BASE
    for offset, name in ((0x3E, 'exit_index'), (0x254 + 0x32, 'baby_id'), (0x254 + 0x24, 'baby_status')):
        struct.pack_into('<H', raw, base + offset, status[name])
    struct.pack_into('<iiii', raw, 0x6d80, *(int(v * 256) for v in row['player_xy_velocity']))
    return raw


def castle_state_bytes(row, wide=False):
    state = bytearray(state_bytes(row, wide))
    base = sections(bytes(state))[b'BUS0'][0] + 0x40000
    state[base:base + IWRAM_SIZE] = iwram_bytes(row['castle_status'], row['game_state'], row)
    return state


class CastleRunner:
    def __init__(self):
        self.elapsed = 0
        self.commands = []
        self.invalid_pointer = False

    def __call__(self, **args):
        self.commands.append(dict(args))
        if args['cmd'] == 'run_frames':
            self.elapsed += args['n']
            return dict(ok=True, frame=1000 + self.elapsed)
        if args['cmd'] != 'read_iwram':
            raise AssertionError('Unexpected native command')
        row = dict(level_id=3, sublevel_id=3, transformation=0,
                   player_xy_velocity=[self.elapsed, 100, 0, 0])
        raw = iwram_bytes(row=row)
        if self.invalid_pointer:
            struct.pack_into('<I', raw, 0x7240, 0)
        offset = args['addr'] - IWRAM_BASE
        if not 0 <= offset <= len(raw) - args['len']:
            raise AssertionError('Observation outside IWRAM')
        return dict(ok=True, data=raw[offset:offset + args['len']].hex())


def castle_evidence():
    fake = CastleRunner()
    evidence = dict(session=[], trace=[], captures=[])
    def capture(name, row, elapsed, with_state):
        return dict(elapsed_frame=elapsed, frame=row['frame'], image=name + '.png', state=name + '.state')
    enabled = runner.execute_session(fake, ACTIONS, capture, evidence, castle_status=True)
    return fake, evidence, enabled


class CastleParserTests(unittest.TestCase):
    def test_exact_offsets_little_endian_and_signed_lives(self):
        expected = dict(HEALTH, lives=-2, deaths=0x4321, inhibition=0xa5, stars=0x7654,
                        entry_type=0x1234, exit_index=0x9abc, baby_id=0xabcd, baby_status=0x1357)
        self.assertEqual(parse_castle_status(iwram_bytes(expected)), expected)
        validate_castle_status(expected, 13)

    def test_truncated_or_extra_iwram_is_rejected(self):
        for length in (0, IWRAM_SIZE - 1, IWRAM_SIZE + 1):
            with self.subTest(length=length), self.assertRaisesRegex(ValueError, '32768'):
                parse_castle_status(bytes(length))

    def test_pointer_bounds_alignment_and_gameplay_identity(self):
        for pointer in (0, 0x02fffffc, 0x0300220e, 0x03005554, 0x03008000, 0xffffffff):
            raw = iwram_bytes(game_state=12)
            struct.pack_into('<I', raw, 0x7240, pointer)
            with self.subTest(pointer=pointer), self.assertRaisesRegex(ValueError, 'pointer'):
                parse_castle_status(raw)
        for pointer in (IWRAM_BASE, 0x03005550):
            with self.subTest(valid_pointer=pointer):
                raw = iwram_bytes({'game_pointer': pointer}, game_state=12)
                self.assertEqual(parse_castle_status(raw)['game_pointer'], pointer)
                raw[0x6b05] = 13
                with self.assertRaisesRegex(ValueError, 'gameplay'):
                    parse_castle_status(raw)

    def test_json_status_rejects_partial_extra_and_noninteger_fields(self):
        missing = dict(HEALTH); missing.pop('lives')
        cases = [missing, dict(HEALTH, extra=1), dict(HEALTH, lives=65535),
                 dict(HEALTH, lives=-32769), dict(HEALTH, lives=True), dict(HEALTH, deaths=-1),
                 dict(HEALTH, inhibition=256), dict(HEALTH, baby_id=97.0),
                 dict(HEALTH, baby_status='5'), dict(HEALTH, game_pointer=GAME_POINTER + 2)]
        for status in cases:
            with self.subTest(status=status), self.assertRaises(ValueError):
                validate_castle_status(status, 13)


class CastleObservationTests(unittest.TestCase):
    def test_default_observation_keeps_existing_window_and_fields(self):
        fake = CastleRunner(); fake.invalid_pointer = True
        observed = runner.observe(fake)
        self.assertEqual(set(observed), {'player_xy_velocity', 'level_id', 'sublevel_id',
                                        'game_state', 'transformation'})
        self.assertEqual(fake.commands, [dict(cmd='read_iwram', addr=IWRAM_BASE + runner.RAM_START,
                                              len=runner.RAM_END - runner.RAM_START)])
        with self.assertRaisesRegex(ValueError, 'pointer'):
            runner.observe(fake, castle_status=True)

    def test_opt_in_automatically_traces_full_snapshots_and_checkpoints(self):
        fake, evidence, enabled = castle_evidence()
        self.assertTrue(enabled)
        self.assertEqual(len(evidence['trace']), 9)
        self.assertTrue(all(row['castle_status'] == HEALTH for row in evidence['trace'] + evidence['session']))
        self.assertTrue(all(command['n'] == 1 for command in fake.commands if command['cmd'] == 'run_frames'))
        self.assertTrue(all((command['addr'], command['len']) == (IWRAM_BASE, IWRAM_SIZE)
                            for command in fake.commands if command['cmd'] == 'read_iwram'))
        self.assertEqual(validate_trace(ACTIONS, evidence['session'], evidence['trace']), 8)

    def test_partial_histories_and_wrong_endpoint_are_rejected(self):
        _, original, _ = castle_evidence()
        for where, index in (('trace', 0), ('trace', 2), ('trace', -1), ('session', 0)):
            evidence = copy.deepcopy(original)
            evidence[where][index].pop('castle_status')
            with self.subTest(where=where, index=index), self.assertRaisesRegex(ValueError, 'Partial'):
                validate_trace(ACTIONS, evidence['session'], evidence['trace'])
        evidence = copy.deepcopy(original)
        evidence['session'][0]['castle_status'] = dict(HEALTH, lives=2)
        with self.assertRaisesRegex(ValueError, 'checkpoint'):
            validate_trace(ACTIONS, evidence['session'], evidence['trace'])
        evidence = copy.deepcopy(original)
        evidence['trace'][2]['castle_status'] = dict(HEALTH, lives='3')
        with self.assertRaisesRegex(ValueError, 'integer'):
            validate_trace(ACTIONS, evidence['session'], evidence['trace'])

    def test_car_contract_and_castle_flag_are_mutually_exclusive(self):
        flags = ['-B'] + (['-O'] if sys.flags.optimize else [])
        args = [sys.executable, *flags, runner.__file__]
        for name in ('rom', 'bios', 'state', 'actions', 'output'):
            args.extend(['--' + name, 'unused'])
        args.extend(['--castle-status', '--expectations', 'unused'])
        result = subprocess.run(args, capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('not allowed with argument', result.stderr)


class CastleStateComparisonTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        _, cls.evidence, _ = castle_evidence()
        cls.folders = write_pair(Path(cls.tmp.name), cls.evidence)
        for wide, folder in enumerate(cls.folders):
            settings = json.loads((folder / 'run-settings.json').read_text())
            settings['observation_profile'] = CASTLE_STATUS_PROFILE
            dump(folder / 'run-settings.json', settings)
            for shot in cls.evidence['captures']:
                row = cls.evidence['trace'][shot['elapsed_frame']]
                (folder / shot['state']).write_bytes(castle_state_bytes(row, bool(wide)))
            (folder / 'final.state').write_bytes(castle_state_bytes(cls.evidence['trace'][-1], bool(wide)))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_equivalent_health_snapshots_and_profile(self):
        result = compare(*self.folders)
        self.assertTrue(result['differential_pass'])
        self.assertEqual(result['compared_state_snapshots'], 3)

    def test_matching_but_wrong_capture_health_is_rejected(self):
        shot = self.evidence['captures'][0]
        paths = [folder / shot['state'] for folder in self.folders]
        expected = self.evidence['trace'][shot['elapsed_frame']]
        for field, value in (('lives', -1), ('deaths', 1), ('baby_status', 0), ('exit_index', 2)):
            with self.subTest(field=field), restore(*paths):
                wrong = dict(expected, castle_status=dict(expected['castle_status'], **{field: value}))
                for wide, path in enumerate(paths):
                    path.write_bytes(castle_state_bytes(wrong, bool(wide)))
                with self.assertRaisesRegex(ValueError, 'castle_status'):
                    compare(*self.folders)

    def test_invalid_pointer_in_matching_saved_states_is_rejected(self):
        paths = [folder / 'final.state' for folder in self.folders]
        with restore(*paths):
            for path in paths:
                raw = bytearray(path.read_bytes())
                base = sections(bytes(raw))[b'BUS0'][0] + 0x40000
                struct.pack_into('<I', raw, base + 0x7240, 0)
                path.write_bytes(raw)
            with self.assertRaisesRegex(ValueError, 'pointer'):
                compare_state(*paths, self.evidence['trace'][-1])

    def test_missing_profile_and_car_contract_cannot_accept_castle_history(self):
        paths = [folder / 'run-settings.json' for folder in self.folders]
        with restore(*paths):
            for path in paths:
                settings = json.loads(path.read_text()); settings.pop('observation_profile')
                dump(path, settings)
            with self.assertRaisesRegex(ValueError, 'profile'):
                compare(*self.folders)
        with self.assertRaisesRegex(ValueError, 'car cycle'):
            compare(*self.folders, EXPECT)


if __name__ == '__main__':
    unittest.main()
