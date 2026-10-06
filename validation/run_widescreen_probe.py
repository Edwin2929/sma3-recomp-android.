#!/usr/bin/env python3
"""Desktop panoramic diagnostic. Keep generated RAM captures and saves private."""
import argparse
import hashlib
import os
from pathlib import Path
import re
import subprocess

root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--rom',type=Path,required=True)
p.add_argument('--bios',type=Path,required=True)
p.add_argument('--output',type=Path,required=True,help='Empty directory; each run needs a fresh save')
p.add_argument('--frames',type=int,default=9600)
p.add_argument('--width',type=int,default=356)
p.add_argument('--generic',action='store_true',help='Control run without authored scenery')
a=p.parse_args()
if not (240<=a.width<=356 and 1<=a.frames<=18000):p.error('width: 240..356; frames: 1..18000')
for path,size,digest in [(a.rom,4194304,'7352d2bd064d9ebaec579e264228aa21c7345b80'),
                         (a.bios,16384,'300c20df6731a33952ded8c436f7f186d25d3492')]:
    data=path.read_bytes()
    if len(data)!=size or hashlib.sha1(data).hexdigest()!=digest:p.error('Unsupported asset: '+path.name)
a.output.mkdir(parents=True,exist_ok=True)
if any(a.output.iterdir()):p.error('Use a new empty output directory')
env=os.environ.copy()
for key in ('GBARECOMP_WS_WIP','GBARECOMP_WIDESCREEN','GBARECOMP_INPUT_RECORD'):env.pop(key,None)
env.update(GBARECOMP_STRICT_STATIC='1',GBARECOMP_FORCE_INTERP='0',GBARECOMP_BIOS_HLE='0',
           GBARECOMP_BIOS_SKIP_INTRO='1',GBARECOMP_LOG_UNMAPPED='1',SMA3_WIDE_PROBE_DIR=str(a.output.resolve()),
           SMA3_AUTHORED_WIDE='0' if a.generic else '1',
           GBARECOMP_INPUT_REPLAY=str(root/'validation/extended-gameplay.csv'))
command=[str(root/'build/sma3_runner'),'--config',str(root/'runtime.toml'),
         '--rom',str(a.rom.resolve()),'--bios',str(a.bios.resolve()),'--frames',str(a.frames),
         '--view-width',str(a.width),'--save-path',str(a.output.resolve()/'fresh.sav'),
         '--dump-png',str(a.output.resolve()/'frame.png')]
with (a.output/'run.log').open('w') as log:
    result=subprocess.run(command,env=env,cwd=root,stdout=log,stderr=subprocess.STDOUT,timeout=360)
text=(a.output/'run.log').read_text()
if result.returncode or 'dispatch_misses=0 interpreted_insns=0 healed_native=0' not in text:
    raise SystemExit('Native dispatch failed; inspect run.log')
unmapped=re.findall(r'\bunmapped=(\d+)',text)
if not unmapped or int(unmapped[-1]):
    raise SystemExit('Unmapped accesses require investigation and a control run; inspect run.log')
print('PASS bounded native run:',a.output,'(not Android or full-game qualification)')
