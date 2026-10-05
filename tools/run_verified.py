#!/usr/bin/env python3
"""Run a bounded strict-static smoke test with verified user assets."""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--rom', type=Path, required=True)
parser.add_argument('--bios', type=Path, required=True)
parser.add_argument('--runner', type=Path, default=ROOT / 'build/sma3_runner')
parser.add_argument('--frames', type=int, default=120)
parser.add_argument('--timeout', type=int, default=120)
parser.add_argument('--output', type=Path, default=ROOT / 'smoke-frame.png')
parser.add_argument('--input', type=Path)
parser.add_argument('--save', type=Path)
parser.add_argument('--bios-intro', action='store_true', help='Include the original BIOS animation for diagnostics')
args = parser.parse_args()
if args.frames < 1 or args.timeout < 1:
    parser.error('frames and timeout must be positive')
for label, path, size, sha1 in [
    ('ROM', args.rom, 4194304, '7352d2bd064d9ebaec579e264228aa21c7345b80'),
    ('BIOS', args.bios, 16384, '300c20df6731a33952ded8c436f7f186d25d3492'),
]:
    if not path.is_file():
        parser.error(f'{label} missing: {path}')
    if path.stat().st_size != size:
        parser.error(f'{label} must contain exactly {size} bytes')
    if hashlib.sha1(path.read_bytes()).hexdigest() != sha1:
        parser.error(f'{label} checksum does not match the supported image')
env = os.environ.copy()
env['GBARECOMP_STRICT_STATIC'] = '1'
env['GBARECOMP_FORCE_INTERP'] = '0'
env['GBARECOMP_BIOS_HLE'] = '0'
env['GBARECOMP_BIOS_SKIP_INTRO'] = '0' if args.bios_intro else '1'
env.pop('GBARECOMP_INPUT_REPLAY', None)
env.pop('GBARECOMP_INPUT_RECORD', None)
if args.input:
    env['GBARECOMP_INPUT_REPLAY'] = str(args.input.resolve())
args.output.parent.mkdir(parents=True, exist_ok=True)
command = [str(args.runner.resolve()), '--config', str(ROOT / 'runtime.toml'),
           '--rom', str(args.rom.resolve()), '--bios', str(args.bios.resolve()),
           '--frames', str(args.frames), '--dump-png', str(args.output.resolve())]
if args.save:
    args.save.parent.mkdir(parents=True, exist_ok=True)
    command += ['--save-path', str(args.save.resolve())]
try:
    result = subprocess.run(command, env=env, cwd=ROOT, timeout=args.timeout)
except subprocess.TimeoutExpired:
    parser.exit(124, 'Smoke test timed out; game execution has NOT been validated.\n')
raise SystemExit(result.returncode)
