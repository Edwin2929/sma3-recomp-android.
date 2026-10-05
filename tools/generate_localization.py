#!/usr/bin/env python3
"""Build private native localization data, validating the exact SMA3 USA image.
The game reads translated message packets through a read-only virtual ROM area.
Only verified ROM pointer slots are redirected; executable bytes are untouched.
"""
from pathlib import Path
import argparse,re,json,hashlib,struct
p=argparse.ArgumentParser();p.add_argument('--rom',required=True,type=Path);p.add_argument('--disasm',required=True,type=Path);a=p.parse_args()
r=Path(__file__).resolve().parents[1];rom=a.rom.read_bytes()
assert hashlib.sha1(rom).hexdigest()=='7352d2bd064d9ebaec579e264228aa21c7345b80'
table={}
for line in (a.disasm/'sma3char.tbl').read_text().splitlines():
 if '=' in line:
  k,v=line.split('=',1);table[v]=bytes.fromhex(k)
table.update({'ã':b'\x60','õ':b'\x61','Ã':b'\x62','Õ':b'\x63'})
# Full-stop/comma glyphs with the spacing expected by this game's renderer.
table['.']=table[r'\{. }'];table[',']=table[r'\{, }']
tokens=sorted(table,key=len,reverse=True)
def encode(s):
 out=bytearray()
 while s:
  k=next((k for k in tokens if s.startswith(k)),None)
  if k is None:raise ValueError('Unsupported character '+repr(s[:20]))
  out+=table[k];s=s[len(k):]
 return bytes(out)
def packet_asm(chunk):
 out=bytearray()
 for line in chunk.splitlines()[1:]:
  if not line.lstrip().startswith(('.strn','.d8')):continue
  for match in re.finditer(r'"([^"]*)"|(0x[0-9a-fA-F]+)',line.split(';')[0]):
   out+=encode(match[1]) if match[1] is not None else bytes([int(match[2],16)])
 return bytes(out)
def blocks(path):
 s=path.read_text();out={}
 for m in re.finditer(r'^(\w+):[^\n]*; ([0-9A-F]{8})\n(.*?)(?=^\w+:|\Z)',s,re.M|re.S):
  if '.strn' in m[3]:out[m[1]]=(int(m[2],16),m[0])
 return out
sources={}
for f in ['Text/StandardMessages.asm','Text/LevelNames.asm','Text/StoryIntro.asm','Data.asm','Data2.asm']:sources.update(blocks(a.disasm/'asm'/f))
widths=rom[0x2f62cc:0x2f63cc]
def width(s):return sum(widths[c] for c in encode(s))
def wrap(s,maxwidth=136):
 lines=[];current=''
 for word in s.split():
  if width(word)>maxwidth:raise ValueError('Word too wide: '+word)
  candidate=(current+' '+word).strip()
  if current and width(candidate)>maxwidth:lines.append(current);current=word
  else:current=candidate
 if current:lines.append(current)
 return lines
records={};metadata=[]
def add(name,payloads,category):
 addr,original=sources[name];expected=packet_asm(original)
 assert rom[addr-0x08000000:addr-0x08000000+len(expected)]==expected, name+' source mismatch'
 records[addr]=payloads;metadata.append({'label':name,'address':hex(addr),'category':category,'bytes':[len(x) for x in payloads]})
def standard(s):
 lines=wrap(s);out=bytearray()
 while len(lines)<4:lines.append('')
 for i,line in enumerate(lines):
  out+=bytes([255,5+i if i<4 else 14])+encode(line)
  if i>=4:
   if (i-4)%4==0:out+=bytes([255,10])
   out+=bytes([255,18])*8
 out+=bytes([255,15,255,255]);return bytes(out)
for line in (r/'localization/messages.tsv').read_text().splitlines():
 key,es,pt=line.split('\t');add('Message_'+key,[standard(es),standard(pt)],'messages')
special={
 'FlipCardsExit':[['',' \\{triright} Seguir','    Salir',''],['',' \\{triright} Seguir','    Sair','']],
 'TryAgain':[['','¿Reintentar','este nivel?',' \\{triright} Sí     No'],['','Tentar esta','fase de novo?',' \\{triright} Sim    Não']],
 'RestartFromMiddleRing':[['','¿Volver al','anillo intermedio?',' \\{triright} Sí     No'],['','Voltar ao','anel do meio?',' \\{triright} Sim    Não']],
 'GameOver':[['¿Quieres','continuar?','  Sí     No'],['Quer','continuar?','  Sim    Não']],
 '1_3_1':[['Hay dos modos','para lanzar','huevos.','¿Quieres cambiar?','','','\\{triright} No    Sí',''],['Há dois modos','para lançar','ovos.','Quer mudar?','','','\\{triright} Não   Sim','']]
}
for key,languages in special.items():
 name='Message_'+key;original=sources[name][1];original_strings=re.findall(r'"(.*?)"',original);payloads=[]
 for translated in languages:
  if key in ['TryAgain','RestartFromMiddleRing']: translated=translated+['']
  assert len(translated)==len(original_strings),(key,len(translated),len(original_strings))
  idx=[0]
  def replace(m):
   i=idx[0];idx[0]+=1
   # Preserve all command bytes, including menu behavior and lives counter.
   old=m[1];commands=re.findall(r'@\{.*?\}',old)
   if key in ['TryAgain','RestartFromMiddleRing'] and i==0:return m[0]
   prefix='';suffix=''
   if commands:prefix=commands[0];suffix=''.join(commands[1:])
   return '"'+prefix+translated[i]+suffix+'"'
  changed=re.sub(r'"(.*?)"',replace,original);payloads.append(packet_asm(changed))
 add(name,payloads,'choices')
for filename,prefix,kind in [('levels.tsv','LevelName','levels'),('story.tsv','StoryIntroText','story')]:
 for line in (r/'localization'/filename).read_text().splitlines():
  key,es,pt=line.split('\t');name=prefix+('_Intro' if kind=='levels' and key=='Intro' else key)
  payloads=[]
  for s in [es,pt]:
   lines=s.split('|');assert len(lines)<=2
   if kind=='levels':
    for line in lines:assert width(line)<=208,(name,line,width(line))
    out=b''.join(bytes([254,i*16,0])+encode(t) for i,t in enumerate(lines))+b'\xfd'
   else:
    # Keep original top/bottom row and scale effects; recenter translated text.
    orig=sources[name][1];ys=[int(x,16) for x in re.findall(r'0xFF,0x02,0x([0-9A-Fa-f]+)',orig)]
    out=b''
    for i,t in enumerate(lines):
     assert width(t)<=224,(name,t,width(t))
     out+=bytes([255,2,ys[min(i,len(ys)-1)],255,3,max(0,(240-width(t))//2)])+encode(t)
    out+=b'\xff\xff'
   payloads.append(out)
  add(name,payloads,kind)
# Text-based file management prompts. Fixed answer columns remain unchanged.
files=[('0','¿QUÉ ARCHIVO?','QUAL ARQUIVO?'),('1','¿CUÁL COPIAR?','QUAL COPIAR?'),('2','SIN PARTIDAS','SEM PARTIDAS'),('3','COPIA COMPLETA','CÓPIA CONCLUÍDA'),('4','¿DÓNDE COPIAR?','COPIAR PARA ONDE?'),('5','NO HAY ESPACIO','SEM ESPAÇO'),('6','¿CUÁL BORRAR?','QUAL APAGAR?'),('8','¿BORRO? SÍ    NO','APAGAR? SIM   NÃO'),('9','ARCHIVO A BORRADO','ARQUIVO A APAGADO'),('A','ARCHIVO B BORRADO','ARQUIVO B APAGADO'),('B','ARCHIVO C BORRADO','ARQUIVO C APAGADO')]
for key,es,pt in files:
 name='FileSelectText'+key;orig=packet_asm(sources[name][1]);add(name,[orig[:3]+encode(s)+b'\xff\xff' for s in [es,pt]],'file_menu')
endings=['Gracias al trabajo en equipo de los Yoshis, los hermanos vuelven a estar juntos. Yoshi libera a la cigüeña, que retoma su viaje y por fin hace la entrega. ¡Gracias, Yoshi! ¡Los hermanos pronto verán a sus padres!','Graças ao trabalho em equipe dos Yoshis, os irmãos estão juntos de novo. Yoshi liberta a cegonha, que retoma a viagem e finalmente faz a entrega. Obrigado, Yoshi! Os irmãos logo vão conhecer seus pais!']
add('EndingText',[b'\xff\x0a'.join(encode(line) for line in wrap(t,208))+b'\xff\xff' for t in endings],'ending')
# Retain two distinct virtual regions, so changing language never mixes a packet.
blobs=[bytearray(),bytearray()];targets={}
for address,payloads in sorted(records.items()):
 targets[address]=[]
 for lang,payload in enumerate(payloads):
  targets[address].append(0x09E00000+lang*0x100000+len(blobs[lang]));blobs[lang]+=payload
assert all(len(b)<0x100000 for b in blobs)
slots=[]
for offset in range(0,len(rom)-3,4):
 pointer=struct.unpack_from('<I',rom,offset)[0]
 if pointer in targets:slots.append((0x08000000+offset,pointer,*targets[pointer]))
missing=[hex(addr) for addr in records if not any(s[1]==addr for s in slots)]
assert not missing,missing
# Four unused glyphs: ã õ Ã Õ. Original source rows are checked and preserved.
font=[]
for code,source in zip(range(0x60,0x64),[0xD8,0xE6,0xAA,0xB8]):
 assert not any(rom[0x2f63cc+code*12:0x2f63cc+(code+1)*12])
 glyph=bytearray(rom[0x2f63cc+source*12:0x2f63cc+(source+1)*12]);glyph[0]=0x36;glyph[1]=0x6c;font.extend(glyph)
out=r/'src/localization_data.h'
s='// Generated by tools/generate_localization.py; private game-specific data.\n#pragma once\n#include <cstdint>\nnamespace sma3 {\n'
for name,data in [('spanish_text',blobs[0]),('portuguese_text',blobs[1]),('portuguese_glyphs',font)]:
 s+='inline constexpr uint8_t '+name+'[] = {\n'
 for i in range(0,len(data),32):s+=','.join(str(x) for x in data[i:i+32])+',\n'
 s+='};\n'
s+='struct TextPointer { uint32_t address, original, spanish, portuguese; };\ninline constexpr TextPointer text_pointers[] = {\n'+''.join('{'+','.join(hex(x) for x in item)+'},\n' for item in slots)+'};\n}\n';out.write_text(s)
report={'records':metadata,'pointer_slots':len(slots),'bytes':[len(x) for x in blobs],'untranslated':['Graphical title and pause-menu labels','Credits role labels and text','Mario Bros bonus game'],'rom_sha1':hashlib.sha1(rom).hexdigest()}
(r/'localization/coverage.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
print('Generated',len(records),'records per language;',len(slots),'verified pointer slots;',report['bytes'],'bytes')
