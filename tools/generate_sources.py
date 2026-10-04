#!/usr/bin/env python3
"""Regenerate local native sources from the owner's verified ROM and BIOS."""
import argparse,hashlib,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--rom',type=Path,required=True)
p.add_argument('--bios',type=Path,required=True)
p.add_argument('--recompiler',type=Path,default=root.parent/'gbarecomp/build/gba_recompile')
a=p.parse_args()
for path,size,sha in [(a.rom,4194304,'7352d2bd064d9ebaec579e264228aa21c7345b80'),(a.bios,16384,'300c20df6731a33952ded8c436f7f186d25d3492')]:
 if not path.is_file() or path.stat().st_size!=size or hashlib.sha1(path.read_bytes()).hexdigest()!=sha:
  p.error(f'Unsupported or missing input: {path}')
exe=str(a.recompiler.resolve());rom=str(a.rom.resolve());bios=str(a.bios.resolve())
def run(*args):subprocess.run([str(x) for x in args],check=True,cwd=root)
run(exe,'--rom',rom,'--config',root/'game.toml','--symbols',root/'symbols.tsv','--out',root/'generated-project/generated','--codegen-shards','64')
run(exe,'--bios',bios,'--config',root/'bios.toml','--out',root/'generated-bios')
run(exe,'--rom',rom,'--config',root/'ram.toml','--out',root/'generated-ram','--output-prefix','sma3ram_','--codegen-shards','2')
run(sys.executable,root/'tools/generate_overlays.py','--rom',rom,'--recompiler',exe)
print('Generated sources are local build inputs and are ignored by Git.')
