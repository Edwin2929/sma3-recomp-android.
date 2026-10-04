"""Import instruction-address annotations from verified sma3-disasm sources."""
from pathlib import Path
import re,sys
root=Path(sys.argv[1]);out=Path(sys.argv[2]);out.parent.mkdir(parents=True,exist_ok=True)
mode='arm';seeds={}
def visit(path):
 global mode
 pending=None
 for line in path.read_text().splitlines():
  code=line.split(';',1)[0].strip()
  if code in ('.arm','.thumb'):mode=code[1:];pending=None;continue
  inc=re.fullmatch(r'\.include "([^"]+)"',code)
  if inc:visit(root/inc[1]);pending=None;continue
  label=re.match(r'^([A-Za-z_][A-Za-z_0-9]*):\s*(.*)$',code)
  if label:pending=label[1];code=label[2]
  if not code:continue
  if code.startswith(('.', '@@')):pending=None;continue
  address=re.search(r';\s*(08[0-9A-Fa-f]{6})(?:\b|/)',line)
  if pending and address:
   addr=int(address[1],16)
   seeds.setdefault(addr,(mode,'sma3_'+pending))
  pending=None
visit(root/'sma3.asm')
out.write_text(''.join(f'0x{a:08X}\t{m}\t{n}\n' for a,(m,n) in sorted(seeds.items())))
print(f'Imported {len(seeds)} named instruction entry points')
