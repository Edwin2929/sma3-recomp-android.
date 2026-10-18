#!/usr/bin/env python3
"""Synthetic scene-transition regressions; not evidence of native gameplay."""
import os
import struct
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from PIL import Image
import run_panorama_session as runner
from compare_panorama_sessions import compare
from panorama_session_checks import required_capture_frames, validate_trace
from test_car_cycle import EXPECT, PLAN, dump, restore, simulate, state_bytes


ACTIONS = [{'n': 4, 'keys': 1007}, {'n': 4, 'keys': 1023}]


class SceneRunner:
    """A frame-indexed IWRAM stub with brief changes between checkpoints."""
    def __init__(self, changes):
        self.elapsed = 0
        self.changes = changes

    def __call__(self, **args):
        if args['cmd'] == 'run_frames':
            self.elapsed += args['n']
            return dict(ok=True, frame=1000 + self.elapsed)
        if args['cmd'] != 'read_iwram':
            raise AssertionError('Unexpected native command')
        if (args['addr'], args['len']) != (0x03000000 + runner.RAM_START, runner.RAM_END - runner.RAM_START):
            raise AssertionError('Unexpected observation range')
        row = dict(level_id=3, sublevel_id=3, game_state=13, transformation=0)
        row.update(self.changes.get(self.elapsed, {}))
        raw = bytearray(runner.RAM_END - runner.RAM_START)
        for field, offset, fmt in (('level_id', 0x6288, 'H'), ('sublevel_id', 0x4cb8, 'H'),
                                   ('game_state', 0x6b05, 'B'), ('transformation', 0x6db2, 'H')):
            struct.pack_into('<' + fmt, raw, offset - runner.RAM_START, row[field])
        struct.pack_into('<iiii', raw, 0x6d80 - runner.RAM_START, self.elapsed * 256, 100 * 256, 0, 0)
        return dict(ok=True, data=raw.hex())


def scene_evidence(changes, start=0):
    evidence = dict(session=[], trace=[], captures=[])
    def capture(name, row, elapsed, with_state):
        return dict(elapsed_frame=elapsed, frame=row['frame'], image=name + '.png', state=name + '.state')
    runner.execute_session(SceneRunner(changes), ACTIONS, capture, evidence, True, start)
    validate_trace(ACTIONS, evidence['session'], evidence['trace'])
    return evidence


def write_pair(root, evidence, actions=ACTIONS, capture_start=0):
    folders = [root / name for name in ('control', 'candidate')]
    for width, folder in zip((240, 356), folders):
        folder.mkdir()
        for name in ('session', 'trace', 'captures'):
            dump(folder / (name + '.json'), evidence[name])
        dump(folder / 'actions.json', actions)
        dump(folder / 'run-status.json', {'execution_complete': True})
        dump(folder / 'run-settings.json', dict(width=width, trace_every_frame=True,
             capture_start=capture_start, asset_sha256={k: '0' * 64 for k in ('rom', 'bios', 'state', 'runner', 'config')}))
        (folder / 'run.log').write_text('[sma3:wide] synthetic test\ndispatch_misses=0 interpreted_insns=0 healed_native=0\n')
        for shot in evidence['captures']:
            Image.new('RGB', (width, 160), (1, 2, 3)).save(folder / shot['image'])
            row = evidence['trace'][shot['elapsed_frame']]
            (folder / shot['state']).write_bytes(state_bytes(row, width == 356))
        (folder / 'final.state').write_bytes(state_bytes(evidence['trace'][-1], width == 356))
    return folders


class TransitionCaptureTests(unittest.TestCase):
    def test_each_brief_transition_is_captured_between_action_endpoints(self):
        for field, value in (('game_state', 12), ('level_id', 4), ('sublevel_id', 7), ('transformation', 6)):
            with self.subTest(field=field):
                evidence = scene_evidence({2: {field: value}})
                self.assertTrue(all(row[field] != value for row in evidence['session']))
                frames = [shot['elapsed_frame'] for shot in evidence['captures']]
                self.assertEqual(frames, [2, 3, 4, 8])
                self.assertEqual(set(frames), required_capture_frames(evidence['session'], evidence['trace']))

    def test_capture_start_keeps_return_transition_and_coincident_endpoint_once(self):
        evidence = scene_evidence({2: {'game_state': 12}, 4: {'sublevel_id': 7}}, start=3)
        frames = [shot['elapsed_frame'] for shot in evidence['captures']]
        self.assertEqual(frames, [3, 4, 5, 8])
        self.assertEqual(set(frames), required_capture_frames(evidence['session'], evidence['trace'], 3))

    def test_position_changes_alone_do_not_create_scene_captures(self):
        evidence = scene_evidence({})
        self.assertNotEqual(evidence['trace'][1]['player_xy_velocity'], evidence['trace'][2]['player_xy_velocity'])
        self.assertEqual([shot['elapsed_frame'] for shot in evidence['captures']], [4, 8])


class TransitionComparisonTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        # Both action endpoints return to the origin in normal gameplay.
        # Neither the transient state nor the transient level appears there.
        cls.evidence = scene_evidence({2: {'game_state': 12},
            3: {'game_state': 12, 'level_id': 4, 'sublevel_id': 7}})
        cls.control, cls.candidate = write_pair(Path(cls.tmp.name), cls.evidence)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_aggregates_include_states_and_levels_between_endpoints(self):
        result = compare(self.control, self.candidate)
        self.assertTrue(result['differential_pass'])
        self.assertEqual(result['level_id'], 3)
        self.assertEqual(result['sampled_game_states'], [12, 13])
        self.assertEqual(result['level_ids'], [3, 4])
        self.assertEqual(result['sublevel_ids'], [3, 7])
        self.assertFalse(result['supported_gameplay_only'])
        self.assertEqual(result['compared_state_snapshots'], 5)

    def test_omitting_the_same_brief_transition_capture_on_both_sides_fails(self):
        paths = [folder / 'captures.json' for folder in (self.control, self.candidate)]
        for elapsed in (2, 3):
            with self.subTest(elapsed=elapsed), restore(*paths):
                for path in paths:
                    dump(path, [shot for shot in self.evidence['captures'] if shot['elapsed_frame'] != elapsed])
                with self.assertRaisesRegex(ValueError, 'capture'):
                    compare(self.control, self.candidate)

    def test_matching_but_wrong_transient_states_cannot_pass(self):
        paths = [folder / 'frame-00003.state' for folder in (self.control, self.candidate)]
        with restore(*paths):
            for wide, path in enumerate(paths):
                wrong = dict(self.evidence['trace'][3], sublevel_id=3)
                path.write_bytes(state_bytes(wrong, bool(wide)))
            with self.assertRaisesRegex(ValueError, 'Serialized game state'):
                compare(self.control, self.candidate)

    def test_car_cycle_support_remains_distinct_from_initial_entry(self):
        _, evidence = simulate()
        with tempfile.TemporaryDirectory() as temp:
            folders = write_pair(Path(temp), evidence, PLAN, 600)
            result = compare(*folders, EXPECT)
        self.assertTrue(result['complete_cycle_pass'])
        self.assertTrue(result['cycle_supported_gameplay_only'])
        self.assertFalse(result['supported_gameplay_only'])
        self.assertEqual(result['sampled_game_states'], [10, 13])


class SessionEnvironmentTests(unittest.TestCase):
    def test_inherited_output_paths_are_replaced_or_removed_without_mutating_parent(self):
        opt_ins = ('GBARECOMP_FP_SAVE', 'GBARECOMP_IRQ_LOG', 'GBARECOMP_SWI_LOG',
                   'GBARECOMP_IWRAM_DUMP', 'GBARECOMP_WRAM_TRACE',
                   'GBARECOMP_MISS_EWRAM_DUMP', 'GBARECOMP_MISS_IWRAM_DUMP',
                   'GBARECOMP_INPUT_REPLAY', 'GBARECOMP_INPUT_RECORD')
        inherited = {name: '/repository/unwanted-output' for name in opt_ins}
        inherited.update(GBARECOMP_COVERAGE_JSON='/repository/coverage.json',
                         GBARECOMP_MISS_FRAG='/repository/misses.toml.frag',
                         SMA3_WIDE_PROBE_DIR='/repository/probes',
                         PATH='/existing/search/path', GBARECOMP_STRICT_STATIC='0')
        with tempfile.TemporaryDirectory(prefix='panorama private ') as temp:
            output = Path(temp)
            with patch.dict(os.environ, inherited, clear=True):
                env = runner.session_environment(output)
                self.assertEqual(dict(os.environ), inherited)
            self.assertEqual(env['GBARECOMP_COVERAGE_JSON'], str(output / 'native-coverage.json'))
            self.assertEqual(env['GBARECOMP_MISS_FRAG'], str(output / 'native-misses.toml.frag'))
            self.assertEqual(env['SMA3_WIDE_PROBE_DIR'], str(output))
            self.assertTrue(all(name not in env for name in opt_ins))
            self.assertEqual(env['GBARECOMP_STRICT_STATIC'], '1')
            self.assertEqual(env['PATH'], inherited['PATH'])
            other = runner.session_environment(output / 'another-session')
            self.assertNotEqual(env['GBARECOMP_COVERAGE_JSON'], other['GBARECOMP_COVERAGE_JSON'])


if __name__ == '__main__':
    unittest.main()
