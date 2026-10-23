"""Boundary tests for the shared session budget; no native game execution."""
import copy
import json
import unittest
from pathlib import Path
from unittest.mock import Mock

from castle_route_progress import validate_castle_contract
from castle_transition_checks import validate_transition_contract
from panorama_session_checks import (
    MAX_SESSION_FRAMES, validate_actions, validate_expectations, validate_trace,
)
from run_panorama_session import execute_session
from test_panorama_transitions import SceneRunner


HERE = Path(__file__).resolve().parent
LIMIT_ACTIONS = [dict(n=600, keys=1023) for _ in range(12)]


def capture(name, row, elapsed, with_state):
    return dict(elapsed_frame=elapsed, frame=row['frame'], image=name + '.png',
                state=name + '.state')


class SessionLimitTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.evidence = dict(session=[], trace=[], captures=[])
        cls.runner = SceneRunner({})
        execute_session(cls.runner, LIMIT_ACTIONS, capture, cls.evidence, True)

    def test_exact_7200_frame_plan_is_valid(self):
        self.assertEqual(MAX_SESSION_FRAMES, 7200)
        validate_actions(LIMIT_ACTIONS)
        conditional = [dict(n=6000, keys=1023, from_transformation=2,
                            until_transformation=0), *LIMIT_ACTIONS[:2]]
        validate_actions(conditional)

    def test_7201_is_rejected_before_any_executor_command(self):
        actions = [*LIMIT_ACTIONS, dict(n=1, keys=1023)]
        with self.assertRaisesRegex(ValueError, '7200'):
            validate_actions(actions)
        command, shot = Mock(), Mock()
        evidence = dict(session=[], trace=[], captures=[])
        with self.assertRaisesRegex(ValueError, '7200'):
            execute_session(command, actions, shot, evidence, True)
        command.assert_not_called()
        shot.assert_not_called()
        self.assertEqual(evidence, dict(session=[], trace=[], captures=[]))

    def test_per_action_limits_are_unchanged(self):
        validate_actions([dict(n=600, keys=1023)])
        validate_actions([dict(n=6000, keys=1023, from_transformation=2,
                               until_transformation=0)])
        for action in (dict(n=601, keys=1023),
                       dict(n=6001, keys=1023, from_transformation=2, until_transformation=0)):
            with self.subTest(action=action), self.assertRaisesRegex(ValueError, 'action frame budget'):
                validate_actions([action])

    def test_two_hundred_action_limit_is_unchanged(self):
        validate_actions([dict(n=1, keys=1023) for _ in range(200)])
        with self.assertRaisesRegex(ValueError, '1..200'):
            validate_actions([dict(n=1, keys=1023) for _ in range(201)])

    def test_executor_and_trace_accept_exact_budget_but_reject_missing_observations(self):
        self.assertEqual(self.runner.elapsed, 7200)
        self.assertEqual(len(self.evidence['trace']), 7201)
        self.assertEqual(self.evidence['session'][-1]['elapsed_frame'], 7200)
        self.assertEqual(validate_trace(LIMIT_ACTIONS, self.evidence['session'],
                                        self.evidence['trace']), 7200)
        with self.assertRaisesRegex(ValueError, 'per-frame observations'):
            validate_trace(LIMIT_ACTIONS, self.evidence['session'], self.evidence['trace'][:-1])
        session = copy.deepcopy(self.evidence['session'])
        session[-1]['action']['n'] += 1
        with self.assertRaises(ValueError):
            validate_trace(LIMIT_ACTIONS, session, self.evidence['trace'])

    def test_older_contract_semantic_bounds_are_not_extended(self):
        for filename, validate in (
            ('car-cycle-expectations.json', validate_expectations),
            ('castle-route-progress-expectations.json', validate_castle_contract),
            ('castle-transition-expectations.json', validate_transition_contract),
        ):
            value = json.loads((HERE / filename).read_text())
            value['final_idle_frames'] = 6000
            validate(value)
            value['final_idle_frames'] = 6001
            with self.subTest(contract=filename), self.assertRaises(ValueError):
                validate(value)


if __name__ == '__main__':
    unittest.main()
