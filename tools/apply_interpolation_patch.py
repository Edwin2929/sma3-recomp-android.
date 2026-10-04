#!/usr/bin/env python3
"""Apply the reviewed presentation patch to the pinned sibling engine."""
from pathlib import Path
import subprocess, sys
engine = Path(sys.argv[1]).resolve()
patch = Path(__file__).resolve().parents[1] / 'patches/gbarecomp-interpolation.patch'
expected = 'e7728148c6829ba526f682876430a0c9022dc6c0'
actual = subprocess.check_output(['git', '-C', str(engine), 'rev-parse', 'HEAD'], text=True).strip()
if actual != expected: raise SystemExit('Interpolation requires GBARecomp commit ' + expected)
command = ['git', '-C', str(engine), 'apply']
if subprocess.run(command + ['--reverse', '--check', str(patch)], capture_output=True).returncode == 0: raise SystemExit(0)
subprocess.run(command + ['--check', str(patch)], check=True)
subprocess.run(command + [str(patch)], check=True)
