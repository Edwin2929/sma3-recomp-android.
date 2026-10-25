#!/usr/bin/env python3
"""Link a host probe that executes the real optional midpoint/full-frame path.

Requires an already completed Unix Makefiles host build. Recompile only its
runtime.cpp with SMA3_FRAME_INTERPOLATION=1 and add read-only upload logging.
The other objects/archives are reused from that exact build; supply the matching
engine source. This is not an Android build or a 120 Hz physical-display test.
The new directory contains private generated-code-linked binaries: do not publish.
"""

import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def require(condition, message):
    if not condition:
        raise ValueError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--engine', type=Path, default=ROOT.parent / 'gbarecomp')
    parser.add_argument('--runtime-source', type=Path,
                        help='Optional matching historical runtime.cpp reconstructed from its patch stack')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        args.build, args.engine, args.output = (path.resolve() for path in
                                               (args.build, args.engine, args.output))
        runtime = (args.runtime_source or args.engine / 'src/runtime/runtime.cpp').resolve()
        source = runtime.read_text()
        source_sha = hashlib.sha256(source.encode()).hexdigest()
        needle = 'bool sma3_frame_paced = false;'
        require(source.count(needle) == 1, 'Runtime lacks the expected presentation path')
        source = source.replace(needle, needle + ' unsigned sma3_capture_serial = 0;')
        needle = 'if (pacer->wait_for_midpoint()) win.present(has_middle ? sma3_blend.middle() : live_fb.data());'
        require(source.count(needle) == 1, 'Unexpected midpoint implementation')
        source = source.replace(needle, '''if (pacer->wait_for_midpoint()) {
                std::fprintf(stderr,"[blend:upload] index=%u frame=%llu kind=middle blended=%d width=%u\\n",sma3_capture_serial++,static_cast<unsigned long long>(ppu.frame_count()),int(has_middle),ppu.render_width());
                win.present(has_middle ? sma3_blend.middle() : live_fb.data());
            }''')
        needle = '        win.present(live_fb.data());\n    };\n\n    // Present-in-place'
        require(source.count(needle) == 1, 'Unexpected full-frame presentation implementation')
        source = source.replace(needle, '''        std::fprintf(stderr,"[blend:upload] index=%u frame=%llu kind=full blended=0 width=%u\\n",sma3_capture_serial++,static_cast<unsigned long long>(ppu.frame_count()),ppu.render_width());
        win.present(live_fb.data());
    };

    // Present-in-place''')
        args.output.mkdir(parents=True, exist_ok=True)
        require(not any(args.output.iterdir()), 'Use a new empty private output directory')
        instrumented = args.output / 'runtime_blend_probe.cpp'
        instrumented.write_text(source)
        settings = args.output / 'enable_blend.cpp'
        settings.write_text('#include "frame_interpolation.h"\n'
                            'namespace {const bool init=[](){sma3::interpolation_enabled.store(true);return true;}();}\n')
        flags = {}
        flags_path = args.build / 'engine/CMakeFiles/gbarecomp_runtime.dir/flags.make'
        for line in flags_path.read_text().splitlines():
            if ' = ' in line:
                key, value = line.split(' = ', 1)
                flags[key] = shlex.split(value)
        options = flags['CXX_DEFINES'] + flags['CXX_INCLUDES'] + flags['CXX_FLAGS']
        options = [value for value in options if not value.startswith('-DGBARECOMP_SRC_DIR')]
        options += [f'-DGBARECOMP_SRC_DIR="{args.engine}"', '-DSMA3_FRAME_INTERPOLATION=1']
        link_file = args.build / 'CMakeFiles/sma3_runner.dir/link.txt'
        link = shlex.split(link_file.read_text())
        compiler = link[0]
        runtime_object, settings_object = args.output / 'runtime_blend_probe.o', args.output / 'enable_blend.o'
        subprocess.run([compiler] + options + ['-c', str(instrumented), '-o', str(runtime_object)],
                       cwd=args.build, check=True)
        subprocess.run([compiler, '-std=c++20', '-O2', '-I' + str(ROOT / 'src'),
                        '-c', str(settings), '-o', str(settings_object)], check=True)
        runner = args.output / 'sma3_runner_blend_probe'
        link[link.index('-o') + 1] = str(runner)
        # Resolve runtime symbols directly before visiting the runtime archive;
        # its original runtime.cpp member is then not extracted by the linker.
        link.insert(link.index('-o'), str(runtime_object))
        link.insert(link.index('-o'), str(settings_object))
        subprocess.run(link, cwd=args.build, check=True)
        result = dict(scope='Actual host midpoint/full-frame code path; no Android device execution',
                      runtime_source_sha256=source_sha,
                      runner_sha256=hashlib.sha256(runner.read_bytes()).hexdigest(),
                      runtime_flags_sha256=hashlib.sha256(flags_path.read_bytes()).hexdigest(),
                      original_link_command_sha256=hashlib.sha256(link_file.read_bytes()).hexdigest())
        (args.output / 'build.json').write_text(json.dumps(result, indent=2) + '\n')
        print(f'PASS host blend probe: {runner}')
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        parser.exit(1, f'FAIL {error}\n')


if __name__ == '__main__':
    main()
