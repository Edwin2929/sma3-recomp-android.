"""Bounded castle-route progress checks; not a natural-transition certificate.

The caller must also bind run-settings.asset_sha256.state to entry_state_sha256
and require observation_profile='castle-status-v1'. Input/frame consistency and
castle-status decoding are shared with the existing session validator.
"""
from itertools import groupby

from panorama_session_checks import action_digest, require, validate_trace


PROFILE = 'castle-route-progress-v1'
ENTRY_STATE_SHA256 = '02f131a013d8bc2c352b05502ce0f420136e6736c8b4864cbfb51c4307de11f2'
WAYPOINT_GEOMETRY = (
    ('wall', 1930, 2096, 1825),
    ('post_elevator', 2224, 2360, 1825),
)
SCENE_FIELDS = ('level_id', 'sublevel_id', 'game_state', 'transformation')


def validate_castle_contract(value):
    """Reject unknown knobs, a different fixture, or weakened requirements."""
    fields = {'profile', 'actions_sha256', 'entry_state_sha256',
              'gameplay_start_frame', 'level_id', 'sublevel_id',
              'final_idle_frames', 'final_min_x_pixels', 'waypoints'}
    require(type(value) is dict and set(value) == fields,
            'Invalid castle progress contract fields')
    require(value['profile'] == PROFILE, 'Unknown castle progress profile')
    digest = value['actions_sha256']
    require(type(digest) is str and len(digest) == 64 and
            all(c in '0123456789abcdef' for c in digest), 'Invalid actions digest')
    require(value['entry_state_sha256'] == ENTRY_STATE_SHA256,
            'Unexpected castle entry-state digest')
    for name in ('gameplay_start_frame', 'level_id', 'sublevel_id',
                 'final_idle_frames', 'final_min_x_pixels'):
        require(type(value[name]) is int, 'Castle contract values must be integers')
    require((value['gameplay_start_frame'], value['level_id'], value['sublevel_id']) ==
            (300, 3, 3), 'Unexpected entry contract for this castle fixture')
    require(300 <= value['final_idle_frames'] <= 6000 and
            2600 <= value['final_min_x_pixels'] <= 8388607,
            'Do not weaken the final progress or stable-idle requirements')
    waypoints = value['waypoints']
    require(type(waypoints) is list and len(waypoints) == 2,
            'Exactly two ordered castle waypoints required')
    waypoint_fields = {'id', 'x_min_pixels', 'x_max_pixels', 'y_max_pixels',
                       'min_consecutive_frames'}
    for point, geometry in zip(waypoints, WAYPOINT_GEOMETRY):
        require(type(point) is dict and set(point) == waypoint_fields,
                'Invalid castle waypoint fields')
        require(all(type(point[name]) is int for name in waypoint_fields - {'id'}),
                'Castle waypoint values must be integers')
        require(tuple(point[name] for name in ('id', 'x_min_pixels', 'x_max_pixels',
                                               'y_max_pixels')) == geometry,
                'Do not change the reviewed castle waypoint geometry or order')
        require(5 <= point['min_consecutive_frames'] <= 6000,
                'Do not weaken the consecutive waypoint-frame requirement')


def _gameplay(row):
    return tuple(row[name] for name in SCENE_FIELDS) == (3, 3, 13, 0)


def _baby_safe(row):
    status = row['castle_status']
    return (status['baby_id'], status['baby_status']) == (97, 5)


def _waypoint(trace, point, start):
    longest, run_start, run_length, first = 0, None, 0, None
    for row in trace[start:]:
        x, y = row['player_xy_velocity'][:2]
        inside = (_gameplay(row) and point['x_min_pixels'] <= x <= point['x_max_pixels']
                  and y <= point['y_max_pixels'])
        if not inside:
            run_start, run_length = None, 0
            continue
        if run_start is None:
            run_start = row['elapsed_frame']
        run_length += 1
        longest = max(longest, run_length)
        if first is None and run_length >= point['min_consecutive_frames']:
            first = (run_start, row['elapsed_frame'])
    return dict(point, passed=first is not None, longest_consecutive_frames=longest,
                first_start_elapsed_frame=None if first is None else first[0],
                first_completed_elapsed_frame=None if first is None else first[1])


def _scene_sequence(trace):
    sequence = []
    for values, rows in groupby(trace, key=lambda row: tuple(row[k] for k in SCENE_FIELDS)):
        run = list(rows)
        sequence.append(dict(zip(SCENE_FIELDS, values),
                             start_elapsed_frame=run[0]['elapsed_frame'],
                             end_elapsed_frame=run[-1]['elapsed_frame'],
                             observed_frames=len(run)))
    return sequence


def _baby_losses(trace):
    """Record normal-gameplay detachments, including recovered damage."""
    episodes, current = [], None
    for row in trace:
        normal = row['game_state'] == 13 and row['transformation'] == 0
        unsafe = normal and not _baby_safe(row)
        if unsafe:
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


def check_castle_progress(actions, session, trace, contract):
    """Return measurements and errors; malformed or unbound evidence raises.

    Geometry uses the unrounded pixel coordinates. A one-frame death, signed
    life decrement, temporary counter change, or return during gameplay cannot
    be hidden by a later gain, recovery, or action endpoint.
    """
    validate_castle_contract(contract)
    total = validate_trace(actions, session, trace)
    require(all('castle_status' in row for row in trace),
            'Castle progress requires per-frame castle-status observations')
    require(action_digest(actions) == contract['actions_sha256'],
            'Actions differ from the reviewed castle progress contract')
    start = contract['gameplay_start_frame']
    final = trace[-1]
    errors = []

    # Entry is synthetic. States 9..13 may initialize it, but no death/restart
    # state is accepted even during warm-up. All later frames must be gameplay.
    unexpected_entry = [r['elapsed_frame'] for r in trace[:start]
                        if r['game_state'] not in (9, 10, 11, 12, 13)]
    unexpected_gameplay = [r['elapsed_frame'] for r in trace[start:] if not _gameplay(r)]
    supported = len(trace) > start and not unexpected_gameplay
    if unexpected_entry:
        errors.append('Unexpected death, restart or unsupported entry state at frame ' +
                      str(unexpected_entry[0]))
    if not supported:
        errors.append('Expected level 3/sublevel 3/state 13/normal Yoshi after frame 300')
    if any(r['transformation'] != 0 for r in trace):
        errors.append('Expected normal transformation 0 throughout the castle route')

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
    death_nonzero = [r['elapsed_frame'] for r in trace if r['castle_status']['deaths'] != 0]
    negative_lives = [r['elapsed_frame'] for r in trace if r['castle_status']['lives'] < 0]
    inhibition_nonzero = [r['elapsed_frame'] for r in trace
                          if r['castle_status']['inhibition'] != 0]
    if life_drops:
        errors.append('Observed life decrement(s) between consecutive frames')
    if negative_lives:
        errors.append('Lives must remain nonnegative on every frame')
    if death_changes or death_nonzero:
        errors.append('Death counter must remain zero on every frame without changes or resets')
    if inhibition_nonzero:
        errors.append('Inhibition must remain zero on every frame')

    waypoints, next_start, ordered = [], start, True
    for point in contract['waypoints']:
        measured = _waypoint(trace, point, next_start)
        measured['passed'] = measured['passed'] and ordered
        waypoints.append(measured)
        if measured['passed']:
            next_start = measured['first_completed_elapsed_frame'] + 1
        else:
            ordered = False
            errors.append('Missing ordered consecutive-frame waypoint: ' + point['id'])

    final_xy = final['player_xy_velocity'][:2]
    idle = 0
    for row in reversed(trace[1:]):
        if (row['elapsed_frame'] < start or not _gameplay(row) or row['keys'] != 1023 or
                row['player_xy_velocity'][:2] != final_xy or
                row['player_xy_velocity'][2:] != [0, 0] or not _baby_safe(row)):
            break
        idle += 1
    if final_xy[0] < contract['final_min_x_pixels']:
        errors.append('Final unrounded X is below the reviewed progress threshold')
    if not _baby_safe(final):
        errors.append('Baby Mario is not safely attached at the final frame')
    if idle < contract['final_idle_frames']:
        errors.append('Insufficient final idle frames with fixed position, zero velocity and Baby Mario safe')

    statuses = [r['castle_status'] for r in trace]
    coordinates = [r['player_xy_velocity'] for r in trace]
    return dict(
        profile=PROFILE, progress_pass=not errors, progress_errors=errors,
        actions_sha256=action_digest(actions), entry_state_sha256=contract['entry_state_sha256'],
        frames=total, observation_interval_frames=1,
        scene_sequence=_scene_sequence(trace),
        game_state_sequence=[key for key, _ in groupby(r['game_state'] for r in trace)],
        transformation_sequence=[key for key, _ in groupby(r['transformation'] for r in trace)],
        level_ids=sorted({r['level_id'] for r in trace}),
        sublevel_ids=sorted({r['sublevel_id'] for r in trace}),
        supported_gameplay_only=supported, unexpected_entry_frames=unexpected_entry,
        unexpected_post_warmup_frames=unexpected_gameplay,
        waypoints=waypoints, final_player_xy_velocity=final['player_xy_velocity'],
        final_game_state=final['game_state'], final_transformation=final['transformation'],
        stable_normal_idle_frames=idle, final_baby_safe=_baby_safe(final),
        initial_lives=statuses[0]['lives'], final_lives=statuses[-1]['lives'],
        life_decrement_events=life_drops, total_lives_lost=sum(e['decrease'] for e in life_drops),
        negative_life_frames=negative_lives,
        death_counter_changes=death_changes, death_counter_nonzero_frames=death_nonzero,
        inhibition_nonzero_frames=inhibition_nonzero,
        baby_loss_episodes=_baby_losses(trace),
        observed_limits=dict(x_min_pixels=min(x[0] for x in coordinates),
                             x_max_pixels=max(x[0] for x in coordinates),
                             y_min_pixels=min(x[1] for x in coordinates),
                             y_max_pixels=max(x[1] for x in coordinates),
                             lives_min=min(s['lives'] for s in statuses),
                             lives_max=max(s['lives'] for s in statuses),
                             deaths_min=min(s['deaths'] for s in statuses),
                             deaths_max=max(s['deaths'] for s in statuses)),
        natural_transition_validated=False,
        scope='Spatial progress and continuity within level 3/sublevel 3 from a synthetic entry.',
        limitations=['The route ends in the lower corridor; the next wall near X=2720 remains beyond this route.',
                     'No natural room transition, castle completion, boss, or Android qualification is established.'])
