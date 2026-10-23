"""Synthetic cumulative-route regressions; this is not a reconstructed input tape.

The published 65-action prefix is fixed. The small synthetic suffix and its
fabricated observations exercise the checker only, independently of calibration.
"""
import copy
import json
import unittest
from pathlib import Path

from castle_post_pipe_checks import (
    BOUNDS, PREFIX, PROFILE, check_castle_post_pipe_progress, validate_post_pipe_contract,
)
from panorama_session_checks import action_digest


HERE = Path(__file__).resolve().parent
CONTRACT = json.loads((HERE / 'castle-post-pipe-expectations.json').read_text())
PREFIX_ACTIONS = json.loads((HERE / 'panorama-castle-transition-actions.json').read_text())
ACTIONS = PREFIX_ACTIONS + [dict(n=600, keys=1007) for _ in range(3)] + [
    dict(n=367, keys=1007), dict(n=120, keys=895), dict(n=470, keys=1023)]
TOTAL = sum(a['n'] for a in ACTIONS)
ENTRY_START, DEPARTURE, IDLE_START = 6075, 6100, TOTAL - 299


def rebind(trace, actions):
    """Keep mutations input-consistent so semantic gates, not schema, reject them."""
    cursor = 0
    for index, action in enumerate(actions):
        for row in trace[cursor + 1:cursor + action['n'] + 1]:
            row['action_index'], row['keys'] = index, action['keys']
        cursor += action['n']
    if cursor + 1 != len(trace):
        raise ValueError('Synthetic action duration does not match its fixture')


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


def example(departure=DEPARTURE, durations=(48, 16, 67)):
    """A synthetic two-pipe history with an exact, revalidated input prefix."""
    loading = departure + durations[0]
    arrival = loading + durations[1]
    returning = arrival + durations[2]
    trace = []
    for t in range(TOTAL + 1):
        state = 10 if t < 10 else 12 if t < 60 else 13
        sublevel = 57 if t < 10 else 3
        entry, exit_index, baby = 0, 0, (97, 5)
        if t <= 2238:
            x = min(2662.93359375, 256 + max(0, t - 300) * 2)
            y = 1800 if t < 1550 else 1952.99609375
            vx, vy = (2 if 300 < t < 1504 else 0), 0
        elif t < 3373:
            x = 2662.93359375 + 1343 * (t - 2238) / (3374 - 2238)
            y, vx, vy = 1872.99609375, 2, 0
        elif t == 3373:
            x, y, vx, vy = 4006.125, 1897.75, 0, 6
        elif t < 3521:
            exit_index = 1016
            state, entry = (9, 1) if t < 3422 else (10, 1) if t < 3454 else (11, 0)
            x, y, vx, vy = (4006.125, 1904.75, 0, 6.97265625) if t < 3423 else (368, 1024, 0, 0)
            if 3423 <= t < 3452:
                baby = (0, 0)
        elif t < 3554:
            exit_index = 1016
            x, y, vx, vy = 368, 1024 + 128.99609375 * (t - 3521) / 33, 0, (0 if t == 3521 else 1.5)
        elif t <= PREFIX['frame_count']:
            exit_index = 1016
            x, y, vx, vy = 368, 1152.99609375, 0, 0
        elif t < ENTRY_START:
            exit_index = 1016
            fraction = (t - PREFIX['frame_count']) / (ENTRY_START - PREFIX['frame_count'])
            x, y, vx, vy = 368 + (2216.27734375 - 368) * fraction, 1120.99609375, 2, 0
        elif t < departure:
            exit_index = 1016
            fraction = (t - ENTRY_START) / max(1, departure - 1 - ENTRY_START)
            x, y, vx, vy = (2216.27734375 + 2 * fraction,
                           1120.99609375 + 24.77734375 * fraction, 0, 5.9765625)
        elif t < returning:
            exit_index = 576
            state, entry = (9, 1) if t < loading else (10, 1) if t < arrival else (11, 0)
            if t <= loading:
                x, y, vx, vy = 2218.27734375, 1152.74609375, 0, 6.97265625
            else:
                x, y, vx, vy, sublevel = 464, 1824, 0, 0, 153
                if t < arrival:
                    baby = (0, 0)
        else:
            exit_index, sublevel = 576, 153
            if t < IDLE_START:
                fraction = (t - returning) / max(1, IDLE_START - returning)
                x, y, vx, vy = 464, 1824 + 18.99609375 * fraction, 0, (0 if t == returning else 1)
            else:
                x, y, vx, vy = 464, 1842.99609375, 0, 0
        trace.append(dict(elapsed_frame=t, frame=6599 + t, action_index=-1, keys=None,
            level_id=3, sublevel_id=sublevel, game_state=state, transformation=0,
            player_xy_velocity=[x, y, vx, vy], castle_status=dict(
                game_pointer=0x0300220c, lives=3, deaths=0, inhibition=0, stars=109,
                entry_type=entry, exit_index=exit_index, baby_id=baby[0], baby_status=baby[1])))
    rebind(trace, ACTIONS)
    return trace


def reviewed(actions=ACTIONS):
    value = copy.deepcopy(CONTRACT)
    value['actions_sha256'] = action_digest(actions)
    return value


def check(trace, actions=ACTIONS, contract=None):
    return check_castle_post_pipe_progress(actions, checkpoints(actions, trace), trace,
                                           reviewed(actions) if contract is None else contract)


class PostPipeContractTests(unittest.TestCase):
    def test_public_action_tape_and_exact_prefix_are_bound(self):
        actions = json.loads((HERE / 'panorama-castle-post-pipe-actions.json').read_text())
        validate_post_pipe_contract(CONTRACT)
        self.assertEqual(action_digest(actions), CONTRACT['actions_sha256'])
        self.assertEqual(actions[:65], PREFIX_ACTIONS)
        self.assertEqual(sum(a['n'] for a in actions[:65]), 3907)
        self.assertEqual(action_digest(PREFIX_ACTIONS), PREFIX['actions_sha256'])
        previous = json.loads((HERE / 'castle-transition-expectations.json').read_text())
        self.assertEqual(action_digest(previous), PREFIX['transition_contract_sha256'])
        self.assertGreater(sum(a['n'] for a in actions), 3907 + 300)
        self.assertLessEqual(sum(a['n'] for a in actions), 7200)

    def test_unknown_missing_wrongly_typed_and_weakened_fields_are_rejected(self):
        changes = [('profile', 'other'), ('actions_sha256', 'bad'),
                   ('entry_state_sha256', '0' * 64), ('gameplay_start_frame', 301),
                   ('level_id', 4), ('source_sublevel_id', 153), ('destination_sublevel_id', 3),
                   ('source_screen', 73), ('prior_exit_index', 0), ('exit_index', 1016),
                   ('exit_entry_type', 0), ('return_entry_type', 1), ('pipe_top_y_pixels', 1151),
                   ('minimum_entry_descent_pixels', 30), ('minimum_entry_descent_pixels', True),
                   ('final_idle_frames', 299), ('final_idle_frames', True),
                   ('load_min_consecutive_frames', 1), ('load_position_pixels', [464, 1825]),
                   ('load_position_pixels', [464.0, 1824]), ('level_id', 3.0), ('allow_damage', True)]
        for name, value in changes:
            altered = copy.deepcopy(CONTRACT)
            altered[name] = value
            with self.subTest(name=name, value=value), self.assertRaises(ValueError):
                validate_post_pipe_contract(altered)
        for name in CONTRACT:
            altered = copy.deepcopy(CONTRACT)
            del altered[name]
            with self.subTest(missing=name), self.assertRaises(ValueError):
                validate_post_pipe_contract(altered)

    def test_prefix_geometry_and_ordered_sequence_are_closed(self):
        changes = []
        for name in PREFIX:
            altered = copy.deepcopy(CONTRACT)
            altered['prefix'][name] = 0 if type(PREFIX[name]) is int else '0' * 64
            changes.append(altered)
        altered = copy.deepcopy(CONTRACT)
        altered['prefix']['extra'] = True
        changes.append(altered)
        for name in BOUNDS:
            for field in CONTRACT[name]:
                altered = copy.deepcopy(CONTRACT)
                altered[name][field] += 1
                changes.append(altered)
            altered = copy.deepcopy(CONTRACT)
            altered[name]['extra'] = 0
            changes.append(altered)
        for sequence in ([13], [13, 9, 11, 13], [13, 10, 9, 11, 13], [13, 9, 10, 11, 13, 9]):
            altered = copy.deepcopy(CONTRACT)
            altered['game_state_sequence'] = sequence
            changes.append(altered)
        for index, altered in enumerate(changes):
            with self.subTest(index=index), self.assertRaises(ValueError):
                validate_post_pipe_contract(altered)

    def test_phase_and_idle_requirements_can_only_be_strengthened(self):
        for phase in range(3):
            for key, value in [('game_state', 13), ('min_frames', 1), ('max_frames', 181),
                               ('min_frames', True), ('extra', 0)]:
                altered = copy.deepcopy(CONTRACT)
                altered['phase_limits'][phase][key] = value
                with self.subTest(phase=phase, key=key), self.assertRaises(ValueError):
                    validate_post_pipe_contract(altered)
        stronger = reviewed()
        stronger['phase_limits'][0].update(min_frames=3, max_frames=119)
        stronger['load_min_consecutive_frames'] = 3
        validate_post_pipe_contract(stronger)
        self.assertTrue(check(example(), contract=stronger)['post_pipe_progress_pass'])
        stronger['final_idle_frames'] = 301
        validate_post_pipe_contract(stronger)
        self.assertFalse(check(example(), contract=stronger)['post_pipe_progress_pass'])


class PostPipeProgressTests(unittest.TestCase):
    def assert_failed(self, trace, **kwargs):
        result = check(trace, **kwargs)
        self.assertFalse(result['post_pipe_progress_pass'])
        self.assertTrue(result['post_pipe_progress_errors'])
        self.assertFalse(result['second_natural_transition_validated'])
        return result

    def test_happy_path_revalidates_prefix_and_two_played_transitions(self):
        result = check(example())
        self.assertTrue(result['post_pipe_progress_pass'], result['post_pipe_progress_errors'])
        self.assertEqual(result['profile'], PROFILE)
        self.assertTrue(result['prefix_transition_pass'])
        self.assertTrue(result['prefix_transition_checks']['transition_pass'])
        self.assertEqual(result['total_transition_count'], 2)
        self.assertEqual(result['post_pipe_transition_count'], 1)
        self.assertEqual(result['post_prefix_game_state_sequence'], [13, 9, 10, 11, 13])
        self.assertEqual(result['entry_start_elapsed_frame'], ENTRY_START)
        self.assertEqual(result['entry_descent_pixels'], 31.75)
        self.assertEqual(result['load_position_elapsed_frame'], DEPARTURE + 49)
        self.assertEqual(result['normal_return_elapsed_frame'], DEPARTURE + 131)
        self.assertEqual(result['final_player_xy_velocity'], [464, 1842.99609375, 0, 0])
        self.assertEqual(result['stable_normal_idle_frames'], 300)
        self.assertEqual(result['baby_loss_episodes'], [])

    def test_phase_durations_are_bounded_without_matching_one_calibration(self):
        self.assertTrue(check(example(durations=(60, 40, 75)))['post_pipe_progress_pass'])
        for duration in ((1, 16, 67), (121, 16, 67), (48, 1, 67),
                         (48, 181, 67), (48, 16, 1), (48, 16, 181)):
            with self.subTest(duration=duration):
                self.assert_failed(example(durations=duration))

    def test_wrong_plan_or_changed_prefix_cannot_hide_behind_matching_trace(self):
        with self.assertRaisesRegex(ValueError, 'Actions differ'):
            check(example(), contract=CONTRACT)
        actions, trace = copy.deepcopy(ACTIONS), example()
        actions[0]['keys'] = 1023
        rebind(trace, actions)
        with self.assertRaisesRegex(ValueError, 'first-transition prefix'):
            check(trace, actions=actions)

    def test_inherited_wall_progress_is_rechecked(self):
        trace = example()
        for row in trace[300:1500]:
            row['player_xy_velocity'][0] = 1872.99609375
        result = self.assert_failed(trace)
        self.assertFalse(result['prefix_transition_pass'])

    def test_first_transition_cannot_be_replaced_by_a_forced_return(self):
        trace = example()
        for row in trace[2239:3908]:
            row['game_state'] = 13
            row['castle_status'].update(entry_type=0, exit_index=1016, baby_id=97, baby_status=5)
        self.assertFalse(self.assert_failed(trace)['prefix_transition_pass'])

    def test_prefix_action_boundary_cannot_be_merged_or_shifted(self):
        actions, trace = copy.deepcopy(ACTIONS), example()
        actions[64]['n'] += 1
        actions[65]['n'] -= 1
        rebind(trace, actions)
        with self.assertRaisesRegex(ValueError, 'first-transition prefix'):
            check(trace, actions=actions)

    def test_missing_reordered_or_repeated_phases_are_rejected(self):
        for state in (9, 10, 11):
            trace = example()
            for row in trace[3908:]:
                if row['game_state'] == state:
                    row['game_state'] = 13
            with self.subTest(removed=state):
                self.assert_failed(trace)
        trace = example()
        trace[6500]['game_state'] = 9
        self.assert_failed(trace)

    def test_destination_coordinates_without_a_second_transition_cannot_pass(self):
        trace = example()
        for row in trace[3908:]:
            row['game_state'] = 13
            row['castle_status'].update(entry_type=0, exit_index=1016, baby_id=97, baby_status=5)
        self.assert_failed(trace)

    def test_exit_index_is_required_before_during_and_after_the_pipe(self):
        for t in (4000, DEPARTURE, DEPARTURE + 50, TOTAL):
            trace = example()
            trace[t]['castle_status']['exit_index'] = 0
            with self.subTest(frame=t):
                self.assertFalse(self.assert_failed(trace)['exit_index_pass'])

    def test_entry_type_is_required_on_every_phase_frame(self):
        for t in (4000, DEPARTURE, DEPARTURE + 50, DEPARTURE + 64, TOTAL):
            trace = example()
            trace[t]['castle_status']['entry_type'] ^= 1
            with self.subTest(frame=t):
                self.assertFalse(self.assert_failed(trace)['entry_type_pass'])

    def test_down_must_be_effective_not_missing_or_opposed_by_up(self):
        for mask in (1023, 831, 959):
            actions, trace = copy.deepcopy(ACTIONS), example()
            actions[-2]['keys'] = mask
            rebind(trace, actions)
            with self.subTest(mask=mask):
                self.assertFalse(self.assert_failed(trace, actions=actions)['effective_down_at_exit'])

    def test_entry_geometry_and_descent_use_unrounded_coordinates(self):
        trace = example()
        trace[ENTRY_START]['player_xy_velocity'][0] = 2207.99609375
        self.assert_failed(trace)
        trace = example()
        trace[ENTRY_START]['player_xy_velocity'][1] = 1130
        self.assert_failed(trace)
        trace = example()
        y = trace[ENTRY_START]['player_xy_velocity'][1] + 30.99609375
        for row in trace[DEPARTURE:DEPARTURE + 49]:
            row['player_xy_velocity'][1] = y
        result = self.assert_failed(trace)
        self.assertEqual(result['entry_descent_pixels'], 30.99609375)
        self.assertFalse(result['origin_evidence_pass'])

    def test_sublevel_changes_only_at_load_and_remains_153_after_it(self):
        for t, sublevel in ((DEPARTURE + 48, 153), (DEPARTURE + 49, 3),
                            (DEPARTURE + 90, 3), (6500, 3), (TOTAL, 3)):
            trace = example()
            trace[t]['sublevel_id'] = sublevel
            with self.subTest(frame=t):
                self.assertFalse(self.assert_failed(trace)['sublevel_load_pass'])

    def test_loader_position_and_velocity_are_continuous_through_state11(self):
        for t, component in ((DEPARTURE + 48, 0), (DEPARTURE + 49, 2),
                             (DEPARTURE + 55, 1), (DEPARTURE + 90, 0)):
            trace = example()
            trace[t]['player_xy_velocity'][component] += 1
            with self.subTest(frame=t, component=component):
                self.assertFalse(self.assert_failed(trace)['load_position_pass'])

    def test_return_destination_and_final_zones_are_distinct(self):
        for t, component, value in ((DEPARTURE + 131, 1, 1841),
                                    (6350, 0, 481), (TOTAL, 1, 1857)):
            trace = example()
            trace[t]['player_xy_velocity'][component] = value
            with self.subTest(frame=t):
                self.assert_failed(trace)

    def test_final_idle_requires_300_exact_fixed_safe_buttonless_frames(self):
        trace = example()
        trace[IDLE_START]['player_xy_velocity'][0] += 1 / 256
        self.assertEqual(self.assert_failed(trace)['stable_normal_idle_frames'], 299)
        trace = example()
        trace[TOTAL]['player_xy_velocity'][2] = 1 / 256
        self.assertEqual(self.assert_failed(trace)['stable_normal_idle_frames'], 0)
        actions, trace = copy.deepcopy(ACTIONS), example()
        actions[-1]['keys'] = 1007
        rebind(trace, actions)
        self.assertEqual(self.assert_failed(trace, actions=actions)['stable_normal_idle_frames'], 0)

    def test_final_health_is_required_even_after_an_otherwise_complete_route(self):
        for field, value in (('lives', -1), ('deaths', 1), ('inhibition', 1), ('baby_status', 8)):
            trace = example()
            trace[-1]['castle_status'][field] = value
            with self.subTest(field=field):
                self.assert_failed(trace)

    def test_life_drops_include_prefix_boundary_and_cannot_hide_behind_a_gain(self):
        trace = example()
        trace[3908]['castle_status']['lives'] = 2
        result = self.assert_failed(trace)
        self.assertEqual(result['life_decrement_events'][0]['elapsed_frame'], 3908)
        trace = example()
        trace[4000]['castle_status']['lives'] = 4
        result = self.assert_failed(trace)
        self.assertEqual(result['total_lives_lost'], 1)
        self.assertEqual(result['initial_lives'], result['final_lives'])
        trace = example()
        for row in trace:
            row['castle_status']['lives'] = 0
        trace[4000]['castle_status']['lives'] = -1
        result = self.assert_failed(trace)
        self.assertEqual(result['negative_life_frames'], [4000])
        self.assertEqual(result['life_decrement_events'][0]['decrease'], 1)

    def test_one_frame_death_reset_or_inhibition_is_not_hidden_by_recovery(self):
        for t in (25, 3908, 4200, DEPARTURE + 50):
            trace = example()
            trace[t]['castle_status']['deaths'] = 1
            with self.subTest(death=t):
                result = self.assert_failed(trace)
                self.assertEqual(len(result['death_counter_changes']), 2)
            trace = example()
            trace[t]['castle_status']['inhibition'] = 1
            with self.subTest(inhibition=t):
                self.assertEqual(self.assert_failed(trace)['inhibition_nonzero_frames'], [t])

    def test_recovered_baby_loss_at_boundary_and_later_still_rejects(self):
        for t in (3908, 4200):
            trace = example()
            trace[t]['castle_status']['baby_status'] = 8
            with self.subTest(frame=t):
                result = self.assert_failed(trace)
                self.assertTrue(result['final_baby_safe'])
                self.assertEqual(result['baby_loss_episodes'][0]['start_elapsed_frame'], t)
                self.assertEqual(result['baby_loss_episodes'][0]['recovery_elapsed_frame'], t + 1)

    def test_loader_placeholders_are_allowed_only_in_loading_phases(self):
        good = check(example())
        self.assertTrue(good['baby_health_pass'])
        self.assertTrue(good['loader_baby_empty_frames'])
        for t, baby in ((DEPARTURE, (0, 0)), (DEPARTURE + 50, (97, 8)),
                        (DEPARTURE + 90, (1, 0)), (6500, (0, 0))):
            trace = example()
            trace[t]['castle_status'].update(baby_id=baby[0], baby_status=baby[1])
            with self.subTest(frame=t):
                self.assert_failed(trace)

    def test_transient_state_level_mode_reset_or_missing_health_cannot_pass(self):
        for field, value in (('game_state', 15), ('level_id', 4), ('transformation', 2)):
            trace = example()
            trace[4200][field] = value
            with self.subTest(field=field):
                self.assert_failed(trace)
        trace = example()
        trace[4200]['frame'] -= 1
        with self.assertRaisesRegex(ValueError, 'reset or gap'):
            check(trace)
        trace = example()
        trace[4200].pop('castle_status')
        with self.assertRaisesRegex(ValueError, 'Partial castle_status'):
            check(trace)
        trace = example()
        session = checkpoints(ACTIONS, trace)
        session[-1]['castle_status']['lives'] -= 1
        with self.assertRaisesRegex(ValueError, 'checkpoint differs'):
            check_castle_post_pipe_progress(ACTIONS, session, trace, reviewed())


if __name__ == '__main__':
    unittest.main()
