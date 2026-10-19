#!/usr/bin/env python3
"""Qualify one natural castle pipe transition from private paired sessions."""
import argparse
import hashlib
import json
import struct
from pathlib import Path

from castle_route_checks import CASTLE_STATUS_PROFILE
from castle_transition_checks import check_castle_transition, validate_transition_contract
from compare_panorama_sessions import compare, read_json
from panorama_session_checks import require


SCOPE = ('Evaluation of one input-driven pipe-transition attempt from the lower '
         'to the upper corridor of level 3/sublevel 3, replayed from the reviewed '
         'synthetic entry fixture at 240 and 356 pixels wide. Only a passing '
         'complete_castle_transition_pass qualifies that transition. Does not '
         'qualify the complete castle, other exits, expanded spawning, Android, '
         'an APK or frame rate.')
ASSET_NAMES = {'rom', 'bios', 'state', 'runner', 'config'}
EVIDENCE_NAMES = ('actions.json', 'trace.json', 'captures.json')


def compare_castle_transition(control, candidate, contract):
    """Keep transition success separate from complete paired qualification."""
    validate_transition_contract(contract)
    settings = []
    for folder in (control, candidate):
        value = read_json(folder / 'run-settings.json')
        require(type(value) is dict, 'Invalid castle session settings')
        require(value.get('observation_profile') == CASTLE_STATUS_PROFILE,
                'Castle transition requires the per-frame health observation profile')
        assets = value.get('asset_sha256')
        require(type(assets) is dict and set(assets) == ASSET_NAMES and
                all(type(digest) is str and len(digest) == 64 and
                    all(c in '0123456789abcdef' for c in digest)
                    for digest in assets.values()),
                'Castle transition requires all five input provenance SHA-256 digests')
        require(assets['state'] == contract['entry_state_sha256'],
                'Castle session did not start from the reviewed entry fixture')
        require(type(value.get('capture_start')) is int and value['capture_start'] == 0,
                'Castle evidence must include the initial entry transitions')
        settings.append(value)

    # The shared comparator binds both complete traces, all scheduled captures
    # and saved states, widths, inputs, assets, builds and native diagnostics.
    # A common trajectory alone is not sufficient to qualify the pipe exit.
    result = compare(control, candidate)
    actions = read_json(control / 'actions.json')
    session = read_json(control / 'session.json')
    trace = read_json(control / 'trace.json')
    transition = check_castle_transition(actions, session, trace, contract)
    passed = (transition['transition_pass'] is True and
              transition['transition_errors'] == [] and
              result['differential_pass'] is True and result['coverage_pass'] is True and
              type(result['raw_gap_frames']) is int and result['raw_gap_frames'] == 0)
    result = dict(result)
    result.update(transition_contract=contract, transition_checks=transition,
                  complete_castle_transition_pass=passed,
                  natural_room_transition_qualified=passed,
                  input_provenance=dict(asset_sha256=settings[0]['asset_sha256'],
                      observation_profile=settings[0]['observation_profile']),
                  # JSON equivalence on both sides is checked above. These
                  # hashes identify the control-side evidence files only.
                  evidence_sha256={name: hashlib.sha256((control / name).read_bytes()).hexdigest()
                      for name in EVIDENCE_NAMES},
                  scope=SCOPE)
    return result


def _failure(error):
    # Filesystem exception strings contain private asset/session paths. The
    # public failure report identifies the failed class without copying them.
    if isinstance(error, OSError):
        message = 'Required session or contract evidence could not be read'
    elif isinstance(error, json.JSONDecodeError):
        message = 'Invalid JSON in session or contract evidence'
    elif isinstance(error, (KeyError, TypeError, IndexError, struct.error)):
        message = 'Incomplete or malformed session or contract evidence'
    else:
        message = str(error)
    return dict(status='failed', error=message, error_type=type(error).__name__,
                differential_pass=False, coverage_pass=False,
                complete_castle_transition_pass=False,
                natural_room_transition_qualified=False, scope=SCOPE)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('control', type=Path)
    p.add_argument('candidate', type=Path)
    p.add_argument('output', type=Path)
    p.add_argument('--contract', type=Path, required=True)
    args = p.parse_args()
    if args.output.exists():
        p.error('Use a new report path; historical evidence must not be overwritten')
    try:
        result = compare_castle_transition(args.control, args.candidate, read_json(args.contract))
        serialized = json.dumps(result, indent=2, allow_nan=False) + '\n'
    except (OSError, ValueError, KeyError, TypeError, IndexError, struct.error) as error:
        result = _failure(error)
        serialized = json.dumps(result, indent=2, allow_nan=False) + '\n'
    try:
        with args.output.open('x') as output:
            output.write(serialized)
    except FileExistsError:
        p.error('Use a new report path; historical evidence must not be overwritten')
    print(json.dumps(result, allow_nan=False))
    if not result['complete_castle_transition_pass']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
