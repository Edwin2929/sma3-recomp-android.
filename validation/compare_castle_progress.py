#!/usr/bin/env python3
"""Qualify a bounded castle route from private paired sessions; emit aggregates only."""
import argparse
import hashlib
import json
import struct
from pathlib import Path

from castle_route_progress import check_castle_progress, validate_castle_contract
from compare_panorama_sessions import compare, read_json
from panorama_session_checks import require


def compare_castle(control, candidate, contract):
    validate_castle_contract(contract)
    for folder in (control, candidate):
        settings = read_json(folder / 'run-settings.json')
        require(settings.get('observation_profile') == 'castle-status-v1',
                'Castle progress requires the per-frame health observation profile')
        require(settings['asset_sha256']['state'] == contract['entry_state_sha256'],
                'Castle session did not start from the reviewed entry fixture')
        require(settings['capture_start'] == 0,
                'Castle evidence must include the initial entry transitions')
    # This verifies both traces, every scheduled center and serialized state,
    # native completion, matching assets and matching input plans.
    result = compare(control, candidate)
    actions = read_json(control / 'actions.json')
    session = read_json(control / 'session.json')
    trace = read_json(control / 'trace.json')
    progress = check_castle_progress(actions, session, trace, contract)
    result.update(route_contract=contract, route_checks=progress,
                  complete_castle_progress_pass=(progress['progress_pass'] and
                      result['differential_pass'] and result['coverage_pass'] and
                      result['raw_gap_frames'] == 0),
                  input_provenance=dict(asset_sha256=settings['asset_sha256'],
                      observation_profile=settings['observation_profile']),
                  evidence_sha256={name: hashlib.sha256((control / name).read_bytes()).hexdigest()
                      for name in ('actions.json', 'trace.json', 'captures.json')},
                  natural_room_transition_qualified=False,
                  scope='Spatial progress through the first castle wall and moving platform, '
                        'then a stable stop in the lower corridor of sublevel 3. '
                        'Does not qualify a natural room transition, the next wall, '
                        'castle completion, expanded spawning, Android or frame rate.')
    return result


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
        result = compare_castle(args.control, args.candidate, read_json(args.contract))
        passed = result['complete_castle_progress_pass']
    except (OSError, ValueError, KeyError, TypeError, IndexError, struct.error) as error:
        result = dict(status='failed', error=str(error), differential_pass=False,
                      complete_castle_progress_pass=False)
        passed = False
    with args.output.open('x') as output:
        output.write(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))
    if not passed:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
