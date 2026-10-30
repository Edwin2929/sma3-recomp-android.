#!/usr/bin/env python3
"""Measure the actual recompiled game and Android presentation path on Linux.

Requires a runner built with SMA3_PRESENTATION_DIAGNOSTICS=ON and private,
verified game assets. The SDL software surface is real, but is not an Android
GPU, physical display, thermal test or measurement of compositor scanout.
Output includes private images/saves; publish only sanitized numeric results.
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


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('runner', 'rom', 'bios', 'state', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--config', type=Path, default=ROOT / 'runtime.toml')
    parser.add_argument('--actions', type=Path)
    parser.add_argument('--start-frame', type=int)
    parser.add_argument('--frames', type=int, default=360)
    parser.add_argument('--fps', type=int, choices=(60, 120), default=120)
    parser.add_argument('--drawable', default='480x320')
    parser.add_argument('--mode', type=int, choices=(0, 1, 2), default=0)
    parser.add_argument('--filter', type=int, choices=range(7), default=1)
    parser.add_argument('--quality', type=int, choices=(0, 720, 1080), default=0)
    parser.add_argument('--controls', type=int, choices=(0, 1), default=1)
    parser.add_argument('--sdl-include', type=Path,
                        default=ROOT.parent / 'gbarecomp/platform/android/third_party/SDL/include')
    parser.add_argument('--timeout', type=float, default=90)
    parser.add_argument('--allow-empty-diagnostics', action='store_true',
                        help='For a separately verified diagnostic build: allow no collector record '
                             'only when SDL measured exactly the mandatory presentations. '
                             'Missing internal metrics remain unavailable, not zero-filled.')
    args = parser.parse_args()
    if not sys.platform.startswith('linux'):
        parser.error('Requires Linux LD_PRELOAD and SDL2')
    for name in ('runner', 'rom', 'bios', 'state', 'config'):
        path = getattr(args, name).resolve()
        if not path.is_file():
            parser.error('Missing input: ' + str(path))
        setattr(args, name, path)
    if args.frames <= 0 or args.timeout <= 0 or not re.fullmatch(r'[1-9]\d*x[1-9]\d*', args.drawable):
        parser.error('Invalid frames, timeout or drawable')
    if max(map(int, args.drawable.split('x'))) > 8192:
        parser.error('Drawable must be at most 8192 pixels per axis')
    for path, expected in ((args.rom, '7352d2bd064d9ebaec579e264228aa21c7345b80'),
                           (args.bios, '300c20df6731a33952ded8c436f7f186d25d3492')):
        if hashlib.sha1(path.read_bytes()).hexdigest() != expected:
            parser.error('Unsupported ROM/BIOS revision')
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    if any(out.iterdir()):
        parser.error('Use a new empty private output directory')
    tap = out / 'sdl_presentation_timing.so'
    subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-Wall', '-Wextra',
                    '-I' + str(args.sdl_include.resolve()),
                    str(ROOT / 'validation/sdl_presentation_timing.c'), '-o', str(tap), '-ldl'], check=True)
    env = {k: v for k, v in os.environ.items() if not k.startswith(('SMA3_', 'GBARECOMP_'))}
    env.update(SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', SDL_RENDER_DRIVER='software',
               SMA3_PRESENTATION_FPS=str(args.fps), SMA3_IMAGE_MODE=str(args.mode),
               SMA3_IMAGE_FILTER=str(args.filter), SMA3_VIDEO_QUALITY=str(args.quality),
               SMA3_TIMING_DRAWABLE=args.drawable, GBARECOMP_TOUCH_CONTROLS=str(args.controls),
               GBARECOMP_TOUCH_DPI='406.4', GBARECOMP_STRICT_STATIC='1',
               GBARECOMP_FORCE_INTERP='0', GBARECOMP_BIOS_HLE='0', GBARECOMP_BIOS_SKIP_INTRO='1',
               GBARECOMP_COVERAGE_JSON=str(out / 'coverage.json'),
               GBARECOMP_MISS_FRAG=str(out / 'misses.toml'), LD_PRELOAD=str(tap))
    if args.actions:
        if args.start_frame is None or args.start_frame < 0:
            parser.error('--actions requires a nonnegative --start-frame')
        frame = args.start_frame
        rows = []
        for action in json.loads(args.actions.read_text()):
            if (set(action) != {'n', 'keys'} or type(action['n']) is not int or action['n'] <= 0
                    or type(action['keys']) is not int or not 0 <= action['keys'] <= 1023):
                parser.error('Invalid action tape')
            rows.append(f'{frame},0x{action["keys"]:03x}')
            frame += action['n']
        if frame - args.start_frame < args.frames:
            parser.error('Action tape is shorter than requested frames')
        replay = out / 'replay.csv'
        replay.write_text('# frame,keyinput_active_low\n' + '\n'.join(rows) + '\n')
        env['GBARECOMP_INPUT_REPLAY'] = str(replay)
    command = [args.runner.name, '--config', str(args.config), '--rom', str(args.rom),
               '--bios', str(args.bios), '--window', '--resize-view', '--load-state', str(args.state),
               '--frames', str(args.frames), '--save-path', str(out / 'private.sav'),
               '--dump-png', str(out / 'final.png')]
    with (out / 'runner.log').open('w') as log:
        process = subprocess.run(command, executable=args.runner, cwd=out, env=env,
                                 stdout=log, stderr=subprocess.STDOUT, timeout=args.timeout)
    if process.returncode:
        raise RuntimeError(f'Runner exited {process.returncode}; see private runner.log')
    summarize(args, out)


def summarize(args, out):
    """Read one completed private run without re-executing the game."""
    log = (out / 'runner.log').read_text()
    def record(prefix, allow_empty=False):
        rows = [line[len(prefix):] for line in log.splitlines() if line.startswith(prefix)]
        if not rows and allow_empty:
            return None
        if len(rows) != 1:
            raise RuntimeError('Expected one record: ' + prefix)
        return json.loads(rows[0])
    diagnostics = record('[presentation-diagnostics] ', args.allow_empty_diagnostics)
    timing = record('[sdl-presentation-timing] ')
    optional_presentations = timing['completed_present_calls'] - args.frames
    if optional_presentations < 0 or (diagnostics is None and optional_presentations != 0):
        raise RuntimeError('SDL presentation count disagrees with missing diagnostic record')
    if diagnostics is not None and diagnostics['optional_success'] != optional_presentations:
        raise RuntimeError('SDL optional presentation count disagrees with diagnostic record')
    coverage = json.loads((out / 'coverage.json').read_text())
    if (coverage.get('coverage') != 'FULLY_STATIC' or
            any(coverage.get(k) != 0 for k in ('distinct_misses', 'interpreted_insns', 'healed_native', 'failed'))):
        raise RuntimeError('Native guest coverage failed')
    final_rows = re.findall(r'^final_pc=.*$', log, re.M)
    if len(final_rows) != 1:
        raise RuntimeError('Missing final runtime status')
    final = {key: int(value, 0) for key, value in
             re.findall(r'(\w+)=(0x[0-9a-fA-F]+|[0-9]+)', final_rows[0])}
    if final.get('frames_presented') != args.frames or final.get('unmapped') or final.get('io_unhandled'):
        raise RuntimeError('Guest frame count or bus checks failed')
    if args.start_frame is not None:
        loaded = re.search(r'^savestate_loaded .* frame=(\d+)$', log, re.M)
        # Windowed present-in-place starts with the already latched image of
        # the restored state. N presentations therefore span N-1 guest frames.
        if (not loaded or int(loaded[1]) != args.start_frame or
                final.get('ppu_frames') != args.start_frame + args.frames - 1):
            raise RuntimeError('Unexpected loaded state or final PPU frame')
    result = dict(scope='native_game_with_host_software_sdl_not_phone_fps', frames=args.frames,
                  fps=args.fps, drawable=args.drawable, mode=args.mode, filter=args.filter,
                  quality=args.quality, controls=bool(args.controls), timing=timing,
                  diagnostics=diagnostics, diagnostics_available=diagnostics is not None,
                  optional_presentations_measured=optional_presentations,
                  final_runtime=final, final_png_sha256=sha(out / 'final.png'),
                  input_sha256={name: sha(getattr(args, name)) for name in
                                ('runner', 'rom', 'bios', 'state', 'config')},
                  coverage='FULLY_STATIC', result='PASS')
    if args.actions:
        result['input_sha256']['actions'] = sha(args.actions)
    (out / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({'result': 'PASS', 'timing': timing,
                      'extra_presentations': optional_presentations,
                      'diagnostics_available': diagnostics is not None,
                      'late_admissions': diagnostics['admission_late'] if diagnostics else None}, indent=2))


if __name__ == '__main__':
    main()
