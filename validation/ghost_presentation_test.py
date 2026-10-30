#!/usr/bin/env python3
"""Capture a real saved ghost through SDL, with an identical ghost-off replay.

Run only after ghost_native_test.py has completed its natural record. This
driver reloads its entry checkpoint once per process, retries through the main
entrance using ordinary buttons, waits 30 extra frames and takes three hops.
It never loads an intermediate state or changes guest memory. The SDL readback
is intentionally not a benchmark. Keep all output except --report private.
"""

import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess

from PIL import Image, ImageChops

from ghost_native_test import ghost_snapshot
from make_level_fixture import sections
from run_panorama_session import ROOT
from time_trial_native_test import snapshot
from validate_localization_gameplay import ASSET_SHA1, coverage, digest, require

RUNNER_SHA256 = '9e5068fe31818e340d9cf508f61e00a8b4ed09f6aa3e353cbbc298084f8d55a9'
ENTRY_SHA256 = 'f3da3fc0e6a0354afcf4cf77fc552dd86f3c5bf6eb43c8ed040e014ff3af9b13'
ENTRY_FRAME = 9621


def presented(folder):
    data = (folder / 'present.rgba').read_bytes()
    require(len(data) >= 16, 'Missing SDL drawable capture')
    number, width, height, size = struct.unpack_from('<4I', data)
    require(0 < width <= 4096 and 0 < height <= 4096 and size == width * height * 4 and
            len(data) == size + 16, 'Malformed SDL drawable capture')
    image = Image.frombytes('RGBA', (width, height), data[16:]).convert('RGB')
    image.save(folder / 'present.png')
    return number, image


def run(args, enabled, actions, store, bests):
    name = 'ghost-on' if enabled else 'ghost-off'
    folder = args.private_output / name
    folder.mkdir()
    frames = sum(row['n'] for row in actions)
    env = {key: value for key, value in os.environ.items()
           if not key.startswith(('SMA3_', 'GBARECOMP_')) and key != 'LD_PRELOAD'}
    env.update(SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy',
               SDL_RENDER_DRIVER='software', GBARECOMP_STRICT_STATIC='1',
               GBARECOMP_FORCE_INTERP='0', GBARECOMP_BIOS_HLE='0',
               GBARECOMP_BIOS_SKIP_INTRO='1', GBARECOMP_TOUCH_CONTROLS='0',
               GBARECOMP_COVERAGE_JSON=str(folder / 'native-coverage.json'),
               GBARECOMP_MISS_FRAG=str(folder / 'native-misses.toml.frag'),
               GBARECOMP_INPUT_REPLAY=str(args.private_output / 'replay.csv'),
               SMA3_LANGUAGE='0', SMA3_IMAGE_MODE='0', SMA3_IMAGE_FILTER='1',
               SMA3_PRESENTATION_FPS='60', SMA3_VIDEO_QUALITY='0',
               SMA3_TIME_TRIAL='1', SMA3_TIME_TRIAL_REPORT=str(folder / 'time-trial.json'),
               LD_PRELOAD=str(args.tap), SMA3_GHOST_PRESENT_TAP=str(folder / 'present.rgba'),
               SMA3_GHOST_PRESENT_FROM=str(max(0, frames - 24)),
               SMA3_GHOST_PRESENT_THROUGH=str(frames + 32))
    if enabled:
        env.update(SMA3_TIME_TRIAL_GHOST_DIR=str(store),
                   SMA3_TIME_TRIAL_GHOST_BESTS=str(bests),
                   SMA3_TIME_TRIAL_GHOST_REPORT=str(folder / 'ghost.json'))
    # --tcp chooses the runtime's headless stepping branch. Real presentation
    # uses the ordinary windowed loop and its existing frame-indexed input
    # replay. No observer server is needed for this bounded capture.
    command = [str(args.runner), '--config', str(ROOT / 'runtime.toml'),
               '--rom', str(args.rom), '--bios', str(args.bios), '--window', '--resize-view',
               '--scale', str(args.scale), '--load-state', str(args.entry_state),
               '--frames', str(frames), '--save-path', str(folder / 'fresh.sav'),
               '--dump-png', str(folder / 'guest.png')]
    with (folder / 'run.log').open('w') as log:
        process = subprocess.run(command, cwd=folder, env=env, stdout=log,
                                 stderr=subprocess.STDOUT, timeout=args.timeout)
    require(process.returncode == 0, 'Windowed runner did not exit cleanly')
    timer = snapshot(folder)
    require(timer[2] == 2 and timer[3] == 1 and timer[5] == 1 and timer[6] == 0 and timer[9] == 0,
            'Presentation retry did not remain one valid running attempt')
    log = (folder / 'run.log').read_text()
    require('renderer=software' in log and 'strict_static=ENABLED' in log,
            'Expected actual SDL software presentation and strict native execution')
    loaded = re.findall(r'^savestate_loaded .* frame=(\d+)$', log, re.MULTILINE)
    require(loaded == [str(ENTRY_FRAME)], 'Expected exactly one load at the absolute input-replay origin')
    # runtime.toml enables quiet mode, which suppresses the optional replay
    # banner. The configured CSV, valid retry timer, renderer and presentation
    # count establish this path without requiring a nonessential log line.
    require('tcp_debug_server listening' not in log, 'Unexpected headless TCP stepping branch')
    index, image = presented(folder)
    require(index == frames - 1, 'Readback did not capture the final expected presentation')
    require(image.size == (240 * args.scale, 160 * args.scale), 'Unexpected actual SDL drawable size')
    counters = re.findall(r'^final_pc=.*$', log, re.MULTILINE)
    require(len(counters) == 1, 'Missing final runtime counters')
    runtime = {key: int(value, 0) for key, value in
               re.findall(r'(\w+)=(0x[0-9a-fA-F]+|[0-9]+)', counters[0])}
    require(runtime.get('io_unhandled') == 0 and runtime.get('frames_presented') == frames,
            'Unhandled IO or unexpected presentation count')
    require(Image.open(folder / 'guest.png').size == (240, 160), 'Unexpected faithful framebuffer size')
    result = {'name': name, 'requested_frames': frames,
              'initial_loads': 1, 'intermediate_loads': 0, 'timer': timer,
              'coverage': coverage(folder), 'runtime': runtime,
              'presentation_index': index,
              'presentation_size': list(image.size), 'presentation_sha256': digest(folder / 'present.png'),
              'faithful_framebuffer_sha256': digest(folder / 'guest.png')}
    if enabled:
        result['ghost'] = ghost_snapshot(folder)
    print(json.dumps({'case': name, 'coverage': result['coverage'],
                      'presentation_size': result['presentation_size'],
                      'presentation_index': index, 'timer': timer[:14]}), flush=True)
    return result, image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('runner', 'rom', 'bios', 'entry_state', 'ghost_directory', 'bests', 'tap',
                 'private_output', 'report'):
        parser.add_argument('--' + name.replace('_', '-'), type=Path, required=True)
    parser.add_argument('--actions', type=Path, default=ROOT / 'validation/ghost-course-1-1.json')
    parser.add_argument('--delay', type=int, default=30)
    parser.add_argument('--scale', type=int, default=4)
    parser.add_argument('--timeout', type=float, default=45)
    args = parser.parse_args()
    for name in ('runner', 'rom', 'bios', 'entry_state', 'ghost_directory', 'bests', 'tap', 'actions'):
        setattr(args, name, getattr(args, name).resolve())
    require(digest(args.runner) == RUNNER_SHA256, 'Expected frozen final candidate runner')
    require(digest(args.entry_state) == ENTRY_SHA256, 'Expected disclosed Try Again entry checkpoint')
    for name in ('rom', 'bios'):
        require(hashlib.sha1(getattr(args, name).read_bytes()).hexdigest() == ASSET_SHA1[name],
                'Unsupported private game asset')
    require(1 <= args.delay <= 60 and 1 <= args.scale <= 6 and 0 < args.timeout <= 120,
            'Delay/scale/timeout outside qualification bounds')
    args.private_output = args.private_output.resolve()
    require(not args.private_output.is_relative_to(ROOT), 'Keep private evidence outside the repository')
    args.private_output.mkdir(parents=True, exist_ok=True)
    require(not any(args.private_output.iterdir()), 'Use a new empty private output directory')
    route = json.loads(args.actions.read_text())
    require(len(route) >= 8 and route[1].get('capture') == 'running' and
            [(a['n'], a['keys']) for a in route[:2]] == [(1, 1022), (480, 1023)],
            'Expected the complete-record route main-entry prefix')
    actions = [{key: value for key, value in row.items() if key != 'capture'} for row in route[:2]]
    actions += [{'n': args.delay, 'keys': 1023}]
    actions += [{key: value for key, value in row.items() if key != 'capture'} for row in route[2:8]]
    (args.private_output / 'presentation-actions.json').write_text(json.dumps(actions, indent=2) + '\n')
    frame, events = ENTRY_FRAME, []
    for action in actions:
        require(set(action) == {'n', 'keys'} and type(action['n']) is int and action['n'] > 0 and
                type(action['keys']) is int and 0 <= action['keys'] <= 1023, 'Invalid button tape')
        events.append(f'{frame},0x{action["keys"]:03x}')
        frame += action['n']
    (args.private_output / 'replay.csv').write_text('# frame,keyinput_active_low\n' + '\n'.join(events) + '\n')
    values = [int(value) for value in args.bests.read_text().split()]
    require(len(values) == 60 and values[0] > 0 and not any(values[1:]), 'Expected the real persisted 1-1 best')
    source = args.ghost_directory / 'course-0.ghost'
    ghost_bytes = source.read_bytes()
    require(64 <= len(ghost_bytes) <= 4 * 1024 * 1024 and ghost_bytes[:8] == b'SMA3GHO1' and
            struct.unpack_from('<I', ghost_bytes, 44)[0] == values[0],
            'Complete stored ghost duration does not match the durable best')
    store = args.private_output / 'saved-ghost-copy'
    store.mkdir(mode=0o700)
    shutil.copy2(source, store / source.name)
    bests = args.private_output / 'persisted-bests.txt'
    shutil.copy2(args.bests, bests)
    with ThreadPoolExecutor(max_workers=2) as pool:
        on_future = pool.submit(run, args, True, actions, store, bests)
        off_future = pool.submit(run, args, False, actions, store, bests)
        (on, on_image), (off, off_image) = on_future.result(), off_future.result()
    require(on['timer'] == off['timer'], 'Ghost presentation changed the timer')
    require(on['faithful_framebuffer_sha256'] == off['faithful_framebuffer_sha256'],
            'Ghost presentation changed the faithful framebuffer')
    require(on['runtime'] == off['runtime'], 'Ghost presentation changed final guest runtime counters')
    require(on['presentation_index'] == off['presentation_index'], 'Compared different SDL presentation indices')
    require(on['ghost'][5] == 2 and on['ghost'][10] == 0 and on['ghost'][11] == 1 and
            on['ghost'][12] == values[0], 'Stored matching-best ghost was not loaded and eligible')
    require((store / source.name).read_bytes() == ghost_bytes and len(list(store.iterdir())) == 1,
            'An incomplete presentation retry modified the saved ghost')
    difference = ImageChops.difference(on_image, off_image)
    bounds = difference.getbbox()
    require(bounds is not None, 'The real SDL output contains no visible ghost')
    pixels = sum(1 for color in difference.getdata() if color != (0, 0, 0))
    require(20 * args.scale**2 <= pixels <= 4096 * args.scale**2,
            'SDL changes do not fit a visible, bounded ghost sprite')
    require(bounds[2] - bounds[0] <= 64 * args.scale and bounds[3] - bounds[1] <= 64 * args.scale,
            'SDL changes exceed the bounded ghost bitmap')
    difference.save(args.private_output / 'present-difference.png')
    comparison = Image.new('RGB', (on_image.width * 2, on_image.height))
    comparison.paste(off_image, (0, 0)); comparison.paste(on_image, (on_image.width, 0))
    comparison.save(args.private_output / 'off-on-comparison.png')
    entry = args.entry_state.read_bytes()
    bus_offset, bus_size = sections(entry)[b'BUS0']
    require(bus_size == 395416, 'Unexpected pinned entry bus layout')
    inherited_unmapped = struct.unpack_from('<Q', entry, bus_offset + 395269)[0]
    report = {
        'schema': 1, 'release': '0.10.17-beta', 'result': 'PASS',
        'scope': 'Real SDL software drawable from the frozen native runner, with and without a reloaded personal-best ghost; not a physical Android capture or performance benchmark',
        'entry_provenance': 'The same disclosed Try Again checkpoint used by ghost_native_test.py; its older prepared ancestry is not a fresh-boot campaign. Each process loads it once and retries through the main entrance using ordinary buttons.',
        'source_sha256': {name: digest(ROOT / name) for name in (
            'validation/ghost_presentation_test.py', 'validation/ghost_native_test.py',
            'validation/ghost-course-1-1.json', 'validation/sdl_ghost_present_capture.c',
            'src/main.cpp', 'src/time_trial_ghost.cpp', 'src/ghost_visual.cpp',
            'src/ghost_visual_detail.h')},
        'input_sha256': {name: digest(getattr(args, name)) for name in (
            'runner', 'rom', 'bios', 'entry_state', 'bests', 'tap')},
        'saved_ghost': {'name': source.name, 'bytes': len(ghost_bytes), 'sha256': digest(source),
                        'best_frames': values[0], 'unchanged_after_retry': True},
        'presentation': {'mode': 'Original', 'filter': 'Nearest', 'requested_fps': 60,
                         'integer_scale': args.scale, 'extra_neutral_frames': args.delay,
                         'touch_controls': False, 'changed_pixels': pixels,
                         'difference_bounds_xyxy': list(bounds)},
        'checks': {'same_guest_timer': True, 'same_faithful_framebuffer': True,
                   'same_final_runtime_counters': True, 'visible_sdl_difference': True,
                   'difference_fits_64_pixel_sprite': True, 'saved_record_unchanged': True,
                   'strict_static_both_cases': True, 'guest_memory_patched': False,
                   'intermediate_state_loads': 0},
        'cases': [on, off],
        'bus_counter_note': {'entry_unmapped': inherited_unmapped,
                             'final_unmapped_both_cases': on['runtime']['unmapped'],
                             'claim': 'Identical on/off counters, not a zero-unmapped claim; strict-static dispatch misses and unhandled IO are zero.'},
        'limitations': ['SDL software renderer under dummy video, not a physical phone',
                        'This pair compares the timer, final faithful framebuffer and runtime counters; complete guest-state comparisons are in the separate uninterrupted native-route report',
                        'Original mode with Nearest at integer scale; other geometry and clipping have component tests',
                        'SDL readback perturbs timings; no benchmark or FPS measurement',
                        'ROM, BIOS, motion, screenshots, SRAM and GBAS remain private'],
    }
    args.report.write_text(json.dumps(report, indent=2) + '\n')
    (args.private_output / 'presentation-report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'result': 'PASS', 'changed_pixels': pixels, 'bounds': bounds,
                      'report': str(args.report), 'private_comparison': str(args.private_output / 'off-on-comparison.png')}), flush=True)


if __name__ == '__main__':
    main()
