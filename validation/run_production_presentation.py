#!/usr/bin/env python3
"""Exercise the production image-mode callbacks with private SDL dummy fixtures.

Requires a host runner built with SMA3_PANORAMA=ON, Pillow, the supported USA
ROM/BIOS, a stable supported gameplay state and an unsupported transition state.
The output directory contains private images, saves and logs; publish only the
sanitized summary.json. Forced drawable sizes exercise the real adaptive-view
callback, but do not emulate Android display rotation or certify device speed.
"""

import argparse
import hashlib
import json
import os
import re
import subprocess
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def environment(output, drawable, mode):
    # A diagnostic route, guest input, or width override must not leak into the
    # production run through the invoking shell. Do not select the probe route.
    env = {key: value for key, value in os.environ.items()
           if not key.startswith(('SMA3_', 'GBARECOMP_'))}
    env.update(SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy',
               SDL_RENDER_DRIVER='software', GBARECOMP_FORCE_DRAWABLE=drawable,
               SMA3_IMAGE_MODE=str(mode), GBARECOMP_STRICT_STATIC='1',
               GBARECOMP_FORCE_INTERP='0', GBARECOMP_BIOS_HLE='0',
               GBARECOMP_BIOS_SKIP_INTRO='1',
               GBARECOMP_COVERAGE_JSON=str(output / 'native-coverage.json'),
               GBARECOMP_MISS_FRAG=str(output / 'native-misses.toml.frag'))
    return env


def parse_final(log):
    rows = re.findall(r'^final_pc=.*$', log, re.MULTILINE)
    require(len(rows) == 1, 'Expected one final runtime status')
    return {key: int(value, 0) for key, value in
            re.findall(r'(\w+)=(0x[0-9a-fA-F]+|[0-9]+)', rows[0])}


def run_case(args, case):
    name, mode, drawable, width, fixture, frames = case
    output = args.output / name
    output.mkdir()
    state = args.gameplay_state if fixture == 'gameplay' else args.fallback_state
    command = [args.runner.name, '--config', str(args.config),
               '--rom', str(args.rom), '--bios', str(args.bios),
               '--window', '--resize-view', '--load-state', str(state),
               '--frames', str(frames), '--save-path', str(output / 'fresh.sav'),
               '--dump-png', str(output / 'frame.png')]
    with (output / 'runner.log').open('w') as log_file:
        process = subprocess.run(command, executable=args.runner, cwd=output,
                                 env=environment(output, drawable, mode),
                                 stdout=log_file, stderr=subprocess.STDOUT,
                                 timeout=args.timeout)
    require(process.returncode == 0, f'{name}: runner exited {process.returncode}')
    log = (output / 'runner.log').read_text()
    require('renderer=software' in log, f'{name}: SDL software renderer not active')
    require('strict_static=ENABLED' in log, f'{name}: strict native execution not active')
    coverage = json.loads((output / 'native-coverage.json').read_text())
    require(coverage.get('ok') and coverage.get('coverage') == 'FULLY_STATIC',
            f'{name}: native execution coverage failed')
    for key in ('distinct_misses', 'interpreted_insns', 'healed_native', 'failed'):
        require(coverage.get(key) == 0, f'{name}: nonzero native diagnostic {key}')
    final = parse_final(log)
    require(final.get('frames_presented') == frames, f'{name}: wrong presented frame count')
    require(final.get('unmapped') == 0 and final.get('io_unhandled') == 0,
            f'{name}: guest bus or I/O fault')
    image = Image.open(output / 'frame.png').convert('RGB')
    require(image.size == (width, 160), f'{name}: framebuffer size {image.size}')
    sizes = [list(map(int, pair)) for pair in re.findall(
        r'host_window: presentation .*?logical=(\d+)x(\d+)', log)]
    require(sizes and sizes[-1] == [width, 160], f'{name}: missing presented logical size')
    require(all(size in ([240, 160], [width, 160]) for size in sizes),
            f'{name}: unexpected logical geometry transition')
    require(len(sizes) <= (2 if width > 240 else 1),
            f'{name}: repeated view fallback or geometry oscillation')
    cold_gaps = [dict(frame=int(frame), unresolved=int(unresolved))
                 for frame, unresolved in re.findall(
                     r'\[sma3:object-gap\] frame=(\d+).*? unresolved=(\d+)', log)]
    if mode != 1:
        require(not cold_gaps, f'{name}: disabled panorama retained object observation')
    result = dict(name=name, mode=mode, fixture=fixture, forced_drawable=drawable,
                  frames=frames, framebuffer=[width, 160], logical_presentations=sizes,
                  final_runtime=final, png_sha256=sha256(output / 'frame.png'),
                  native_coverage={key: coverage[key] for key in
                                   ('coverage', 'distinct_misses', 'interpreted_insns',
                                    'healed_native', 'failed')},
                  diagnostic_object_gap_observations=cold_gaps)
    return result, image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('runner', 'rom', 'bios', 'gameplay-state', 'fallback-state', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--config', type=Path, default=ROOT / 'runtime.toml')
    parser.add_argument('--frames', type=int, default=120)
    parser.add_argument('--fallback-frames', type=int, default=12)
    parser.add_argument('--timeout', type=float, default=60)
    args = parser.parse_args()
    try:
        require(args.frames > 0 and args.fallback_frames > 0 and args.timeout > 0,
                'Frame counts and timeout must be positive')
        for name in ('runner', 'rom', 'bios', 'gameplay_state', 'fallback_state', 'config'):
            path = getattr(args, name).resolve()
            require(path.is_file(), f'Missing required input: {path}')
            setattr(args, name, path)
        for path, expected in ((args.rom, '7352d2bd064d9ebaec579e264228aa21c7345b80'),
                               (args.bios, '300c20df6731a33952ded8c436f7f186d25d3492')):
            require(hashlib.sha1(path.read_bytes()).hexdigest() == expected,
                    'Unsupported ROM or BIOS revision')
        args.output = args.output.resolve()
        args.output.mkdir(parents=True, exist_ok=True)
        require(not any(args.output.iterdir()), 'Use a new empty private output directory')
        cases = [
            ('original-landscape', 0, '2400x1080', 240, 'gameplay', args.frames),
            ('panorama-347', 1, '3120x1440', 347, 'gameplay', args.frames),
            ('panorama-356', 1, '2400x1080', 356, 'gameplay', args.frames),
            ('panorama-portrait', 1, '1080x2400', 240, 'gameplay', args.frames),
            ('stretch-native-source', 2, '2400x1080', 240, 'gameplay', args.frames),
            ('transition-original', 0, '2400x1080', 240, 'fallback', args.fallback_frames),
            ('transition-panorama', 1, '2400x1080', 240, 'fallback', args.fallback_frames),
        ]
        results, images = {}, {}
        for case in cases:
            result, image = run_case(args, case)
            results[case[0]], images[case[0]] = result, image
            print(f'PASS {case[0]} logical={image.width}x{image.height}', flush=True)
        comparisons = []
        for name in results:
            reference = ('original-landscape' if results[name]['fixture'] == 'gameplay'
                         else 'transition-original')
            if name == reference:
                continue
            image, native = images[name], images[reference]
            left = (image.width - 240) // 2
            crop = image.crop((left, 0, left + 240, 160))
            require(crop.tobytes() == native.tobytes(), f'{name}: native center differs')
            require(results[name]['final_runtime'] == results[reference]['final_runtime'],
                    f'{name}: final guest runtime counters differ')
            comparison = dict(case=name, reference=reference,
                              native_center_bytes_equal=True, final_runtime_equal=True,
                              margins=[left, image.width - 240 - left])
            if image.width > 240:
                margins = (image.crop((0, 0, left, 160)),
                           image.crop((left + 240, 0, image.width, 160)))
                colors = [len(margin.getcolors(margin.width * margin.height) or [])
                          for margin in margins]
                require(all(count > 1 for count in colors), f'{name}: empty or solid margins')
                comparison['margin_distinct_colors'] = colors
            comparisons.append(comparison)
        summary = dict(
            schema=1, result='PASS',
            route='production callbacks; SMA3_WIDE_PROBE_DIR is absent',
            environment='SDL dummy video/audio, software renderer, synthetic drawable sizes',
            limits=[
                'Separate host launches do not test an Android device or live OS rotation.',
                'Stretch case verifies a native source framebuffer; Android destination scaling is separate.',
                'Final counter equality and native crop equality do not prove complete guest-state equality.',
                'Known supported scene and unsupported transition do not certify every level or offscreen spawn.',
                'Cold savestate observation may keep the original view until a coherent submission is available.',
            ],
            input_sha256={name: sha256(getattr(args, name)) for name in
                          ('runner', 'rom', 'bios', 'gameplay_state', 'fallback_state', 'config')},
            cases=list(results.values()), comparisons=comparisons)
        (args.output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
        print(f'PASS summary cases={len(results)} native_center_comparisons={len(comparisons)}')
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f'FAIL {error}\n')


if __name__ == '__main__':
    main()
