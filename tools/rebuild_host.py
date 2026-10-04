#!/usr/bin/env python3
"""Rebuild and validate the Linux diagnostic binary from this checkpoint."""
from pathlib import Path
import argparse
import subprocess
import shutil
root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--cmake', default=shutil.which('cmake') or str(root.parent/'build-tools/cmake/data/bin/cmake'))
parser.add_argument('--jobs', type=int, default=4)
args = parser.parse_args()
def run(*command): subprocess.run([str(x) for x in command], check=True)
generated = root/'generated-project'
run(args.cmake, '-S', generated, '-B', generated/'build', '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_CXX_FLAGS_RELEASE=-O0 -g0')
run(args.cmake, '--build', generated/'build', '--target', 'gbarecomp_game', '-j', args.jobs)
# Archivers may accept empty objects left by interrupted compilation.
# Repair only corrupt outputs, preserving all successfully built objects.
for obj in sorted((generated/'build/CMakeFiles/gbarecomp_game.dir/generated').glob('*.o')):
    with obj.open('rb') as stream: magic = stream.read(4)
    if magic == b'\x7fELF': continue
    run('c++', '-O0', '-g0', '-std=gnu++20', '-I', root.parent/'gbarecomp/src/armv4t',
        '-I', generated/'generated', '-c', generated/'generated'/obj.name.removesuffix('.o'), '-o', obj)
    with obj.open('rb') as stream:
        if stream.read(4) != b'\x7fELF': raise RuntimeError(f'Invalid object: {obj}')
    run('ar', 'r', generated/'build/libgbarecomp_game.a', obj)
run('python3', root/'tools/verify_archive.py', generated/'build/libgbarecomp_game.a')
command = [args.cmake, '-S', root, '-B', root/'build', '-DCMAKE_BUILD_TYPE=Release',
           f'-DSMA3_PREBUILT_GAME={generated}/build/libgbarecomp_game.a']
toml = root.parent/'gbarecomp/build/_deps/tomlplusplus-src'
if (toml/'toml.hpp').exists(): command.append(f'-DFETCHCONTENT_SOURCE_DIR_TOMLPLUSPLUS={toml}')
run(*command)
run(args.cmake, '--build', root/'build', '--target', 'sma3_runner', '-j', args.jobs)
