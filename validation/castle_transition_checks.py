"""Evidence gates for one played castle pipe transition within sublevel 3.

The caller must bind run-settings.asset_sha256.state to entry_state_sha256 and
require observation_profile='castle-status-v1'. The inherited entry fixture is
synthetic; this module qualifies only the later, input-driven pipe transition.
"""
from itertools import groupby

from castle_route_progress import check_castle_progress
from panorama_session_checks import action_digest, require, validate_trace


PROFILE = 'castle-natural-pipe-transition-v1'
ENTRY_STATE_SHA256 = '02f131a013d8bc2c352b05502ce0f420136e6736c8b4864cbfb51c4307de11f2'
PREFIX_SHA256 = 'af302618d4858bbd150047e1db7b6c637e0dd011cf8b4f990ef56eade64e7a37'
PREFIX = dict(action_count=37, frame_count=2238, actions_sha256=PREFIX_SHA256)
STATE_SEQUENCE = [13, 9, 10, 11, 13]
SCENE_FIELDS = ('level_id', 'sublevel_id', 'game_state', 'transformation')
BOUNDS = {
    'origin_bounds': (3992, 4016, 1872, 1912),
    'return_bounds': (352, 384, 1008, 1040),
    'destination_bounds': (352, 384, 1008, 1168),
    'final_bounds': (352, 384, 1136, 1168),
}
BOUND_FIELDS = ('x_min_pixels', 'x_max_pixels', 'y_min_pixels', 'y_max_pixels')
PHASE_LIMITS = ((9, 2, 120), (10, 2, 180), (11, 2, 180))
PREFIX_CONTRACT = dict(
    profile='castle-route-progress-v1', actions_sha256=PREFIX_SHA256,
    entry_state_sha256=ENTRY_STATE_SHA256, gameplay_start_frame=300,
    level_id=3, sublevel_id=3, final_idle_frames=300, final_min_x_pixels=2600,
    waypoints=[dict(id=name, x_min_pixels=left, x_max_pixels=right,
                    y_max_pixels=1825, min_consecutive_frames=5)
               for name, left, right in (('wall', 1930, 2096),
                                         ('post_elevator', 2224, 2360))])


def _digest(value):
    return (type(value) is str and len(value) == 64 and
            all(c in '0123456789abcdef' for c in value))


def validate_transition_contract(value):
    """Reject unknown fields, a different source, or weakened evidence gates."""
    fields = {'profile', 'actions_sha256', 'entry_state_sha256',
              'gameplay_start_frame', 'level_id', 'sublevel_id', 'prefix',
              'game_state_sequence', 'source_screen', 'exit_index',
              'exit_entry_type', 'return_entry_type', 'exit_min_y_pixels',
              'phase_limits', 'load_position_pixels', 'load_min_consecutive_frames',
              'final_idle_frames', *BOUNDS}
    require(type(value) is dict and set(value) == fields,
            'Invalid castle transition contract fields')
    require(value['profile'] == PROFILE, 'Unknown castle transition profile')
    require(_digest(value['actions_sha256']), 'Invalid transition actions digest')
    require(value['entry_state_sha256'] == ENTRY_STATE_SHA256,
            'Unexpected castle entry-state digest')
    fixed = dict(gameplay_start_frame=300, level_id=3, sublevel_id=3,
                 source_screen=127, exit_index=1016, exit_entry_type=1,
                 return_entry_type=0, exit_min_y_pixels=1904)
    for name, expected in fixed.items():
        require(type(value[name]) is int and value[name] == expected,
                'Unexpected fixed castle transition field: ' + name)
    prefix = value['prefix']
    require(type(prefix) is dict and set(prefix) == set(PREFIX) and
            type(prefix['action_count']) is int and type(prefix['frame_count']) is int and
            prefix == PREFIX, 'Do not change the reviewed castle route prefix')
    sequence = value['game_state_sequence']
    require(type(sequence) is list and all(type(s) is int for s in sequence) and
            sequence == STATE_SEQUENCE, 'Do not change the ordered transition phases')
    for name, geometry in BOUNDS.items():
        bound = value[name]
        require(type(bound) is dict and set(bound) == set(BOUND_FIELDS) and
                all(type(bound[k]) is int for k in BOUND_FIELDS) and
                tuple(bound[k] for k in BOUND_FIELDS) == geometry,
                'Do not change the reviewed castle transition geometry: ' + name)
    limits = value['phase_limits']
    require(type(limits) is list and len(limits) == 3,
            'Exactly three ordered transition phase limits required')
    for phase, (state, minimum, maximum) in zip(limits, PHASE_LIMITS):
        require(type(phase) is dict and set(phase) ==
                {'game_state', 'min_frames', 'max_frames'} and
                all(type(v) is int for v in phase.values()), 'Invalid transition phase limit')
        require(phase['game_state'] == state and
                minimum <= phase['min_frames'] <= phase['max_frames'] <= maximum,
                'Do not weaken or reorder the bounded transition phase limits')
    position = value['load_position_pixels']
    require(type(position) is list and len(position) == 2 and
            all(type(v) is int for v in position) and position == [368, 1024],
            'Unexpected castle pipe load position')
    require(type(value['load_min_consecutive_frames']) is int and
            2 <= value['load_min_consecutive_frames'] <= 180,
            'Do not weaken the consecutive load-position evidence')
    require(type(value['final_idle_frames']) is int and
            300 <= value['final_idle_frames'] <= 6000,
            'Do not weaken the final safe, stable idle requirement')


def _inside(row, bounds):
    x, y = row['player_xy_velocity'][:2]
    return (bounds['x_min_pixels'] <= x <= bounds['x_max_pixels'] and
            bounds['y_min_pixels'] <= y <= bounds['y_max_pixels'])


def _baby_safe(row):
    status = row['castle_status']
    return (status['baby_id'], status['baby_status']) == (97, 5)


def _down(row):
    # Active-low GBA input: Down must be held and its opposite, Up, released.
    keys = row['keys']
    return type(keys) is int and keys & 0x80 == 0 and keys & 0x40 != 0


def _runs(trace, fields):
    result = []
    for values, rows in groupby(trace, key=lambda row: tuple(row[k] for k in fields)):
        rows = list(rows)
        result.append(dict(zip(fields, values),
                           start_elapsed_frame=rows[0]['elapsed_frame'],
                           end_elapsed_frame=rows[-1]['elapsed_frame'],
                           observed_frames=len(rows)))
    return result


def _baby_losses(trace):
    """Record normal-gameplay detachments; loader placeholders are not damage."""
    episodes, current = [], None
    for row in trace:
        normal = row['game_state'] == 13 and row['transformation'] == 0
        if normal and not _baby_safe(row):
            if current is None:
                current = dict(start_elapsed_frame=row['elapsed_frame'],
                               end_elapsed_frame=row['elapsed_frame'],
                               observed_frames=0, recovery_elapsed_frame=None)
                episodes.append(current)
            current['end_elapsed_frame'] = row['elapsed_frame']
            current['observed_frames'] += 1
        elif current is not None:
            if normal and _baby_safe(row):
                current['recovery_elapsed_frame'] = row['elapsed_frame']
            current = None
    return episodes


def _health(trace, errors):
    life_drops, death_changes = [], []
    for previous, row in zip(trace, trace[1:]):
        before, after = previous['castle_status'], row['castle_status']
        if after['lives'] < before['lives']:
            life_drops.append(dict(elapsed_frame=row['elapsed_frame'],
                                   previous=before['lives'], current=after['lives'],
                                   decrease=before['lives'] - after['lives']))
        if after['deaths'] != before['deaths']:
            death_changes.append(dict(elapsed_frame=row['elapsed_frame'],
                                      previous=before['deaths'], current=after['deaths'],
                                      delta=after['deaths'] - before['deaths']))
    nonzero_deaths = [r['elapsed_frame'] for r in trace if r['castle_status']['deaths'] != 0]
    negative_lives = [r['elapsed_frame'] for r in trace if r['castle_status']['lives'] < 0]
    inhibition = [r['elapsed_frame'] for r in trace if r['castle_status']['inhibition'] != 0]
    if life_drops:
        errors.append('Observed life decrement(s) between consecutive frames')
    if negative_lives:
        errors.append('Lives must remain nonnegative on every frame')
    if nonzero_deaths or death_changes:
        errors.append('Death counter must remain zero without changes or resets')
    if inhibition:
        errors.append('Inhibition must remain zero on every frame')
    return dict(life_decrement_events=life_drops,
                total_lives_lost=sum(e['decrease'] for e in life_drops),
                negative_life_frames=negative_lives, death_counter_changes=death_changes,
                death_counter_nonzero_frames=nonzero_deaths,
                inhibition_nonzero_frames=inhibition)


def check_castle_transition(actions, session, trace, contract):
    """Return qualification and measurements; malformed/unbound evidence raises."""
    validate_transition_contract(contract)
    total = validate_trace(actions, session, trace)
    require(all('castle_status' in row for row in trace),
            'Castle transition requires per-frame castle-status observations')
    require(action_digest(actions) == contract['actions_sha256'],
            'Actions differ from the reviewed castle transition contract')
    count, prefix_frames = PREFIX['action_count'], PREFIX['frame_count']
    require(len(actions) > count and action_digest(actions[:count]) == PREFIX_SHA256 and
            sum(a['n'] for a in actions[:count]) == prefix_frames,
            'Actions do not preserve the reviewed castle route prefix')
    prefix = check_castle_progress(actions[:count], session[:count],
                                   trace[:prefix_frames + 1], PREFIX_CONTRACT)
    errors = []
    if not prefix['progress_pass']:
        errors.append('Inherited castle route prefix did not satisfy its progress contract')
    start = contract['gameplay_start_frame']
    entry_bad = [r['elapsed_frame'] for r in trace[:start]
                 if r['game_state'] not in (9, 10, 11, 12, 13)]
    scene_bad = [r['elapsed_frame'] for r in trace[start:]
                 if (r['level_id'], r['sublevel_id'], r['transformation']) != (3, 3, 0)]
    transformation_bad = [r['elapsed_frame'] for r in trace if r['transformation'] != 0]
    if entry_bad:
        errors.append('Unsupported death, restart or entry state during warm-up')
    if scene_bad:
        errors.append('Expected level 3/sublevel 3/normal Yoshi after warm-up')
    if transformation_bad:
        errors.append('Transformation must remain 0 throughout the route and loading')
    health = _health(trace, errors)
    baby_losses = _baby_losses(trace)
    if baby_losses:
        errors.append('Baby Mario must remain safely attached throughout normal gameplay, even if later recovered')
    transition_baby_bad = [r['elapsed_frame'] for r in trace[start:]
                           if (r['game_state'] == 9 and not _baby_safe(r)) or
                           (r['game_state'] in (10, 11) and
                            (r['castle_status']['baby_id'], r['castle_status']['baby_status'])
                            not in ((97, 5), (0, 0)))]
    if transition_baby_bad:
        errors.append('Baby Mario must be 97/5 during state 9; only 97/5 or loader 0/0 is allowed in states 10/11')

    runs = _runs(trace[start:], ('game_state',))
    sequence = [r['game_state'] for r in runs]
    phase_sequence_ok = sequence == STATE_SEQUENCE
    if not phase_sequence_ok:
        errors.append('Expected exactly one post-warm-up transition 13 -> 9 -> 10 -> 11 -> 13')
    departure = load_start = loaded = arrival = normal_return = None
    down_at_exit = origin_ok = exit_ok = entry_type_ok = load_ok = return_ok = False
    phases, unexpected_exit, unexpected_entry_type = [], [], []
    load_frames = 0
    if phase_sequence_ok:
        departure, load_start, arrival, normal_return = (
            runs[i]['start_elapsed_frame'] for i in (1, 2, 3, 4))
        if departure <= prefix_frames:
            errors.append('Pipe transition did not occur after the inherited route prefix')
        for run, limit in zip(runs[1:4], contract['phase_limits']):
            passed = limit['min_frames'] <= run['observed_frames'] <= limit['max_frames']
            phases.append(dict(run, min_frames=limit['min_frames'],
                               max_frames=limit['max_frames'], passed=passed))
            if not passed:
                errors.append('Transition phase duration is outside its reviewed limits: ' +
                              str(run['game_state']))
        before, first = trace[departure - 1], trace[departure]
        down_at_exit = _down(before) and _down(first)
        if not down_at_exit:
            errors.append('Effective Down input is required immediately before and at pipe exit')
        before_xy, first_xy = before['player_xy_velocity'], first['player_xy_velocity']
        origin_ok = (_inside(before, contract['origin_bounds']) and
                     _inside(first, contract['origin_bounds']) and
                     first_xy[1] >= contract['exit_min_y_pixels'] and
                     before_xy[3] >= 0 and first_xy[3] > 0 and
                     abs(first_xy[0] - before_xy[0]) <= 16 and
                     0 < first_xy[1] - before_xy[1] <= 16)
        if not origin_ok:
            errors.append('Missing continuous downward entry at the reviewed pipe mouth')
        for row in trace[start:]:
            elapsed, status = row['elapsed_frame'], row['castle_status']
            expected_exit = 0 if elapsed < departure else contract['exit_index']
            expected_entry = (contract['exit_entry_type'] if departure <= elapsed < arrival
                              else contract['return_entry_type'])
            if status['exit_index'] != expected_exit:
                unexpected_exit.append(elapsed)
            if status['entry_type'] != expected_entry:
                unexpected_entry_type.append(elapsed)
        exit_ok, entry_type_ok = not unexpected_exit, not unexpected_entry_type
        if not exit_ok:
            errors.append('Exit index must change from 0 to 1016 at state 9 and persist to the end')
        if not entry_type_ok:
            errors.append('Entry type must be 1 during states 9/10 and 0 before/after them')
        if any(r['player_xy_velocity'] != first_xy for r in trace[departure:load_start]):
            errors.append('Departure position/velocity changed during the exit fade')

        expected_load = [*contract['load_position_pixels'], 0, 0]
        loaded = next((r['elapsed_frame'] for r in trace[load_start:arrival]
                       if r['player_xy_velocity'] == expected_load), None)
        if loaded is not None:
            load_frames = arrival - loaded
            load_ok = (load_frames >= contract['load_min_consecutive_frames'] and
                       all(r['player_xy_velocity'] == first_xy
                           for r in trace[load_start:loaded]) and
                       all(r['player_xy_velocity'] == expected_load
                           for r in trace[loaded:normal_return]))
        if not load_ok:
            errors.append('Missing sustained state-10 load at (368, 1024) through state 11')
        return_ok = (_inside(trace[normal_return], contract['return_bounds']) and
                     all(_inside(r, contract['destination_bounds'])
                         for r in trace[normal_return:]))
        if not return_ok:
            errors.append('Normal gameplay did not resume and remain in the destination area')

    final = trace[-1]
    final_xy = final['player_xy_velocity'][:2]
    idle = 0
    for row in reversed(trace[1:]):
        if (normal_return is None or row['elapsed_frame'] < normal_return or
                tuple(row[k] for k in SCENE_FIELDS) != (3, 3, 13, 0) or
                row['keys'] != 1023 or row['player_xy_velocity'][:2] != final_xy or
                row['player_xy_velocity'][2:] != [0, 0] or not _baby_safe(row) or
                row['castle_status']['entry_type'] != 0 or
                row['castle_status']['exit_index'] != 1016):
            break
        idle += 1
    final_zone_ok = _inside(final, contract['final_bounds'])
    if not final_zone_ok:
        errors.append('Final position is outside the reviewed destination landing area')
    if not _baby_safe(final):
        errors.append('Baby Mario is not safely attached at the final frame')
    if idle < contract['final_idle_frames']:
        errors.append('Insufficient final destination idle with fixed position, zero velocity and Baby safe')
    statuses = [r['castle_status'] for r in trace]
    coordinates = [r['player_xy_velocity'] for r in trace]
    loading_empty = [r['elapsed_frame'] for r in trace
                     if r['game_state'] in (10, 11) and
                     (r['castle_status']['baby_id'], r['castle_status']['baby_status']) == (0, 0)]
    return dict(
        profile=PROFILE, transition_pass=not errors, transition_errors=errors,
        actions_sha256=action_digest(actions), entry_state_sha256=contract['entry_state_sha256'],
        frames=total, observation_interval_frames=1,
        prefix_progress_pass=prefix['progress_pass'], prefix_progress_errors=prefix['progress_errors'],
        prefix_actions_sha256=PREFIX_SHA256, prefix_frames=prefix_frames,
        prefix_waypoints=prefix['waypoints'],
        scene_sequence=_runs(trace, SCENE_FIELDS),
        game_state_sequence=[r['game_state'] for r in _runs(trace, ('game_state',))],
        post_warmup_game_state_sequence=sequence,
        transformation_sequence=[k for k, _ in groupby(r['transformation'] for r in trace)],
        level_ids=sorted({r['level_id'] for r in trace}),
        sublevel_ids=sorted({r['sublevel_id'] for r in trace}),
        unexpected_entry_frames=entry_bad, unexpected_scene_frames=scene_bad,
        unexpected_transformation_frames=transformation_bad,
        transition_count=sum(a['game_state'] == 13 and b['game_state'] == 9
                             for a, b in zip(trace[start:], trace[start + 1:])),
        phase_sequence_pass=phase_sequence_ok, phase_measurements=phases,
        departure_elapsed_frame=departure, loading_start_elapsed_frame=load_start,
        load_position_elapsed_frame=loaded, load_position_frames_in_state10=load_frames,
        arrival_elapsed_frame=arrival, normal_return_elapsed_frame=normal_return,
        effective_down_at_exit=down_at_exit, origin_evidence_pass=origin_ok,
        exit_index_pass=exit_ok, unexpected_exit_index_frames=unexpected_exit,
        entry_type_pass=entry_type_ok, unexpected_entry_type_frames=unexpected_entry_type,
        load_position_pass=load_ok, destination_return_pass=return_ok,
        final_position_pass=final_zone_ok, stable_normal_idle_frames=idle,
        final_player_xy_velocity=final['player_xy_velocity'],
        final_game_state=final['game_state'], final_transformation=final['transformation'],
        initial_lives=statuses[0]['lives'], final_lives=statuses[-1]['lives'],
        final_baby_safe=_baby_safe(final), baby_loss_episodes=baby_losses,
        unexpected_transition_baby_frames=transition_baby_bad,
        baby_health_pass=not baby_losses and not transition_baby_bad,
        loader_baby_empty_frames=loading_empty,
        observed_limits=dict(x_min_pixels=min(x[0] for x in coordinates),
                             x_max_pixels=max(x[0] for x in coordinates),
                             y_min_pixels=min(x[1] for x in coordinates),
                             y_max_pixels=max(x[1] for x in coordinates),
                             lives_min=min(s['lives'] for s in statuses),
                             lives_max=max(s['lives'] for s in statuses),
                             deaths_min=min(s['deaths'] for s in statuses),
                             deaths_max=max(s['deaths'] for s in statuses)),
        reviewed_limits={name: contract[name] for name in
                         (*BOUNDS, 'phase_limits', 'final_idle_frames',
                          'load_min_consecutive_frames')},
        natural_transition_validated=not errors,
        scope='Evaluation of one input-driven pipe-transition attempt from screen 127 to a different location in level 3/sublevel 3.',
        limitations=['The inherited initial entry is synthetic; only a passing result qualifies the later pipe transition.',
                     'Castle completion, boss behavior and Android qualification are outside this contract.'],
        **health)
