#!/usr/bin/env python3
"""Aggregate qualification regressions; these fixtures are not native evidence."""
import copy
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from compare_castle_progress import compare_castle

ROOT = Path(__file__).resolve().parent


class CastleAggregateTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.contract = json.loads((ROOT / 'castle-route-progress-expectations.json').read_text())
        self.folders = [Path(self.temp.name) / name for name in ('control', 'candidate')]
        self.settings = dict(observation_profile='castle-status-v1', capture_start=0,
            asset_sha256={'state': self.contract['entry_state_sha256']})
        for folder in self.folders:
            folder.mkdir()
            for name in ('actions', 'session', 'trace', 'captures'):
                (folder / (name + '.json')).write_text('[]\n')
        self.write_settings(self.settings)
        self.base = dict(differential_pass=True, coverage_pass=True, raw_gap_frames=0)

    def write_settings(self, settings):
        for folder in self.folders:
            (folder / 'run-settings.json').write_text(json.dumps(settings))

    def run_aggregate(self, *, route_pass=True, raw_gaps=0):
        base = dict(self.base, raw_gap_frames=raw_gaps)
        with patch('compare_castle_progress.compare', return_value=base), \
             patch('compare_castle_progress.check_castle_progress', return_value={
                 'progress_pass': route_pass, 'progress_errors': [] if route_pass else ['No progress']}):
            return compare_castle(*self.folders, self.contract)

    def test_identical_but_wrong_starting_assets_do_not_qualify(self):
        settings = copy.deepcopy(self.settings)
        settings['asset_sha256']['state'] = '0' * 64
        self.write_settings(settings)
        with self.assertRaisesRegex(ValueError, 'entry fixture'):
            self.run_aggregate()

    def test_missing_health_profile_cannot_be_replaced_by_trajectory_equality(self):
        settings = dict(self.settings)
        del settings['observation_profile']
        self.write_settings(settings)
        with self.assertRaisesRegex(ValueError, 'health observation'):
            self.run_aggregate()

    def test_skipping_entry_captures_does_not_qualify(self):
        self.write_settings(dict(self.settings, capture_start=300))
        with self.assertRaisesRegex(ValueError, 'entry transitions'):
            self.run_aggregate()

    def test_invisible_unresolved_objects_still_fail_the_strict_route_result(self):
        result = self.run_aggregate(raw_gaps=1)
        self.assertTrue(result['coverage_pass'])
        self.assertTrue(result['route_checks']['progress_pass'])
        self.assertFalse(result['complete_castle_progress_pass'])

    def test_equal_centers_and_zero_gaps_do_not_substitute_for_progress(self):
        result = self.run_aggregate(route_pass=False)
        self.assertTrue(result['differential_pass'])
        self.assertFalse(result['complete_castle_progress_pass'])

    def test_successful_bounded_route_does_not_claim_a_room_transition(self):
        result = self.run_aggregate()
        self.assertTrue(result['complete_castle_progress_pass'])
        self.assertFalse(result['natural_room_transition_qualified'])
        self.assertEqual(result['route_contract'], self.contract)
        self.assertEqual(set(result['evidence_sha256']), {'actions.json', 'trace.json', 'captures.json'})


if __name__ == '__main__':
    unittest.main()
