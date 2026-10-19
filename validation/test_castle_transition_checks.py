#!/usr/bin/env python3
"""Synthetic transition-validator regressions; no game execution is involved."""
import copy
import json
import unittest
from pathlib import Path

from castle_transition_checks import (
    BOUNDS, PREFIX, PROFILE, check_castle_transition, validate_transition_contract,
)
from panorama_session_checks import action_digest


HERE = Path(__file__).resolve().parent
CONTRACT = json.loads((HERE / 'castle-transition-expectations.json').read_text())
PREFIX_ACTIONS = json.loads((HERE / 'panorama-castle-progress-actions.json').read_text())
# A separate synthetic extension keeps unit tests independent of recalibration
# of the public transition action tape. The inherited prefix is still exact.
ACTIONS = PREFIX_ACTIONS + [dict(n=600, keys=1007), dict(n=375, keys=1007),
                            dict(n=100, keys=895), dict(n=180, keys=1023),
                            dict(n=300, keys=1023)]
TOTAL = sum(a['n'] for a in ACTIONS)


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


def example(departure=3250, durations=(48, 32, 67)):
    """Synthetic prefix, downward pipe entry, loader, return and 300-frame rest."""
    loading = departure + durations[0]
    arrival = loading + durations[1]
    returning = arrival + durations[2]
    idle_start = TOTAL - 299
    trace, cursor = [], 0
    for index, action in [(-1, dict(n=1, keys=None)), *enumerate(ACTIONS)]:
        for _ in range(action['n']):
            t = cursor
            mode = 10 if t < 10 else 12 if t < 60 else 13
            entry, exit_index, baby = 0, 0, (97, 5)
            if t <= PREFIX['frame_count']:
                x = min(2662.93359375, 256 + max(0, t - 300) * 2)
                y = 1800 if t < 1550 else 1952.99609375
                velocity = [2 if 300 < t < 1504 else 0, 0]
            elif t < departure - 1:
                fraction = (t - PREFIX['frame_count']) / (departure - PREFIX['frame_count'])
                x, y, velocity = 2662.93359375 + 1343 * fraction, 1872.99609375, [2, 0]
            elif t == departure - 1:
                x, y, velocity = 4006.125, 1897.75, [0, 6]
            else:
                exit_index = 1016
                if t < loading:
                    mode, entry = 9, 1
                    x, y, velocity = 4006.125, 1904.75, [0, 6.97265625]
                elif t < arrival:
                    mode, entry = 10, 1
                    if t == loading:
                        x, y, velocity = 4006.125, 1904.75, [0, 6.97265625]
                    else:
                        x, y, velocity = 368, 1024, [0, 0]
                        if t < arrival - 2:
                            baby = (0, 0)
                elif t < returning:
                    mode = 11
                    x, y, velocity = 368, 1024, [0, 0]
                elif t < idle_start:
                    mode = 13
                    fraction = (t - returning) / (idle_start - returning)
                    x, y = 368, 1024 + 128.99609375 * fraction
                    velocity = [0, 0 if t == returning else 1.5]
                else:
                    mode = 13
                    x, y, velocity = 368, 1152.99609375, [0, 0]
            trace.append(dict(elapsed_frame=t, frame=6599 + t,
                action_index=index, keys=action['keys'], level_id=3,
                sublevel_id=57 if t < 10 else 3, game_state=mode,
                transformation=0, player_xy_velocity=[x, y, *velocity],
                castle_status=dict(game_pointer=0x0300220c, lives=3, deaths=0,
                    inhibition=0, stars=109, entry_type=entry, exit_index=exit_index,
                    baby_id=baby[0], baby_status=baby[1])))
            cursor += 1
    return trace


def reviewed(actions=ACTIONS):
    contract = copy.deepcopy(CONTRACT)
    contract['actions_sha256'] = action_digest(actions)
    return contract


def check(trace, actions=ACTIONS, contract=None):
    return check_castle_transition(actions, checkpoints(actions, trace), trace,
                                   reviewed(actions) if contract is None else contract)


class TransitionContractTests(unittest.TestCase):
    def test_public_tape_and_inherited_prefix_are_bound(self):
        actions = json.loads((HERE / 'panorama-castle-transition-actions.json').read_text())
        validate_transition_contract(CONTRACT)
        self.assertEqual(action_digest(actions), CONTRACT['actions_sha256'])
        self.assertEqual(actions[:37], PREFIX_ACTIONS)
        self.assertEqual(action_digest(actions[:37]), PREFIX['actions_sha256'])
        self.assertEqual(sum(a['n'] for a in actions[:37]), 2238)
        self.assertGreater(sum(a['n'] for a in actions), 2238 + 300)

    def test_unknown_missing_wrongly_typed_and_weakened_fields_are_rejected(self):
        changes = [('profile', 'other'), ('actions_sha256', 'bad'),
                   ('entry_state_sha256', '0' * 64), ('gameplay_start_frame', 299),
                   ('level_id', 4), ('sublevel_id', 57), ('source_screen', 126),
                   ('exit_index', 0), ('exit_entry_type', 0), ('return_entry_type', 1),
                   ('exit_min_y_pixels', 1903), ('source_screen', 127.0),
                   ('exit_entry_type', True), ('final_idle_frames', 299),
                   ('final_idle_frames', True), ('load_min_consecutive_frames', 1),
                   ('load_position_pixels', [369, 1024]),
                   ('load_position_pixels', [368.0, 1024]), ('accept_deaths', True)]
        for name, value in changes:
            changed = copy.deepcopy(CONTRACT)
            changed[name] = value
            with self.subTest(name=name, value=value), self.assertRaises(ValueError):
                validate_transition_contract(changed)
        for name in CONTRACT:
            changed = copy.deepcopy(CONTRACT)
            changed.pop(name)
            with self.subTest(missing=name), self.assertRaises(ValueError):
                validate_transition_contract(changed)

    def test_prefix_geometry_and_sequence_are_closed(self):
        cases = []
        for name, value in [('action_count', 36), ('frame_count', 2237),
                            ('actions_sha256', '0' * 64), ('extra', 0)]:
            changed = copy.deepcopy(CONTRACT)
            changed['prefix'][name] = value
            cases.append(changed)
        for name in BOUNDS:
            for field in ('x_min_pixels', 'x_max_pixels', 'y_min_pixels', 'y_max_pixels'):
                changed = copy.deepcopy(CONTRACT)
                changed[name][field] += 1
                cases.append(changed)
            changed = copy.deepcopy(CONTRACT)
            changed[name]['extra'] = 1
            cases.append(changed)
        for sequence in ([13], [13, 9, 10, 13], [13, 10, 9, 11, 13],
                         [13, 9, 10, 11, 13, 9, 10, 11, 13]):
            changed = copy.deepcopy(CONTRACT)
            changed['game_state_sequence'] = sequence
            cases.append(changed)
        for index, changed in enumerate(cases):
            with self.subTest(case=index), self.assertRaises(ValueError):
                validate_transition_contract(changed)

    def test_phase_limits_can_only_be_strengthened(self):
        for index in range(3):
            for name, value in [('game_state', 13), ('min_frames', 1),
                                ('max_frames', 181), ('min_frames', True), ('extra', 0)]:
                changed = copy.deepcopy(CONTRACT)
                changed['phase_limits'][index][name] = value
                with self.subTest(index=index, name=name), self.assertRaises(ValueError):
                    validate_transition_contract(changed)
        stronger = reviewed()
        stronger['phase_limits'][0].update(min_frames=3, max_frames=119)
        stronger['load_min_consecutive_frames'] = 3
        validate_transition_contract(stronger)
        self.assertTrue(check(example(), contract=stronger)['transition_pass'])
        stronger['final_idle_frames'] = 301
        validate_transition_contract(stronger)
        self.assertFalse(check(example(), contract=stronger)['transition_pass'])

    def test_wrong_actions_or_altered_prefix_raise_even_with_consistent_trace(self):
        trace = example()
        with self.assertRaisesRegex(ValueError, 'Actions differ'):
            check_castle_transition(ACTIONS, checkpoints(ACTIONS, trace), trace, CONTRACT)
        actions = copy.deepcopy(ACTIONS)
        actions[0]['keys'] = 1023
        for row in trace[1:actions[0]['n'] + 1]:
            row['keys'] = 1023
        with self.assertRaisesRegex(ValueError, 'route prefix'):
            check(trace, actions)


class CastleTransitionTests(unittest.TestCase):
    def test_happy_path_proves_transition_even_when_sublevel_is_unchanged(self):
        result = check(example())
        self.assertTrue(result['transition_pass'], result['transition_errors'])
        self.assertEqual(result['profile'], PROFILE)
        self.assertEqual(result['post_warmup_game_state_sequence'], [13, 9, 10, 11, 13])
        self.assertEqual(result['game_state_sequence'], [10, 12, 13, 9, 10, 11, 13])
        self.assertEqual(result['transition_count'], 1)
        self.assertEqual(result['departure_elapsed_frame'], 3250)
        self.assertEqual(result['loading_start_elapsed_frame'], 3298)
        self.assertEqual(result['load_position_elapsed_frame'], 3299)
        self.assertEqual(result['arrival_elapsed_frame'], 3330)
        self.assertEqual(result['normal_return_elapsed_frame'], 3397)
        self.assertEqual(result['stable_normal_idle_frames'], 300)
        self.assertEqual(result['baby_loss_episodes'], [])
        self.assertEqual(len(result['loader_baby_empty_frames']), 29)
        self.assertTrue(result['natural_transition_validated'])
        self.assertTrue(result['prefix_progress_pass'])

    def test_phase_durations_are_bounded_without_matching_calibration_exactly(self):
        result = check(example(durations=(60, 40, 75)))
        self.assertTrue(result['transition_pass'], result['transition_errors'])
        for durations in ((1, 32, 67), (121, 32, 67), (48, 1, 67), (48, 32, 1)):
            with self.subTest(durations=durations):
                self.assertFalse(check(example(durations=durations))['transition_pass'])

    def test_no_exit_and_fake_coordinate_return_cannot_pass(self):
        for moved in (False, True):
            trace = example()
            for row in trace[2239:]:
                row['game_state'] = 13
                row['castle_status'].update(entry_type=0, exit_index=0)
                if not moved:
                    row['player_xy_velocity'] = [3926.65234375, 1888.99609375, 0, 0]
            result = check(trace)
            with self.subTest(fake_coordinate_return=moved):
                self.assertFalse(result['transition_pass'])
                self.assertEqual(result['transition_count'], 0)
                self.assertFalse(result['natural_transition_validated'])

    def test_omitted_reordered_repeated_or_one_frame_illegal_states_fail(self):
        for failure in ('omit9', 'omit10', 'omit11', 'reorder', 'repeat', 'death', 'warmup_death'):
            trace = example()
            if failure.startswith('omit'):
                target = int(failure[4:])
                for row in trace[300:]:
                    if row['game_state'] == target:
                        row['game_state'] = 13
            elif failure == 'reorder':
                for row in trace[3250:3330]:
                    row['game_state'] = 10 if row['game_state'] == 9 else 9
            elif failure == 'repeat':
                for frame, state in ((3450, 9), (3451, 10), (3452, 11)):
                    trace[frame]['game_state'] = state
            else:
                trace[50 if failure == 'warmup_death' else 3000]['game_state'] = 15
            with self.subTest(failure=failure):
                self.assertFalse(check(trace)['transition_pass'])

    def test_prefix_must_demonstrate_progress_not_just_have_the_old_inputs(self):
        trace = example()
        for row in trace[:2239]:
            row['player_xy_velocity'] = [1872.99609375, 1800, 0, 0]
        result = check(trace)
        self.assertFalse(result['prefix_progress_pass'])
        self.assertFalse(result['transition_pass'])
        self.assertTrue(result['origin_evidence_pass'])

    def test_origin_requires_unrounded_coordinates_and_downward_continuity(self):
        for failure in ('old_wall', 'below_threshold', 'teleport', 'upward', 'no_prior_origin'):
            trace = example()
            if failure == 'old_wall':
                trace[3250]['player_xy_velocity'][0] = 1872.99609375
            elif failure == 'below_threshold':
                trace[3250]['player_xy_velocity'][1] = 1903.99609375
            elif failure == 'teleport':
                trace[3249]['player_xy_velocity'][1] = 1872
            elif failure == 'upward':
                trace[3250]['player_xy_velocity'][3] = -1
            else:
                trace[3249]['player_xy_velocity'][0] = 3900
            result = check(trace)
            with self.subTest(failure=failure):
                self.assertFalse(result['origin_evidence_pass'])
                self.assertFalse(result['transition_pass'])

    def test_correct_states_and_source_without_effective_down_fail(self):
        # Elapsed 3213 is a real Right frame in the synthetic reviewed tape.
        result = check(example(departure=3213))
        self.assertTrue(result['phase_sequence_pass'])
        self.assertTrue(result['origin_evidence_pass'])
        self.assertFalse(result['effective_down_at_exit'])
        self.assertFalse(result['transition_pass'])

    def test_exit_index_must_change_at_departure_and_never_reset(self):
        for frame, value in ((3000, 1016), (3250, 1015), (3299, 0), (TOTAL, 0)):
            trace = example()
            trace[frame]['castle_status']['exit_index'] = value
            result = check(trace)
            with self.subTest(frame=frame, value=value):
                self.assertFalse(result['transition_pass'])
                self.assertIn(frame, result['unexpected_exit_index_frames'])

    def test_entry_type_matches_each_phase_including_final_state(self):
        for frame, value in ((3000, 1), (3250, 0), (3299, 0), (3330, 1), (TOTAL, 1)):
            trace = example()
            trace[frame]['castle_status']['entry_type'] = value
            result = check(trace)
            with self.subTest(frame=frame, value=value):
                self.assertFalse(result['transition_pass'])
                self.assertIn(frame, result['unexpected_entry_type_frames'])

    def test_load_must_be_sustained_in_state10_and_through_state11(self):
        for failure in ('wrong_x', 'subpixel_y', 'velocity', 'only_one_load_frame', 'intro_drift'):
            trace = example()
            if failure == 'wrong_x':
                trace[3299]['player_xy_velocity'][0] = 369
            elif failure == 'subpixel_y':
                trace[3301]['player_xy_velocity'][1] += 1 / 256
            elif failure == 'velocity':
                trace[3301]['player_xy_velocity'][2] = 1 / 256
            elif failure == 'only_one_load_frame':
                for row in trace[3298:3329]:
                    row['player_xy_velocity'] = [4006.125, 1904.75, 0, 6.97265625]
            else:
                trace[3350]['player_xy_velocity'][0] += 1 / 256
            result = check(trace)
            with self.subTest(failure=failure):
                self.assertFalse(result['load_position_pass'])
                self.assertFalse(result['transition_pass'])

    def test_fade_cannot_hide_an_extra_position_change(self):
        trace = example()
        trace[3280]['player_xy_velocity'][0] += 1 / 256
        result = check(trace)
        self.assertFalse(result['transition_pass'])
        self.assertTrue(any('exit fade' in error for error in result['transition_errors']))

    def test_normal_return_and_final_rest_must_be_in_the_destination(self):
        for failure in ('return_source', 'wrong_return_height', 'excursion', 'final_source', 'final_load_point'):
            trace = example()
            if failure == 'return_source':
                trace[3397]['player_xy_velocity'] = [4006.125, 1904.75, 0, 0]
            elif failure == 'wrong_return_height':
                trace[3397]['player_xy_velocity'][1] = 1040 + 1 / 256
            elif failure == 'excursion':
                trace[3420]['player_xy_velocity'][0] = 385
            else:
                for row in trace[TOTAL - 299:]:
                    row['player_xy_velocity'] = ([4000, 1904, 0, 0] if failure == 'final_source'
                                                   else [368, 1024, 0, 0])
            with self.subTest(failure=failure):
                self.assertFalse(check(trace)['transition_pass'])

    def test_level_sublevel_and_transformation_cannot_change_during_loading(self):
        for frame, field, value in ((3000, 'sublevel_id', 4), (3299, 'sublevel_id', 57),
                                    (3330, 'level_id', 4), (3299, 'transformation', 2),
                                    (25, 'transformation', 2)):
            trace = example()
            trace[frame][field] = value
            with self.subTest(frame=frame, field=field):
                self.assertFalse(check(trace)['transition_pass'])

    def test_earned_life_cannot_mask_later_life_decrement(self):
        trace = example()
        for row in trace[2300:3000]:
            row['castle_status']['lives'] = 4
        result = check(trace)
        self.assertFalse(result['transition_pass'])
        self.assertEqual(result['initial_lives'], result['final_lives'])
        self.assertEqual(result['total_lives_lost'], 1)
        self.assertEqual(result['life_decrement_events'][0]['elapsed_frame'], 3000)

    def test_negative_lives_and_zero_to_minus_one_are_rejected(self):
        for failure in ('negative_constant', 'last_life_loss', 'final_loss'):
            trace = example()
            if failure == 'negative_constant':
                for row in trace:
                    row['castle_status']['lives'] = -1
            elif failure == 'last_life_loss':
                for row in trace:
                    row['castle_status']['lives'] = 0
                trace[3300]['castle_status']['lives'] = -1
            else:
                trace[-1]['castle_status']['lives'] = 2
            result = check(trace)
            with self.subTest(failure=failure):
                self.assertFalse(result['transition_pass'])
        trace = example()
        for row in trace:
            row['castle_status']['lives'] = 0
        self.assertTrue(check(trace)['transition_pass'])

    def test_one_frame_death_or_counter_reset_is_detected_between_endpoints(self):
        for frame in (5, 3000, 3300, TOTAL):
            trace = example()
            trace[frame]['castle_status']['deaths'] = 1
            result = check(trace)
            with self.subTest(frame=frame):
                self.assertFalse(result['transition_pass'])
                self.assertEqual(result['death_counter_nonzero_frames'], [frame])
                self.assertEqual(len(result['death_counter_changes']), 1 if frame == TOTAL else 2)

    def test_inhibition_is_zero_even_inside_loader_and_at_final_frame(self):
        for frame in (5, 3300, TOTAL):
            trace = example()
            trace[frame]['castle_status']['inhibition'] = 1
            result = check(trace)
            with self.subTest(frame=frame):
                self.assertFalse(result['transition_pass'])
                self.assertEqual(result['inhibition_nonzero_frames'], [frame])

    def test_recovered_gameplay_baby_loss_still_rejects_the_route(self):
        trace = example()
        for row in trace[3017:3050]:
            row['castle_status']['baby_status'] = 8
        for row in trace[3350:3353]:
            row['castle_status'].update(baby_id=0, baby_status=0)
        result = check(trace)
        self.assertFalse(result['transition_pass'])
        self.assertFalse(result['baby_health_pass'])
        self.assertEqual(result['baby_loss_episodes'], [dict(start_elapsed_frame=3017,
            end_elapsed_frame=3049, observed_frames=33, recovery_elapsed_frame=3050)])
        self.assertEqual(len(result['loader_baby_empty_frames']), 32)

    def test_loader_zeroes_are_allowed_without_a_baby_loss_episode(self):
        trace = example()
        for row in trace[3350:3353]:
            row['castle_status'].update(baby_id=0, baby_status=0)
        result = check(trace)
        self.assertTrue(result['transition_pass'], result['transition_errors'])
        self.assertTrue(result['baby_health_pass'])
        self.assertEqual(result['baby_loss_episodes'], [])

    def test_exit_requires_safe_baby_and_loader_rejects_anomalous_pairs(self):
        for frame, baby in ((3251, (0, 0)), (3251, (97, 8)),
                             (3300, (97, 0)), (3350, (0, 5)), (3350, (97, 8))):
            trace = example()
            trace[frame]['castle_status'].update(baby_id=baby[0], baby_status=baby[1])
            result = check(trace)
            with self.subTest(frame=frame, baby=baby):
                self.assertFalse(result['transition_pass'])
                self.assertEqual(result['unexpected_transition_baby_frames'], [frame])

    def test_empty_baby_in_gameplay_is_a_loss_and_final_baby_must_be_safe(self):
        trace = example()
        trace[3000]['castle_status'].update(baby_id=0, baby_status=0)
        result = check(trace)
        self.assertFalse(result['transition_pass'])
        self.assertEqual(result['baby_loss_episodes'][0]['start_elapsed_frame'], 3000)
        for frame, field, value in ((TOTAL, 'baby_id', 0), (TOTAL, 'baby_status', 8),
                                    (TOTAL - 298, 'baby_status', 8)):
            trace = example()
            trace[frame]['castle_status'][field] = value
            with self.subTest(frame=frame, field=field):
                self.assertFalse(check(trace)['transition_pass'])

    def test_final_idle_requires_exact_xy_zero_velocities_and_300_safe_frames(self):
        for component in range(4):
            trace = example()
            trace[TOTAL - 298]['player_xy_velocity'][component] += 1 / 256
            result = check(trace)
            with self.subTest(component=component):
                self.assertFalse(result['transition_pass'])
                self.assertEqual(result['stable_normal_idle_frames'], 298)
        trace = example()
        trace[TOTAL - 299]['player_xy_velocity'][2] = 1 / 256
        result = check(trace)
        self.assertFalse(result['transition_pass'])
        self.assertEqual(result['stable_normal_idle_frames'], 299)

    def test_trace_resets_gaps_bad_inputs_and_checkpoint_disagreement_raise(self):
        for failure in ('reset', 'gap', 'keys', 'status_checkpoint', 'missing_status', 'invalid_pointer'):
            trace = example()
            session = checkpoints(ACTIONS, trace)
            if failure == 'reset':
                trace[3000]['frame'] = 0
            elif failure == 'gap':
                trace.pop(3000)
            elif failure == 'keys':
                trace[-2]['keys'] = 1007
            elif failure == 'status_checkpoint':
                session[-1]['castle_status']['lives'] += 1
            elif failure == 'missing_status':
                for row in [*trace, *session]:
                    row.pop('castle_status')
            else:
                trace[3300]['castle_status']['game_pointer'] += 1
            with self.subTest(failure=failure), self.assertRaises(ValueError):
                check_castle_transition(ACTIONS, session, trace, reviewed())


if __name__ == '__main__':
    unittest.main()
