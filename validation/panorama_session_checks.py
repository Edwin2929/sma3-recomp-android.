"""Fail-closed input/trace checks. No ROM, BIOS or native runner is required."""
import hashlib
import json
import math
from itertools import groupby

OBSERVABLES = ('frame', 'player_xy_velocity', 'level_id', 'sublevel_id',
               'game_state', 'transformation')


def require(condition, message):
    # Do not use assert: validation must also run under python -O.
    if not condition:
        raise ValueError(message)


def action_digest(actions):
    encoded = json.dumps(actions, sort_keys=True, separators=(',', ':')).encode()
    return hashlib.sha256(encoded).hexdigest()


def validate_actions(actions):
    require(type(actions) is list and 1 <= len(actions) <= 200,
            '1..200 input actions required')
    for action in actions:
        require(type(action) is dict, 'Each action must be an object')
        conditional = 'until_transformation' in action
        fields = {'n', 'keys'} | ({'from_transformation', 'until_transformation'}
                                  if conditional else set())
        require(set(action) == fields, 'Invalid action fields')
        limit = 6000 if conditional else 600
        require(type(action['n']) is int and 1 <= action['n'] <= limit,
                'Invalid action frame budget')
        require(type(action['keys']) is int and 0 <= action['keys'] <= 1023,
                'Invalid active-low key mask')
        if conditional:
            require(all(type(action[k]) is int and 0 <= action[k] <= 65535
                        for k in ('from_transformation', 'until_transformation')),
                    'Invalid transformation condition')
            require(action['from_transformation'] != action['until_transformation'],
                    'A conditional action must observe a change')
    require(sum(x['n'] for x in actions) <= 6000, 'Maximum 6000 frames per session')


def validate_trace(actions, session, trace):
    """Bind every observation to the actual inputs and all action endpoints."""
    validate_actions(actions)
    require(len(session) == len(actions), 'Missing action checkpoints')
    total = sum(row['action']['n'] for row in session)
    require(1 <= total <= 6000 and len(trace) == total + 1,
            'Missing per-frame observations (including the initial state)')
    for i, row in enumerate(trace):
        require(type(row['elapsed_frame']) is int and row['elapsed_frame'] == i and type(row['frame']) is int,
                'Non-contiguous observation trace')
        require(row['frame'] == trace[0]['frame'] + i, 'Frame counter reset or gap')
        require(all(type(row[k]) is int for k in
                    ('level_id', 'sublevel_id', 'game_state', 'transformation')),
                'Invalid observed game state')
        xy = row['player_xy_velocity']
        require(type(xy) is list and len(xy) == 4 and
                all(type(v) in (int, float) and math.isfinite(v) for v in xy),
                'Invalid observed player position/velocity')
    require(trace[0]['action_index'] == -1 and trace[0]['keys'] is None,
            'Missing initial observation')
    cursor = 0
    for index, (planned, end) in enumerate(zip(actions, session)):
        actual = end['action']
        require(set(actual) == {'n', 'keys'} and type(actual['n']) is int and
                1 <= actual['n'] <= planned['n'] and type(actual['keys']) is int and
                actual['keys'] == planned['keys'],
                'Executed input differs from plan')
        rows = trace[cursor + 1:cursor + actual['n'] + 1]
        require(all(r['action_index'] == index and r['keys'] == planned['keys']
                    for r in rows), 'Per-frame input differs from checkpoint')
        if 'until_transformation' in planned:
            require(trace[cursor]['transformation'] == planned['from_transformation'],
                    'Conditional action did not start in the required mode')
            target = planned['until_transformation']
            require(rows[-1]['transformation'] == target and
                    all(r['transformation'] != target for r in rows[:-1]),
                    'Conditional action missed its first target frame or timed out')
        else:
            require(actual['n'] == planned['n'], 'Truncated fixed-duration action')
        cursor += actual['n']
        require(end['elapsed_frame'] == cursor and
                all(end[k] == trace[cursor][k] for k in OBSERVABLES),
                'Action checkpoint differs from frame trace')
    return total


def required_capture_frames(session, trace, start=0, interval=60):
    endpoints = {r['elapsed_frame'] for r in session}
    return {i for i in range(max(1, start), len(trace))
            if i % interval == 0 or i in endpoints or
            trace[i]['transformation'] != trace[i - 1]['transformation']}


def validate_expectations(value):
    require(type(value) is dict and set(value) == {
        'profile', 'actions_sha256', 'gameplay_start_frame', 'level_id',
        'sublevel_id', 'final_idle_frames', 'escape_min_dx_pixels'}, 'Invalid cycle contract fields')
    require(value['profile'] == 'car-complete-cycle-v1', 'Unknown cycle profile')
    require(type(value['actions_sha256']) is str and len(value['actions_sha256']) == 64 and
            all(c in '0123456789abcdef' for c in value['actions_sha256']), 'Invalid actions digest')
    for name in ('gameplay_start_frame', 'level_id', 'sublevel_id', 'final_idle_frames', 'escape_min_dx_pixels'):
        require(type(value[name]) is int, 'Cycle contract values must be integers')
    require((value['gameplay_start_frame'], value['level_id'], value['sublevel_id']) == (600, 18, 119),
            'Unexpected entry contract for this car fixture')
    require(1200 <= value['final_idle_frames'] <= 6000 and 32 <= value['escape_min_dx_pixels'] <= 65535,
            'Do not weaken the stable-normal or escape-distance requirements')



def check_car_cycle(actions, session, trace, expectations):
    """Return measured results; never infer a completed cycle from old samples."""
    validate_trace(actions, session, trace)
    validate_expectations(expectations)
    require(action_digest(actions) == expectations['actions_sha256'],
            'Actions differ from the reviewed cycle contract')
    sequence = [mode for mode, _ in groupby(r['transformation'] for r in trace)]
    final = trace[-1]
    idle = 0
    for row in reversed(trace[1:]):
        if (row['transformation'], row['game_state'], row['keys']) != (0, 13, 1023):
            break
        idle += 1
    recoveries = [i for i in range(1, len(trace))
                  if trace[i - 1]['transformation'] == 2 and trace[i]['transformation'] == 0]
    recovery = recoveries[0] if recoveries else None
    dx = None if recovery is None else (
        final['player_xy_velocity'][0] - trace[recovery]['player_xy_velocity'][0])
    start = expectations['gameplay_start_frame']
    supported = len(trace) > start and all(
        (r['game_state'], r['level_id'], r['sublevel_id']) ==
        (13, expectations['level_id'], expectations['sublevel_id']) for r in trace[start:])
    errors = []
    if sequence != [0, 2, 0]:
        errors.append('Expected exactly 0 -> 2 -> 0; observed ' + str(sequence))
    if final['transformation'] != 0:
        errors.append('Final transformation is not normal Yoshi')
    if not supported:
        errors.append('Unexpected gameplay state, level or sublevel after entry warm-up')
    if idle < expectations['final_idle_frames']:
        errors.append('Insufficient consecutive normal, idle gameplay frames at the end')
    if dx is None or dx < expectations['escape_min_dx_pixels']:
        errors.append('Insufficient measured rightward separation from the recovery point')
    return dict(cycle_pass=not errors, cycle_errors=errors,
                transformation_sequence=sequence, final_transformation=final['transformation'],
                final_game_state=final['game_state'], stable_normal_idle_frames=idle,
                recovery_elapsed_frame=recovery, final_dx_from_recovery=dx,
                supported_gameplay_only=supported, observation_interval_frames=1)
