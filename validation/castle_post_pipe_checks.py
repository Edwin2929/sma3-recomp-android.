"""Qualify a second played castle pipe after the unchanged first-transition route.

The caller must bind the entry-state digest and castle-status-v1 observation
profile. Synthetic test histories are not evidence of native gameplay.
"""
import json
from pathlib import Path

from castle_transition_checks import (
    ENTRY_STATE_SHA256, STATE_SEQUENCE, SCENE_FIELDS, BOUND_FIELDS, PHASE_LIMITS,
    _baby_losses, _baby_safe, _digest, _down, _health, _inside, _runs,
    check_castle_transition, validate_transition_contract,
)
from panorama_session_checks import MAX_SESSION_FRAMES, action_digest, require, validate_trace


PROFILE = 'castle-post-pipe-progress-v1'
PREFIX = dict(action_count=65, frame_count=3907,
    actions_sha256='e87a2b74b03b8ac9871254617ab2d7a3698f54a966f4b53f55a51aa0e5c12d93',
    transition_contract_sha256='0cf81e0a68d24e13d854987dd18bc990a5a214cc0fa94c77919c412d5465529b')
PREFIX_CONTRACT_PATH = Path(__file__).with_name('castle-transition-expectations.json')
BOUNDS = {
    'entry_bounds': (2208, 2240, 1104, 1136),
    'origin_bounds': (2208, 2240, 1136, 1168),
    'return_bounds': (448, 480, 1808, 1840),
    'destination_bounds': (448, 480, 1808, 1872),
    'final_bounds': (448, 480, 1832, 1856),
}


def validate_post_pipe_contract(value):
    """Close the schema, source, geometry and minimum evidence requirements."""
    fields = {'profile', 'actions_sha256', 'entry_state_sha256',
              'gameplay_start_frame', 'level_id', 'source_sublevel_id',
              'destination_sublevel_id', 'prefix', 'game_state_sequence',
              'source_screen', 'prior_exit_index', 'exit_index', 'exit_entry_type',
              'return_entry_type', 'pipe_top_y_pixels', 'minimum_entry_descent_pixels',
              'phase_limits', 'load_position_pixels', 'load_min_consecutive_frames',
              'final_idle_frames', *BOUNDS}
    require(type(value) is dict and set(value) == fields,
            'Invalid castle post-pipe contract fields')
    require(value['profile'] == PROFILE, 'Unknown castle post-pipe profile')
    require(_digest(value['actions_sha256']), 'Invalid post-pipe actions digest')
    require(value['entry_state_sha256'] == ENTRY_STATE_SHA256,
            'Unexpected castle entry-state digest')
    fixed = dict(gameplay_start_frame=300, level_id=3, source_sublevel_id=3,
                 destination_sublevel_id=153, source_screen=72, prior_exit_index=1016,
                 exit_index=576, exit_entry_type=1, return_entry_type=0, pipe_top_y_pixels=1152)
    for name, expected in fixed.items():
        require(type(value[name]) is int and value[name] == expected,
                'Unexpected fixed castle post-pipe field: ' + name)
    prefix = value['prefix']
    require(type(prefix) is dict and set(prefix) == set(PREFIX) and
            type(prefix['action_count']) is int and type(prefix['frame_count']) is int and
            prefix == PREFIX, 'Do not change the reviewed first-transition prefix')
    sequence = value['game_state_sequence']
    require(type(sequence) is list and all(type(s) is int for s in sequence) and
            sequence == STATE_SEQUENCE, 'Do not change the ordered post-pipe phases')
    for name, geometry in BOUNDS.items():
        bounds = value[name]
        require(type(bounds) is dict and set(bounds) == set(BOUND_FIELDS) and
                all(type(bounds[k]) is int for k in BOUND_FIELDS) and
                tuple(bounds[k] for k in BOUND_FIELDS) == geometry,
                'Do not change the reviewed post-pipe geometry: ' + name)
    limits = value['phase_limits']
    require(type(limits) is list and len(limits) == 3,
            'Exactly three ordered post-pipe phase limits required')
    for limit, (state, minimum, maximum) in zip(limits, PHASE_LIMITS):
        require(type(limit) is dict and set(limit) ==
                {'game_state', 'min_frames', 'max_frames'} and
                all(type(v) is int for v in limit.values()), 'Invalid post-pipe phase limit')
        require(limit['game_state'] == state and
                minimum <= limit['min_frames'] <= limit['max_frames'] <= maximum,
                'Do not weaken or reorder the bounded post-pipe phase limits')
    load = value['load_position_pixels']
    require(type(load) is list and len(load) == 2 and
            all(type(v) is int for v in load) and load == [464, 1824],
            'Unexpected second-pipe load position')
    require(type(value['minimum_entry_descent_pixels']) is int and
            31 <= value['minimum_entry_descent_pixels'] <= 64,
            'Do not weaken the measured pipe-entry descent')
    require(type(value['load_min_consecutive_frames']) is int and
            2 <= value['load_min_consecutive_frames'] <= 180,
            'Do not weaken the consecutive load-position evidence')
    require(type(value['final_idle_frames']) is int and
            300 <= value['final_idle_frames'] <= MAX_SESSION_FRAMES,
            'Do not weaken the final safe, stable idle requirement')


def check_castle_post_pipe_progress(actions, session, trace, contract):
    """Return measured gates; malformed, unbound or incomplete evidence raises."""
    validate_post_pipe_contract(contract)
    total = validate_trace(actions, session, trace)
    require(all('castle_status' in row for row in trace),
            'Castle post-pipe progress requires per-frame castle-status observations')
    require(action_digest(actions) == contract['actions_sha256'],
            'Actions differ from the reviewed castle post-pipe contract')
    count, prefix_frames = PREFIX['action_count'], PREFIX['frame_count']
    require(len(actions) > count and action_digest(actions[:count]) == PREFIX['actions_sha256'] and
            sum(a['n'] for a in actions[:count]) == prefix_frames and
            session[count - 1]['elapsed_frame'] == prefix_frames,
            'Actions do not preserve the reviewed first-transition prefix')
    previous_contract = json.loads(PREFIX_CONTRACT_PATH.read_text())
    require(action_digest(previous_contract) == PREFIX['transition_contract_sha256'],
            'Inherited first-transition contract differs from its reviewed digest')
    validate_transition_contract(previous_contract)
    prefix = check_castle_transition(actions[:count], session[:count],
                                    trace[:prefix_frames + 1], previous_contract)
    errors = []
    if not prefix['transition_pass'] or prefix['transition_errors']:
        errors.append('Inherited first-transition prefix did not satisfy its unchanged contract')
    start = contract['gameplay_start_frame']
    level_bad = [r['elapsed_frame'] for r in trace[start:] if r['level_id'] != 3]
    transformation_bad = [r['elapsed_frame'] for r in trace if r['transformation'] != 0]
    if level_bad:
        errors.append('Expected level 3 throughout the post-warm-up route and loading')
    if transformation_bad:
        errors.append('Transformation must remain 0 throughout the cumulative route')
    # Full history, including the 3907 -> 3908 boundary, is intentional.
    health = _health(trace, errors)
    losses = _baby_losses(trace)
    if losses:
        errors.append('Baby Mario must remain safely attached during normal gameplay, even if later recovered')
    baby_bad = [r['elapsed_frame'] for r in trace[start:]
                if (r['game_state'] == 9 and not _baby_safe(r)) or
                (r['game_state'] in (10, 11) and
                 (r['castle_status']['baby_id'], r['castle_status']['baby_status'])
                 not in ((97, 5), (0, 0)))]
    if baby_bad:
        errors.append('Baby Mario must be 97/5 in state 9; only 97/5 or loader 0/0 is allowed in 10/11')

    runs = _runs(trace[prefix_frames:], ('game_state',))
    sequence = [r['game_state'] for r in runs]
    phase_ok = sequence == STATE_SEQUENCE
    if not phase_ok:
        errors.append('Expected exactly one additional transition 13 -> 9 -> 10 -> 11 -> 13 after the prefix')
    entry_start = departure = load_start = loaded = arrival = normal_return = None
    descent = None
    down_ok = origin_ok = exit_ok = entry_ok = load_ok = return_ok = False
    phases, unexpected_exit, unexpected_entry = [], [], []
    load_frames = 0
    if phase_ok:
        departure, load_start, arrival, normal_return = (
            runs[i]['start_elapsed_frame'] for i in (1, 2, 3, 4))
        for run, limit in zip(runs[1:4], contract['phase_limits']):
            passed = limit['min_frames'] <= run['observed_frames'] <= limit['max_frames']
            phases.append(dict(run, min_frames=limit['min_frames'],
                               max_frames=limit['max_frames'], passed=passed))
            if not passed:
                errors.append('Post-pipe phase duration is outside reviewed limits: ' + str(run['game_state']))
        before, first = trace[departure - 1], trace[departure]
        before_xy, first_xy = before['player_xy_velocity'], first['player_xy_velocity']
        down_ok = _down(before) and _down(first)
        if down_ok:
            entry_start = departure - 1
            while (entry_start > prefix_frames + 1 and _down(trace[entry_start - 1]) and
                   trace[entry_start - 1]['game_state'] == 13):
                entry_start -= 1
            descent = first_xy[1] - trace[entry_start]['player_xy_velocity'][1]
            origin_ok = (
                _inside(trace[entry_start], contract['entry_bounds']) and
                _inside(before, contract['origin_bounds']) and
                _inside(first, contract['origin_bounds']) and
                descent >= contract['minimum_entry_descent_pixels'] and
                before_xy[3] >= 0 and first_xy[3] > 0 and
                abs(first_xy[0] - before_xy[0]) <= 16 and
                0 < first_xy[1] - before_xy[1] <= 16)
        if not down_ok:
            errors.append('Effective Down input is required immediately before and at the second pipe exit')
        if not origin_ok:
            errors.append('Missing measured descent from the reviewed Down-entry area to the second pipe mouth')
        for row in trace[prefix_frames:]:
            elapsed, status = row['elapsed_frame'], row['castle_status']
            expected_exit = (contract['prior_exit_index'] if elapsed < departure else contract['exit_index'])
            expected_entry = (contract['exit_entry_type'] if departure <= elapsed < arrival
                              else contract['return_entry_type'])
            if status['exit_index'] != expected_exit:
                unexpected_exit.append(elapsed)
            if status['entry_type'] != expected_entry:
                unexpected_entry.append(elapsed)
        exit_ok, entry_ok = not unexpected_exit, not unexpected_entry
        if not exit_ok:
            errors.append('Exit index must change from 1016 to 576 at the second state 9 and persist')
        if not entry_ok:
            errors.append('Entry type must be 1 during states 9/10 and 0 before/after them')
        if any(r['player_xy_velocity'] != first_xy for r in trace[departure:load_start]):
            errors.append('Departure position/velocity changed during the second exit fade')
        expected_load = [*contract['load_position_pixels'], 0, 0]
        loaded = next((r['elapsed_frame'] for r in trace[load_start:arrival]
                       if r['player_xy_velocity'] == expected_load), None)
        if loaded is not None:
            load_frames = arrival - loaded
            load_ok = (load_frames >= contract['load_min_consecutive_frames'] and
                       all(r['player_xy_velocity'] == first_xy for r in trace[load_start:loaded]) and
                       all(r['player_xy_velocity'] == expected_load for r in trace[loaded:normal_return]))
        if not load_ok:
            errors.append('Missing sustained state-10 load at (464, 1824) through state 11')
        return_ok = (_inside(trace[normal_return], contract['return_bounds']) and
                     all(_inside(r, contract['destination_bounds']) for r in trace[normal_return:]))
        if not return_ok:
            errors.append('Normal gameplay did not resume and remain inside the second destination area')

    sublevel_bad = [r['elapsed_frame'] for r in trace[start:]
        if r['sublevel_id'] != (153 if loaded is not None and r['elapsed_frame'] >= loaded else 3)]
    if sublevel_bad:
        errors.append('Sublevel must remain 3 until destination load and 153 on every frame thereafter')
    final = trace[-1]
    final_xy, idle = final['player_xy_velocity'][:2], 0
    for row in reversed(trace[1:]):
        if (normal_return is None or row['elapsed_frame'] < normal_return or
                tuple(row[k] for k in SCENE_FIELDS) != (3, 153, 13, 0) or
                row['keys'] != 1023 or row['player_xy_velocity'][:2] != final_xy or
                row['player_xy_velocity'][2:] != [0, 0] or not _baby_safe(row) or
                row['castle_status']['entry_type'] != 0 or row['castle_status']['exit_index'] != 576):
            break
        idle += 1
    final_ok = _inside(final, contract['final_bounds'])
    if not final_ok:
        errors.append('Final position is outside the reviewed second-destination landing area')
    if not _baby_safe(final):
        errors.append('Baby Mario is not safely attached at the final frame')
    if idle < contract['final_idle_frames']:
        errors.append('Insufficient final destination idle with fixed position, zero velocity and Baby safe')
    return dict(
        profile=PROFILE, post_pipe_progress_pass=not errors, post_pipe_progress_errors=errors,
        actions_sha256=action_digest(actions), entry_state_sha256=contract['entry_state_sha256'],
        frames=total, post_pipe_frames=total - prefix_frames, observation_interval_frames=1,
        prefix=dict(PREFIX), prefix_transition_pass=prefix['transition_pass'],
        prefix_transition_errors=prefix['transition_errors'], prefix_transition_checks=prefix,
        scene_sequence=_runs(trace, SCENE_FIELDS),
        game_state_sequence=[r['game_state'] for r in _runs(trace, ('game_state',))],
        post_warmup_game_state_sequence=[r['game_state'] for r in _runs(trace[start:], ('game_state',))],
        post_prefix_game_state_sequence=sequence,
        post_pipe_transition_count=sum(a['game_state'] == 13 and b['game_state'] == 9
            for a, b in zip(trace[prefix_frames:], trace[prefix_frames + 1:])),
        total_transition_count=sum(a['game_state'] == 13 and b['game_state'] == 9
            for a, b in zip(trace[start:], trace[start + 1:])),
        phase_sequence_pass=phase_ok, phase_measurements=phases,
        entry_start_elapsed_frame=entry_start, departure_elapsed_frame=departure,
        loading_start_elapsed_frame=load_start, load_position_elapsed_frame=loaded,
        load_position_frames_in_state10=load_frames, arrival_elapsed_frame=arrival,
        normal_return_elapsed_frame=normal_return, effective_down_at_exit=down_ok,
        entry_descent_pixels=descent, origin_evidence_pass=origin_ok,
        exit_index_pass=exit_ok, unexpected_exit_index_frames=unexpected_exit,
        entry_type_pass=entry_ok, unexpected_entry_type_frames=unexpected_entry,
        sublevel_load_pass=not sublevel_bad, unexpected_sublevel_frames=sublevel_bad,
        load_position_pass=load_ok, destination_return_pass=return_ok, final_position_pass=final_ok,
        stable_normal_idle_frames=idle, final_player_xy_velocity=final['player_xy_velocity'],
        final_game_state=final['game_state'], final_sublevel_id=final['sublevel_id'],
        final_transformation=final['transformation'],
        initial_lives=trace[0]['castle_status']['lives'], final_lives=final['castle_status']['lives'],
        final_baby_safe=_baby_safe(final), baby_loss_episodes=losses,
        unexpected_transition_baby_frames=baby_bad, baby_health_pass=not losses and not baby_bad,
        loader_baby_empty_frames=[r['elapsed_frame'] for r in trace
            if r['game_state'] in (10, 11) and
            (r['castle_status']['baby_id'], r['castle_status']['baby_status']) == (0, 0)],
        unexpected_transformation_frames=transformation_bad, unexpected_level_frames=level_bad,
        reviewed_limits={name: contract[name] for name in (*BOUNDS, 'phase_limits',
            'minimum_entry_descent_pixels', 'load_min_consecutive_frames', 'final_idle_frames')},
        second_natural_transition_validated=not errors,
        scope='One additional played pipe transition from castle sublevel 3 to 153 after the unchanged 65-action prefix.',
        limitations=['The original initial entry is synthetic.',
                     'Passing positions and health do not qualify the complete castle or other exits.',
                     'Android, expanded spawning and frame rate are outside this contract.'],
        **health)
