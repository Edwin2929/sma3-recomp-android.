#!/usr/bin/env python3
"""Apply the ordered SMA3 patches without overwriting unrelated engine edits."""
from pathlib import Path
import hashlib,subprocess,sys,tempfile
engine=Path(sys.argv[1]).resolve();root=Path(__file__).resolve().parents[1]
expected='e7728148c6829ba526f682876430a0c9022dc6c0'
if subprocess.check_output(['git','-C',str(engine),'rev-parse','HEAD'],text=True).strip()!=expected:raise SystemExit('SMA3 requires GBARecomp commit '+expected)
patches=[root/'patches'/('gbarecomp-'+n+'.patch') for n in ['interpolation','controls','localization','touch-design','widescreen-objects']]
paths=['src/runtime/'+n for n in ['host_platform.cpp','host_platform.h','host_window.cpp','runtime.cpp']]
paths += ['src/gba/gba_ppu.h','src/gba/gba_ppu.cpp']
def snapshot(folder):return tuple(hashlib.sha256((folder/p).read_bytes()).digest() for p in paths)
# Later patches may overlap earlier reverse-check context. Reconstruct each
# recognized state rather than relying on independent reverse checks.
with tempfile.TemporaryDirectory(prefix='sma3-patches-') as work:
 temp=Path(work)
 for path in paths:
  p=temp/path;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(subprocess.check_output(['git','-C',str(engine),'show','HEAD:'+path]))
 states=[snapshot(temp)]
 for patch in patches:
  subprocess.run(['git','apply',str(patch)],cwd=temp,check=True);states.append(snapshot(temp))
 current=snapshot(engine)
 if current not in states:raise SystemExit('Unrecognized local engine changes; refusing to overwrite them')
 for patch in patches[states.index(current):]:subprocess.run(['git','apply',str(patch)],cwd=engine,check=True)
 if snapshot(engine)!=states[-1]:raise SystemExit('Patched engine verification failed')
