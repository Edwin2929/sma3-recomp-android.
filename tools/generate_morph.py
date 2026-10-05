#!/usr/bin/env python3
"""Compile the byte-verified, dynamically allocated morph routine as native code.
Only this audited position-independent Thumb routine is relocated. Code addresses
and PC-relative loads follow the allocation; switch labels stay canonical.
"""
from pathlib import Path
import argparse, subprocess, re
p=argparse.ArgumentParser();p.add_argument('--rom',required=True,type=Path);p.add_argument('--recompiler',type=Path)
a=p.parse_args();root=Path(__file__).resolve().parents[1]
out=root/'generated-morph';out.mkdir(exist_ok=True)
base=0x03004D50;source=0x08040CE0;size=0x4B4
config=out/'config.toml'
config.write_text('''[program]
name="SMA3 dynamically allocated morph"
id="morph"
load_address=0x08000000
size=0x00400000
entry_pc=0x03004D50
speculative_literal_harvest=false
static_resume_all=true
codegen_shards=2
[identity]
sha1="7352d2bd064d9ebaec579e264228aa21c7345b80"
[[code_copy]]
runtime_start=0x03004D50
source_start=0x08040CE0
size=0x4B4
[[extra_func]]
addr=0x03004D50
mode="thumb"
name="morph_entry"
''')
subprocess.run([str((a.recompiler or root.parent/'gbarecomp/build/gba_recompile').resolve()),'--rom',str(a.rom.resolve()),'--config',str(config),'--out',str(out),'--output-prefix','morph_','--codegen-shards','2'],check=True)
for path in out.glob('morph_recompiled_*.cpp'):
 text=path.read_text();lines=[]
 for line in text.splitlines():
  if not line.lstrip().startswith(('case ','/*','//')):
   line=re.sub(r'0x([0-9a-fA-F]{8})u',lambda m: '('+m.group(0)+' + sma3_morph_delta)' if base<=int(m[1],16)<base+size+4 else m[0],line)
  line=line.replace('uint32_t _resume = g_runtime_resume_pc;', 'uint32_t _resume = g_runtime_resume_pc - sma3_morph_delta;')
  lines.append(line)
 path.write_text('#include <cstdint>\nextern thread_local uint32_t sma3_morph_delta;\n'+'\n'.join(lines)+'\n')
