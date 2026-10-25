#!/usr/bin/env python3
"""Capture every SDL texture upload during a production-path input replay.

Linux/little-endian host only; requires a C compiler and SDL2 headers. Use a
new private output directory: uploads.rgb, logs and saves contain game data.
No panoramic probe route is selected, and only normal KEYINPUT is replayed.
Synthetic window sizes test geometry, not physical Android display behavior.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('runner', 'rom', 'bios', 'state', 'actions', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--start-frame', type=int, required=True,
                        help='PPU frame stored in the supplied private state')
    parser.add_argument('--frames', type=int, default=600)
    parser.add_argument('--mode', type=int, choices=(0, 1, 2), default=1)
    parser.add_argument('--drawable', default='2400x1080')
    parser.add_argument('--drawable-sequence',
                        help='SDL query overrides by upload index, e.g. 0:2400x1080,280:1080x2400')
    parser.add_argument('--config', type=Path, default=ROOT / 'runtime.toml')
    parser.add_argument('--sdl-include', type=Path,
                        default=ROOT.parent / 'gbarecomp/platform/android/third_party/SDL/include')
    parser.add_argument('--cc', default='cc')
    parser.add_argument('--timeout', type=float, default=90)
    args = parser.parse_args()
    try:
        require(sys.platform.startswith('linux') and sys.byteorder == 'little',
                'This capture helper requires a little-endian Linux host')
        require(args.frames > 0 and args.start_frame >= 0 and args.timeout > 0,
                'Invalid frame count, start frame or timeout')
        for name in ('runner', 'rom', 'bios', 'state', 'actions', 'config'):
            path = getattr(args, name).resolve()
            require(path.is_file(), f'Missing input: {path}')
            setattr(args, name, path)
        require(os.access(args.runner, os.X_OK), 'The supplied runner is not executable')
        require((args.sdl_include / 'SDL.h').is_file(), 'SDL2 include directory is missing')
        require(re.fullmatch(r'[1-9]\d*x[1-9]\d*', args.drawable), 'Invalid drawable size')
        if args.drawable_sequence:
            require(re.fullmatch(r'\d+:[1-9]\d*x[1-9]\d*(,\d+:[1-9]\d*x[1-9]\d*)*',
                                 args.drawable_sequence), 'Invalid drawable sequence')
            starts = [int(item.split(':')[0]) for item in args.drawable_sequence.split(',')]
            require(starts[0] == 0 and starts == sorted(set(starts)),
                    'Drawable sequence must start at zero and strictly increase')
        actions = json.loads(args.actions.read_text())
        require(isinstance(actions, list) and actions, 'Actions must be a nonempty list')
        frame = args.start_frame
        events = []
        for action in actions:
            require(isinstance(action, dict) and set(action) == {'n', 'keys'} and
                    type(action['n']) is int and action['n'] > 0 and
                    type(action['keys']) is int and 0 <= action['keys'] <= 1023,
                    'Use a fixed action tape containing only positive n and active-low keys')
            events.append(f'{frame},0x{action["keys"]:03x}')
            frame += action['n']
        require(frame - args.start_frame >= args.frames, 'Action tape is shorter than the run')
        args.output = args.output.resolve()
        args.output.mkdir(parents=True, exist_ok=True)
        require(not any(args.output.iterdir()), 'Use a new empty private output directory')
        source = ROOT / 'validation/sdl_presentation_capture.c'
        tap = args.output / 'sdl_presentation_capture.so'
        subprocess.run([args.cc, '-shared', '-fPIC', '-O2', '-Wall', '-Wextra',
                        '-I' + str(args.sdl_include.resolve()), str(source), '-o', str(tap),
                        '-ldl'], check=True)
        replay = args.output / 'replay.csv'
        replay.write_text('# frame,keyinput_active_low\n' + '\n'.join(events) + '\n')
        env = {key: value for key, value in os.environ.items()
               if not key.startswith(('SMA3_', 'GBARECOMP_'))}
        env.update(SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', SDL_RENDER_DRIVER='software',
                   SMA3_IMAGE_MODE=str(args.mode), GBARECOMP_FORCE_DRAWABLE=args.drawable,
                   GBARECOMP_INPUT_REPLAY=str(replay), GBARECOMP_STRICT_STATIC='1',
                   GBARECOMP_FORCE_INTERP='0', GBARECOMP_BIOS_HLE='0',
                   GBARECOMP_BIOS_SKIP_INTRO='1',
                   GBARECOMP_COVERAGE_JSON=str(args.output / 'coverage.json'),
                   GBARECOMP_MISS_FRAG=str(args.output / 'misses.toml'),
                   SMA3_UPLOAD_TAP=str(args.output / 'uploads.rgb'), LD_PRELOAD=str(tap))
        if args.drawable_sequence:
            env.pop('GBARECOMP_FORCE_DRAWABLE')
            env['SMA3_TAP_DRAWABLES'] = args.drawable_sequence
        settings = dict(mode=args.mode, start_frame=args.start_frame, frames=args.frames,
                        drawables=args.drawable_sequence or args.drawable,
                        input_sha256={name: digest(getattr(args, name)) for name in
                                      ('runner', 'rom', 'bios', 'state', 'actions', 'config')},
                        capture_source_sha256=digest(source))
        (args.output / 'settings.json').write_text(json.dumps(settings, indent=2) + '\n')
        command = [args.runner.name, '--config', str(args.config), '--rom', str(args.rom),
                   '--bios', str(args.bios), '--window', '--resize-view', '--load-state',
                   str(args.state), '--frames', str(args.frames), '--save-path',
                   str(args.output / 'fresh.sav')]
        with (args.output / 'runner.log').open('w') as log:
            process = subprocess.run(command, executable=args.runner, cwd=args.output,
                                     env=env, stdout=log, stderr=subprocess.STDOUT,
                                     timeout=args.timeout)
        require(process.returncode == 0, f'Runner exited {process.returncode}')
        log = (args.output / 'runner.log').read_text()
        loaded = re.search(r'^savestate_loaded .* frame=(\d+)$', log, re.MULTILINE)
        require(loaded and int(loaded[1]) == args.start_frame, 'Loaded frame differs from replay origin')
        require('self_heal_coverage=FULLY_STATIC dispatch_misses=0 interpreted_insns=0 healed_native=0' in log,
                'Native execution did not remain fully static')
        require((args.output / 'uploads.rgb').stat().st_size > 16, 'No game texture uploads captured')
        print(f'PASS capture frames={args.frames}; private output: {args.output}')
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'FAIL {error}\n')


if __name__ == '__main__':
    main()
