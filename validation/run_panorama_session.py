#!/usr/bin/env python3
"""Bounded native input session from a private savestate; never publish its output."""
import argparse
import hashlib
import json
import os
import socket
import struct
import subprocess
import time
from pathlib import Path
from PIL import Image
from panorama_session_checks import (action_digest, check_car_cycle, require,
                                     validate_actions, validate_expectations)

ROOT = Path(__file__).resolve().parents[1]
RAM_START, RAM_END = 0x4cb8, 0x6db4


def observe(command):
    raw = bytes.fromhex(command(cmd='read_iwram', addr=0x03000000 + RAM_START,
                                len=RAM_END - RAM_START)['data'])
    require(len(raw) == RAM_END - RAM_START, 'Truncated IWRAM observation')
    return dict(player_xy_velocity=[v / 256 for v in struct.unpack_from('<iiii', raw, 0x6d80 - RAM_START)],
                level_id=struct.unpack_from('<H', raw, 0x6288 - RAM_START)[0],
                sublevel_id=struct.unpack_from('<H', raw, 0x4cb8 - RAM_START)[0],
                game_state=raw[0x6b05 - RAM_START],
                transformation=struct.unpack_from('<H', raw, 0x6db2 - RAM_START)[0])


def execute_session(command, actions, capture, evidence, trace_enabled=False, capture_start=0):
    """Conditional waits stop on the FIRST target frame, never on a 60-frame sample."""
    validate_actions(actions)
    trace_enabled = trace_enabled or any('until_transformation' in a for a in actions)
    session, trace, shots = evidence['session'], evidence['trace'], evidence['captures']
    current = observe(command)
    elapsed = 0
    if trace_enabled:
        trace.append(dict(current, frame=None, elapsed_frame=0, action_index=-1, keys=None))
    for index, action in enumerate(actions):
        conditional = 'until_transformation' in action
        if conditional:
            require(current['transformation'] == action['from_transformation'],
                    f'Action {index}: expected starting transformation {action["from_transformation"]}')
        executed = 0
        while executed < action['n']:
            n = 1 if trace_enabled else action['n']
            previous = current['transformation']
            reply = command(cmd='run_frames', n=n, keyinput=action['keys'])
            executed += n
            elapsed += n
            current = dict(observe(command), frame=reply['frame'])
            if trace_enabled:
                if trace[0]['frame'] is None:
                    trace[0]['frame'] = reply['frame'] - 1
                require(reply['frame'] == trace[0]['frame'] + elapsed, 'Frame counter reset or gap')
                trace.append(dict(current, elapsed_frame=elapsed, action_index=index, keys=action['keys']))
            reached = conditional and current['transformation'] == action['until_transformation']
            endpoint = reached or executed == action['n']
            if trace_enabled:
                if elapsed >= capture_start and (elapsed % 60 == 0 or endpoint or
                                                previous != current['transformation']):
                    shots.append(capture(f'frame-{elapsed:05}', current, elapsed, True))
            elif endpoint:
                capture(f'step-{index:03}', current, elapsed, False)
            if endpoint:
                row = dict(current, action={'n': executed, 'keys': action['keys']})
                if trace_enabled:
                    row['elapsed_frame'] = elapsed
                session.append(row)
                break
        if conditional:
            require(reached, f'Action {index}: transformation wait timed out after {executed} frames')
    return trace_enabled


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('rom', 'bios', 'state', 'actions', 'output'):
        p.add_argument('--' + name, type=Path, required=True)
    p.add_argument('--width', type=int, default=356)
    p.add_argument('--trace-every-frame', action='store_true')
    p.add_argument('--expectations', type=Path, help='Cycle contract; also enables per-frame tracing')
    a = p.parse_args()
    if not 240 <= a.width <= 356:
        p.error('width must be 240..356')
    try:
        actions = json.loads(a.actions.read_text())
        validate_actions(actions)
        expectations = json.loads(a.expectations.read_text()) if a.expectations else None
        if expectations is not None:
            validate_expectations(expectations)
            require(action_digest(actions) == expectations['actions_sha256'],
                    'Actions differ from the reviewed cycle contract')
        binary, config = ROOT / 'build/sma3_runner', ROOT / 'runtime.toml'
        for path in (a.rom, a.bios, a.state, binary, config):
            require(path.is_file(), f'Missing required private asset or native build: {path}')
        for path, sha in ((a.rom, '7352d2bd064d9ebaec579e264228aa21c7345b80'),
                          (a.bios, '300c20df6731a33952ded8c436f7f186d25d3492')):
            require(hashlib.sha1(path.read_bytes()).hexdigest() == sha, 'Unsupported asset')
    except (OSError, ValueError) as error:
        p.error(str(error))
    a.output = a.output.resolve()
    a.output.mkdir(parents=True, exist_ok=True)
    if any(a.output.iterdir()):
        p.error('Use a new empty output directory')
    tracing = bool(a.trace_every_frame or expectations or
                   any('until_transformation' in x for x in actions))
    start = expectations['gameplay_start_frame'] if expectations else 0
    settings = dict(width=a.width, trace_every_frame=tracing, capture_start=start,
                    asset_sha256={name: hashlib.sha256(path.read_bytes()).hexdigest()
                                  for name, path in (('rom', a.rom), ('bios', a.bios),
                                                     ('state', a.state), ('runner', binary),
                                                     ('config', config))})
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        port = sock.getsockname()[1]

    def command(**arguments):
        with socket.create_connection(('127.0.0.1', port), timeout=45) as sock:
            sock.sendall((json.dumps(arguments) + '\n').encode())
            with sock.makefile('rb') as stream:
                reply = json.loads(stream.readline())
            require(reply.get('ok'), f'Native command failed: {reply}')
            return reply

    def capture(name, row, elapsed, with_state):
        shot = command(cmd='screenshot')
        image = name + '.png'
        Image.frombytes('RGB', (shot['w'], shot['h']), bytes.fromhex(shot['data'])).save(a.output / image)
        result = dict(elapsed_frame=elapsed, frame=row['frame'], image=image)
        if with_state:
            result['state'] = name + '.state'
            command(cmd='savestate_save', path=str(a.output / result['state']))
        return result

    env = os.environ.copy()
    for name in ('GBARECOMP_INPUT_REPLAY', 'GBARECOMP_INPUT_RECORD', 'GBARECOMP_WS_WIP', 'GBARECOMP_WIDESCREEN'):
        env.pop(name, None)
    env.update(GBARECOMP_STRICT_STATIC='1', GBARECOMP_FORCE_INTERP='0', GBARECOMP_BIOS_HLE='0',
               GBARECOMP_BIOS_SKIP_INTRO='1', SMA3_WIDE_PROBE_DIR=str(a.output),
               SMA3_AUTHORED_WIDE='1', SMA3_WIDE_OBJECT_BOUNDS='0')
    args = [str(binary), '--config', str(config), '--rom', str(a.rom.resolve()),
            '--bios', str(a.bios.resolve()), '--tcp', str(port), '--view-width', str(a.width),
            '--save-path', str(a.output / 'fresh.sav')]
    evidence = dict(session=[], trace=[], captures=[])
    status = dict(execution_complete=False)
    proc = None
    error = None
    try:
        with (a.output / 'run.log').open('w') as log:
            proc = subprocess.Popen(args, env=env, stdout=log, stderr=subprocess.STDOUT, cwd=ROOT)
            for _ in range(50):
                try:
                    command(cmd='ping')
                    break
                except OSError:
                    require(proc.poll() is None, 'Runner exited; see run.log')
                    time.sleep(.1)
            else:
                raise ValueError('Local debug server unavailable')
            command(cmd='savestate_load', path=str(a.state.resolve()))
            execute_session(command, actions, capture, evidence, tracing, start)
            command(cmd='savestate_save', path=str(a.output / 'final.state'))
            command(cmd='quit')
            proc.wait(timeout=10)
            require(proc.returncode == 0, 'Runner failed; see run.log')
            status['execution_complete'] = True
            if expectations:
                status['cycle_checks'] = check_car_cycle(actions, evidence['session'], evidence['trace'], expectations)
                require(status['cycle_checks']['cycle_pass'], '; '.join(status['cycle_checks']['cycle_errors']))
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as exc:
        error = str(exc)
        status['error'] = error
    finally:
        if proc is not None and proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
        outputs = {'session.json': evidence['session'], 'actions.json': actions,
                   'run-settings.json': settings, 'run-status.json': status}
        if tracing:
            outputs.update({'trace.json': evidence['trace'], 'captures.json': evidence['captures']})
        for name, value in outputs.items():
            (a.output / name).write_text(json.dumps(value, indent=2) + '\n')
    if error:
        raise SystemExit(error)
    print('Saved bounded private session; no full-game or Android qualification.')


if __name__ == '__main__':
    main()
