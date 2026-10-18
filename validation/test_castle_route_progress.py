#!/usr/bin/env python3
"""Synthetic validator regressions; these tests do not execute the game."""
import copy
import json
import unittest
from pathlib import Path

from castle_route_progress import check_castle_progress, validate_castle_contract
from panorama_session_checks import action_digest


HERE = Path(__file__).resolve().parent
CONTRACT = json.loads((HERE / 'castle-route-progress-expectations.json').read_text())
ACTIONS = [{'n': 300, 'keys': 1023}, {'n': 600, 'keys': 1007},
           {'n': 60, 'keys': 1023}, {'n': 300, 'keys': 1023}]


def checkpoints(actions, trace):
    result, cursor = [], 0
    for action in actions:
        cursor += action['n']
        row = copy.deepcopy(trace[cursor])
        row.pop('keys')
        row.pop('action_index')
        row['action'] = dict(action)
        result.append(row)
    return result


def example():
    """A synthetic route crosses each region and then settles for 300 frames."""
    trace, cursor = [], 0
    for index, action in [(-1, {'n': 1, 'keys': None}), *enumerate(ACTIONS)]:
        for _ in range(action['n']):
            elapsed = cursor
            if elapsed <= 300:
                x, vx = 256, 0
            elif elapsed <= 900:
                x, vx = 256 + (elapsed - 300) * 4, 4
            elif elapsed <= 960:
                x, vx = 2656 + (elapsed - 900) / 256, 1 / 256
            else:
                x, vx = 2656 + 60 / 256, 0
            y = 1800 + max(0, min(50, elapsed - 850)) * 3
            trace.append(dict(elapsed_frame=elapsed, frame=7000 + elapsed,
                action_index=index, keys=action['keys'], level_id=3, sublevel_id=3,
                game_state=10 if elapsed < 10 else 12 if elapsed < 60 else 13,
                transformation=0,
                player_xy_velocity=[x, y, vx, 3 if 850 < elapsed <= 900 else 0],
                castle_status=dict(game_pointer=0x0300220c, lives=5, deaths=0,
                    inhibition=0, stars=100, entry_type=0, exit_index=0,
                    baby_id=97, baby_status=5)))
            cursor += 1
    return trace


def check(trace, actions=ACTIONS, contract=None):
    reviewed = copy.deepcopy(CONTRACT) if contract is None else copy.deepcopy(contract)
    reviewed['actions_sha256'] = action_digest(actions)
    return check_castle_progress(actions, checkpoints(actions, trace), trace, reviewed)


class CastleContractTests(unittest.TestCase):
    def test_reviewed_actions_are_bound_to_contract(self):
        actions = json.loads((HERE / 'panorama-castle-progress-actions.json').read_text())
        validate_castle_contract(CONTRACT)
        self.assertEqual(action_digest(actions), CONTRACT['actions_sha256'])
        self.assertEqual((len(actions), sum(a['n'] for a in actions)), (37, 2238))

    def test_closed_contract_cannot_weaken_or_change_the_fixture(self):
        changes = [('profile', 'castle-route-progress-v2'), ('actions_sha256', 'bad'),
                   ('entry_state_sha256', '0' * 64), ('gameplay_start_frame', 301),
                   ('level_id', 4), ('sublevel_id', 4), ('final_idle_frames', 299),
                   ('final_min_x_pixels', 2599), ('final_idle_frames', True),
                   ('final_min_x_pixels', 2600.0), ('accept_deaths', True)]
        for name, value in changes:
            with self.subTest(name=name, value=value), self.assertRaises(ValueError):
                validate_castle_contract(dict(CONTRACT, **{name: value}))
        for name in CONTRACT:
            changed = copy.deepcopy(CONTRACT)
            changed.pop(name)
            with self.subTest(missing=name), self.assertRaises(ValueError):
                validate_castle_contract(changed)

    def test_waypoint_geometry_order_schema_and_minima_are_fixed(self):
        for index in (0, 1):
            for name, value in [('id', 'other'), ('x_min_pixels', 1800),
                                ('x_max_pixels', 9999), ('y_max_pixels', 1826),
                                ('min_consecutive_frames', 4), ('extra', 0)]:
                changed = copy.deepcopy(CONTRACT)
                changed['waypoints'][index][name] = value
                with self.subTest(index=index, name=name), self.assertRaises(ValueError):
                    validate_castle_contract(changed)
        changed = copy.deepcopy(CONTRACT)
        changed['waypoints'].reverse()
        with self.assertRaises(ValueError):
            validate_castle_contract(changed)
        strengthened = copy.deepcopy(CONTRACT)
        strengthened['final_idle_frames'] = 301
        strengthened['final_min_x_pixels'] = 2601
        strengthened['waypoints'][0]['min_consecutive_frames'] = 6
        validate_castle_contract(strengthened)

    def test_rebound_actions_are_rejected(self):
        trace = example()
        with self.assertRaisesRegex(ValueError, 'Actions differ'):
            check_castle_progress(ACTIONS, checkpoints(ACTIONS, trace), trace, CONTRACT)


class CastleProgressTests(unittest.TestCase):
    def test_happy_path_reports_progress_not_a_natural_transition(self):
        result = check(example())
        self.assertTrue(result['progress_pass'], result['progress_errors'])
        self.assertEqual(result['game_state_sequence'], [10, 12, 13])
        self.assertEqual(result['transformation_sequence'], [0])
        self.assertEqual(result['stable_normal_idle_frames'], 300)
        self.assertTrue(all(p['passed'] for p in result['waypoints']))
        self.assertLess(result['waypoints'][0]['first_completed_elapsed_frame'],
                        result['waypoints'][1]['first_start_elapsed_frame'])
        self.assertFalse(result['natural_transition_validated'])
        self.assertEqual(result['total_lives_lost'], 0)

    def test_stationary_routes_never_pass_despite_good_idle(self):
        for x in (256, 1872.99609375, 2600):
            trace = example()
            for row in trace:
                row['player_xy_velocity'] = [x, 1800, 0, 0]
            with self.subTest(x=x):
                result = check(trace)
                self.assertFalse(result['progress_pass'])
                self.assertFalse(any(p['passed'] for p in result['waypoints']))

    def test_unrounded_old_wall_and_final_threshold_do_not_gain_a_pixel(self):
        trace = example()
        for row in trace:
            row['player_xy_velocity'][0] = min(row['player_xy_velocity'][0], 1872.99609375)
        result = check(trace)
        self.assertEqual(result['observed_limits']['x_max_pixels'], 1872.99609375)
        self.assertFalse(result['waypoints'][0]['passed'])
        for x, expected in ((2599.99609375, False), (2600, True)):
            trace = example()
            for row in trace[961:]:
                row['player_xy_velocity'][0] = x
            with self.subTest(x=x):
                self.assertEqual(check(trace)['progress_pass'], expected)

    def test_waypoint_x_bounds_use_unrounded_coordinates(self):
        for x, expected in ((1930 - 1 / 256, False), (1930, True),
                            (2096, True), (2096 + 1 / 256, False)):
            trace = example()
            for row in trace:
                if 1930 <= row['player_xy_velocity'][0] <= 2096:
                    row['player_xy_velocity'][0] = x
            with self.subTest(x=x):
                self.assertEqual(check(trace)['progress_pass'], expected)

    def test_waypoints_require_five_consecutive_frames_in_the_right_place(self):
        for failure in ('four_frames', 'alternating', 'height', 'sublevel', 'order'):
            trace = example()
            if failure == 'order':
                for row in trace[719:760]:
                    row['player_xy_velocity'][0] += 300
                for row in trace[792:827]:
                    row['player_xy_velocity'][0] -= 300
            else:
                matches = [r for r in trace if 1930 <= r['player_xy_velocity'][0] <= 2096]
                for i, row in enumerate(matches):
                    if failure == 'four_frames' and i >= 4:
                        row['player_xy_velocity'][1] = 1825 + 1 / 256
                    if failure == 'alternating' and i % 2:
                        row['player_xy_velocity'][1] = 1825 + 1 / 256
                    if failure == 'height': row['player_xy_velocity'][1] = 1825 + 1 / 256
                    if failure == 'sublevel': row['sublevel_id'] = 4
            with self.subTest(failure=failure):
                self.assertFalse(check(trace)['progress_pass'])

    def test_earned_life_does_not_mask_a_later_decrement(self):
        trace = example()
        for row in trace[400:500]:
            row['castle_status']['lives'] = 6
        result = check(trace)
        self.assertFalse(result['progress_pass'])
        self.assertEqual(result['initial_lives'], result['final_lives'])
        self.assertEqual(result['life_decrement_events'],
                         [dict(elapsed_frame=500, previous=6, current=5, decrease=1)])

    def test_signed_zero_to_minus_one_and_every_individual_drop_are_counted(self):
        trace = example()
        for row in trace:
            row['castle_status']['lives'] = 0 if row['elapsed_frame'] < 400 else -1
        result = check(trace)
        self.assertFalse(result['progress_pass'])
        self.assertEqual(result['life_decrement_events'][0]['current'], -1)
        self.assertEqual(result['total_lives_lost'], 1)
        trace = example()
        for row in trace[400:]:
            row['castle_status']['lives'] -= 1 if row['elapsed_frame'] < 500 else 2
        result = check(trace)
        self.assertEqual([e['elapsed_frame'] for e in result['life_decrement_events']], [400, 500])

    def test_constant_negative_lives_fail_without_a_decrement_but_zero_is_valid(self):
        for lives, expected in ((-1, False), (0, True)):
            trace = example()
            for row in trace:
                row['castle_status']['lives'] = lives
            result = check(trace)
            with self.subTest(lives=lives):
                self.assertEqual(result['progress_pass'], expected)
                self.assertEqual(result['life_decrement_events'], [])
                self.assertEqual(bool(result['negative_life_frames']), not expected)

    def test_death_counter_increment_reset_and_initial_nonzero_fail(self):
        for pattern in ('pulse', 'initial', 'constant'):
            trace = example()
            if pattern == 'pulse': trace[401]['castle_status']['deaths'] = 1
            if pattern == 'initial': trace[0]['castle_status']['deaths'] = 1
            if pattern == 'constant':
                for row in trace: row['castle_status']['deaths'] = 1
            with self.subTest(pattern=pattern):
                result = check(trace)
                self.assertFalse(result['progress_pass'])
                if pattern == 'pulse':
                    self.assertEqual([e['delta'] for e in result['death_counter_changes']], [1, -1])

    def test_death_restart_transform_and_inhibition_fail_even_between_endpoints(self):
        for elapsed in (5, 301, 700):
            for state in (15, 16, 17, 18, 19, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45):
                trace = example()
                trace[elapsed]['game_state'] = state
                with self.subTest(elapsed=elapsed, state=state):
                    self.assertFalse(check(trace)['progress_pass'])
        for elapsed in (5, 300, 700):
            for field, value in (('transformation', 2), ('inhibition', 1)):
                trace = example()
                target = trace[elapsed]['castle_status'] if field == 'inhibition' else trace[elapsed]
                target[field] = value
                with self.subTest(elapsed=elapsed, field=field):
                    self.assertFalse(check(trace)['progress_pass'])

    def test_entry_allowlist_ends_at_frame_300(self):
        for state in (9, 10, 11, 12):
            for elapsed, expected in ((299, True), (300, False)):
                trace = example()
                trace[elapsed]['game_state'] = state
                with self.subTest(state=state, elapsed=elapsed):
                    self.assertEqual(check(trace)['progress_pass'], expected)

    def test_recovered_baby_loss_is_recorded_without_rejecting_route(self):
        trace = example()
        for row in trace[401:407]: row['castle_status']['baby_status'] = 4
        result = check(trace)
        self.assertTrue(result['progress_pass'], result['progress_errors'])
        self.assertEqual(result['baby_loss_episodes'], [dict(start_elapsed_frame=401,
            end_elapsed_frame=406, observed_frames=6, recovery_elapsed_frame=407)])

    def test_final_baby_unsafe_or_loss_during_idle_fails(self):
        for elapsed, field, value in ((1260, 'baby_id', 96), (1260, 'baby_status', 4),
                                      (1000, 'baby_status', 4)):
            trace = example()
            trace[elapsed]['castle_status'][field] = value
            with self.subTest(elapsed=elapsed, field=field):
                self.assertFalse(check(trace)['progress_pass'])

    def test_idle_requires_exact_position_velocity_and_no_buttons(self):
        for failure in ('x_jitter', 'y_jitter', 'vx', 'vy', 'button', 'short'):
            trace = example()
            actions = copy.deepcopy(ACTIONS)
            if failure in ('x_jitter', 'y_jitter', 'vx', 'vy'):
                index = {'x_jitter': 0, 'y_jitter': 1, 'vx': 2, 'vy': 3}[failure]
                trace[1000]['player_xy_velocity'][index] += 1 / 256
            if failure == 'button':
                actions[-1]['keys'] = 1007
                for row in trace[961:]: row['keys'] = 1007
            if failure == 'short':
                actions[-1]['n'] -= 1
                trace.pop()
            with self.subTest(failure=failure):
                self.assertFalse(check(trace, actions)['progress_pass'])

    def test_frame_gaps_resets_inputs_and_status_checkpoint_mismatches_raise(self):
        for failure in ('gap', 'reset', 'keys', 'status_checkpoint', 'missing_status'):
            trace = example()
            session = checkpoints(ACTIONS, trace)
            reviewed = dict(CONTRACT, actions_sha256=action_digest(ACTIONS))
            if failure == 'gap': trace.pop(401)
            if failure == 'reset': trace[401]['frame'] = 0
            if failure == 'keys': trace[401]['keys'] = 1023
            if failure == 'status_checkpoint': session[-1]['castle_status']['lives'] += 1
            if failure == 'missing_status':
                for row in [*trace, *session]: row.pop('castle_status')
            with self.subTest(failure=failure), self.assertRaises(ValueError):
                check_castle_progress(ACTIONS, session, trace, reviewed)


if __name__ == '__main__':
    unittest.main()
