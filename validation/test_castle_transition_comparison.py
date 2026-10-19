#!/usr/bin/env python3
"""Synthetic aggregate/CLI regressions; none of these fixtures is native evidence."""
import copy
import hashlib
import io
import json
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from unittest.mock import patch

import compare_castle_transition as wrapper
from test_car_cycle import dump, restore
from test_castle_status import castle_evidence, castle_state_bytes
from test_panorama_transitions import write_pair


ROOT = Path(__file__).resolve().parent
CONTRACT = ROOT / 'castle-transition-expectations.json'


class CastleTransitionComparisonTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='castle transition private ')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.contract = json.loads(CONTRACT.read_text())
        _, self.evidence, _ = castle_evidence()
        self.folders = write_pair(self.root, self.evidence)
        for wide, folder in enumerate(self.folders):
            settings = json.loads((folder / 'run-settings.json').read_text())
            settings['observation_profile'] = 'castle-status-v1'
            settings['asset_sha256']['state'] = self.contract['entry_state_sha256']
            dump(folder / 'run-settings.json', settings)
            for shot in self.evidence['captures']:
                row = self.evidence['trace'][shot['elapsed_frame']]
                (folder / shot['state']).write_bytes(castle_state_bytes(row, bool(wide)))
            (folder / 'final.state').write_bytes(
                castle_state_bytes(self.evidence['trace'][-1], bool(wide)))

    def qualify(self, transition=None):
        # The 8-frame stub above only supplies differential-comparison evidence.
        # Route acceptance is controlled here to test aggregate logic separately.
        value = transition or dict(transition_pass=True, transition_errors=[])
        with patch.object(wrapper, 'check_castle_transition', return_value=value):
            return wrapper.compare_castle_transition(*self.folders, self.contract)

    def update_settings(self, folder, **changes):
        path = folder / 'run-settings.json'
        settings = json.loads(path.read_text())
        settings.update(changes)
        dump(path, settings)

    def assert_rejected(self, pattern):
        with patch.object(wrapper, 'check_castle_transition') as route:
            with self.assertRaisesRegex(ValueError, pattern):
                wrapper.compare_castle_transition(*self.folders, self.contract)
            route.assert_not_called()

    def test_equal_healthy_histories_without_a_transition_cannot_qualify(self):
        result = self.qualify(dict(transition_pass=False, transition_errors=['No pipe transition']))
        self.assertTrue(result['differential_pass'])
        self.assertTrue(result['coverage_pass'])
        self.assertFalse(result['complete_castle_transition_pass'])
        self.assertFalse(result['natural_room_transition_qualified'])

    def test_complete_approval_requires_all_gates_and_emits_only_aggregates(self):
        result = self.qualify()
        self.assertTrue(result['complete_castle_transition_pass'])
        self.assertTrue(result['natural_room_transition_qualified'])
        self.assertEqual(result['transition_contract'], self.contract)
        self.assertEqual(result['input_provenance']['observation_profile'], 'castle-status-v1')
        self.assertEqual(set(result['evidence_sha256']), {'actions.json', 'trace.json', 'captures.json'})
        for name, digest in result['evidence_sha256'].items():
            self.assertEqual(digest, hashlib.sha256((self.folders[0] / name).read_bytes()).hexdigest())
        self.assertIn('pipe-transition attempt', result['scope'])
        self.assertIn('Only a passing complete_castle_transition_pass qualifies', result['scope'])
        self.assertIn('Does not qualify the complete castle', result['scope'])
        self.assertIn('Android, an APK or frame rate', result['scope'])
        encoded = json.dumps(result, allow_nan=False)
        self.assertNotIn(self.temp.name, encoded)
        self.assertTrue({'actions', 'trace', 'session', 'captures'}.isdisjoint(result))

    def test_raw_invisible_gap_rejects_even_when_coverage_and_transition_pass(self):
        log = self.folders[1] / 'run.log'
        log.write_text(log.read_text() +
                       '[sma3:object-gap] frame=1004 coherent=1 unresolved=1 visible_unresolved=0\n')
        result = self.qualify()
        self.assertTrue(result['coverage_pass'])
        self.assertTrue(result['transition_checks']['transition_pass'])
        self.assertEqual(result['raw_gap_frames'], 1)
        self.assertFalse(result['complete_castle_transition_pass'])
        self.assertFalse(result['natural_room_transition_qualified'])

    def test_failed_coverage_rejects_an_observed_transition(self):
        log = self.folders[1] / 'run.log'
        log.write_text(log.read_text() +
                       '[sma3:object-gap] frame=1004 coherent=0 unresolved=1 visible_unresolved=1\n')
        result = self.qualify()
        self.assertFalse(result['coverage_pass'])
        self.assertFalse(result['complete_castle_transition_pass'])
        self.assertFalse(result['natural_room_transition_qualified'])

    def test_false_comparison_results_and_contradictory_route_flags_cannot_pass(self):
        for field in ('differential_pass', 'coverage_pass'):
            result = dict(differential_pass=True, coverage_pass=True, raw_gap_frames=0)
            result[field] = False
            with self.subTest(field=field), patch.object(wrapper, 'compare', return_value=result):
                result = self.qualify()
            self.assertFalse(result['complete_castle_transition_pass'])
            self.assertFalse(result['natural_room_transition_qualified'])
        result = self.qualify(dict(transition_pass=True, transition_errors=['Unresolved route failure']))
        self.assertFalse(result['complete_castle_transition_pass'])
        self.assertFalse(result['natural_room_transition_qualified'])

    def test_same_width_pair_does_not_qualify_as_widescreen(self):
        self.update_settings(self.folders[1], width=240)
        self.assert_rejected('widths')

    def test_missing_frame_on_both_sides_cannot_be_hidden_by_equality(self):
        trace = copy.deepcopy(self.evidence['trace'])
        trace.pop(2)
        for folder in self.folders:
            dump(folder / 'trace.json', trace)
        self.assert_rejected('per-frame observations')

    def test_missing_both_traces_does_not_fall_back_to_legacy_qualification(self):
        for folder in self.folders:
            for index, shot in enumerate(self.evidence['captures']):
                (folder / f'step-{index:03}.png').write_bytes((folder / shot['image']).read_bytes())
            (folder / 'trace.json').unlink()
        self.assertTrue(wrapper.compare(*self.folders)['differential_pass'])
        with patch.object(wrapper, 'check_castle_transition') as route:
            with self.assertRaises(FileNotFoundError):
                wrapper.compare_castle_transition(*self.folders, self.contract)
            route.assert_not_called()

    def test_same_omitted_capture_on_both_sides_is_rejected(self):
        for folder in self.folders:
            dump(folder / 'captures.json', self.evidence['captures'][1:])
        self.assert_rejected('capture')

    def test_equal_but_wrong_serialized_health_is_rejected(self):
        shot = self.evidence['captures'][0]
        wrong = copy.deepcopy(self.evidence['trace'][shot['elapsed_frame']])
        wrong['castle_status']['deaths'] = 1
        for wide, folder in enumerate(self.folders):
            (folder / shot['state']).write_bytes(castle_state_bytes(wrong, bool(wide)))
        self.assert_rejected('Serialized castle_status')

    def test_incomplete_native_run_or_nonzero_completion_counters_reject(self):
        for folder in self.folders:
            path = folder / 'run-status.json'
            with self.subTest(folder=folder.name), restore(path):
                dump(path, {'execution_complete': False})
                self.assert_rejected('execution did not complete')
        log = self.folders[0] / 'run.log'
        log.write_text(log.read_text() +
                       'dispatch_misses=1 interpreted_insns=0 healed_native=0\n')
        self.assert_rejected('static completion counters')

    def test_different_native_build_provenance_is_rejected(self):
        path = self.folders[1] / 'run-settings.json'
        settings = json.loads(path.read_text())
        settings['asset_sha256']['runner'] = '1' * 64
        dump(path, settings)
        self.assert_rejected('Different starting assets, native build')

    def test_fixture_profile_and_initial_capture_are_bound_on_each_side(self):
        for folder in self.folders:
            path = folder / 'run-settings.json'
            for change, pattern in (({'observation_profile': None}, 'health observation'),
                                    ({'capture_start': 300}, 'entry transitions'),
                                    ({'capture_start': False}, 'entry transitions')):
                with self.subTest(side=folder.name, change=change), restore(path):
                    self.update_settings(folder, **change)
                    self.assert_rejected(pattern)
            with self.subTest(side=folder.name, fixture=True), restore(path):
                settings = json.loads(path.read_text())
                settings['asset_sha256']['state'] = '0' * 64
                dump(path, settings)
                self.assert_rejected('entry fixture')

    def test_missing_or_nonhash_asset_provenance_is_rejected_before_publication(self):
        for folder in self.folders:
            path = folder / 'run-settings.json'
            for malformed in ('missing', 'private path'):
                with self.subTest(side=folder.name, malformed=malformed), restore(path):
                    settings = json.loads(path.read_text())
                    if malformed == 'missing':
                        del settings['asset_sha256']['bios']
                    else:
                        settings['asset_sha256']['rom'] = str(self.root / 'private-rom.gba')
                    dump(path, settings)
                    self.assert_rejected('five input provenance SHA-256')

    def test_real_route_checker_rejects_equal_evidence_for_the_wrong_action_plan(self):
        # No checker stub: paired equality cannot replace the contract's plan.
        with self.assertRaises(ValueError):
            wrapper.compare_castle_transition(*self.folders, self.contract)

    def test_invalid_contract_is_rejected_before_any_paired_comparison(self):
        invalid = dict(self.contract, allow_deaths=True)
        with patch.object(wrapper, 'compare') as paired:
            with self.assertRaises(ValueError):
                wrapper.compare_castle_transition(*self.folders, invalid)
            paired.assert_not_called()

    def invoke_cli(self, output, contract=CONTRACT):
        argv = ['compare_castle_transition.py', *map(str, self.folders),
                str(output), '--contract', str(contract)]
        stdout, stderr = io.StringIO(), io.StringIO()
        code = 0
        with patch.object(sys, 'argv', argv), redirect_stdout(stdout), redirect_stderr(stderr):
            try:
                wrapper.main()
            except SystemExit as error:
                code = error.code
        return code, stdout.getvalue(), stderr.getvalue()

    def test_cli_success_writes_parseable_matching_json_and_zero_exit(self):
        output = self.root / 'report.json'
        with patch.object(wrapper, 'check_castle_transition', return_value=dict(
                transition_pass=True, transition_errors=[])):
            code, stdout, _ = self.invoke_cli(output)
        self.assertEqual(code, 0)
        report = json.loads(output.read_text())
        self.assertEqual(report, json.loads(stdout))
        self.assertTrue(report['complete_castle_transition_pass'])
        self.assertTrue(report['natural_room_transition_qualified'])

    def test_cli_invalid_json_writes_a_failed_report_without_private_paths_or_payload(self):
        (self.folders[0] / 'run-settings.json').write_text('{"private": "secret payload"')
        output = self.root / 'report.json'
        code, stdout, _ = self.invoke_cli(output)
        self.assertEqual(code, 1)
        report = json.loads(output.read_text())
        self.assertEqual(report, json.loads(stdout))
        self.assertFalse(report['complete_castle_transition_pass'])
        self.assertFalse(report['natural_room_transition_qualified'])
        self.assertEqual(report['error_type'], 'JSONDecodeError')
        self.assertNotIn('secret payload', stdout)
        self.assertNotIn(self.temp.name, stdout)

    def test_cli_missing_evidence_and_invalid_contract_fail_with_json(self):
        path = self.folders[0] / 'run-settings.json'
        path.unlink()
        output = self.root / 'missing.json'
        code, stdout, _ = self.invoke_cli(output)
        self.assertEqual(code, 1)
        self.assertEqual(json.loads(stdout), json.loads(output.read_text()))
        self.assertFalse(json.loads(stdout)['natural_room_transition_qualified'])
        self.assertNotIn(self.temp.name, stdout)
        invalid = self.root / 'invalid-contract.json'
        dump(invalid, dict(self.contract, allow_deaths=True))
        code, stdout, _ = self.invoke_cli(self.root / 'invalid.json', invalid)
        self.assertEqual(code, 1)
        self.assertFalse(json.loads(stdout)['complete_castle_transition_pass'])
        self.assertEqual(json.loads(stdout)['error_type'], 'ValueError')

    def test_cli_never_overwrites_an_existing_report(self):
        output = self.root / 'historical.json'
        original = b'{"historical_evidence": true}\n'
        output.write_bytes(original)
        with patch.object(wrapper, 'compare_castle_transition') as compare:
            code, stdout, stderr = self.invoke_cli(output)
            compare.assert_not_called()
        self.assertEqual(code, 2)
        self.assertEqual(stdout, '')
        self.assertIn('must not be overwritten', stderr)
        self.assertEqual(output.read_bytes(), original)


if __name__ == '__main__':
    unittest.main()
