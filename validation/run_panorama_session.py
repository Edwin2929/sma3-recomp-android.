#!/usr/bin/env python3
"""Bounded native input session from a private savestate; never publish its output."""
import argparse,hashlib,json,os,socket,struct,subprocess,time
from pathlib import Path
from PIL import Image
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--rom',type=Path,required=True);p.add_argument('--bios',type=Path,required=True)
p.add_argument('--state',type=Path,required=True);p.add_argument('--actions',type=Path,required=True)
p.add_argument('--output',type=Path,required=True);p.add_argument('--width',type=int,default=356)
a=p.parse_args()
if not 240<=a.width<=356:p.error('width must be 240..356')
for path,sha in [(a.rom,'7352d2bd064d9ebaec579e264228aa21c7345b80'),(a.bios,'300c20df6731a33952ded8c436f7f186d25d3492')]:
 if hashlib.sha1(path.read_bytes()).hexdigest()!=sha:p.error('Unsupported asset')
actions=json.loads(a.actions.read_text())
if not isinstance(actions,list) or not actions or len(actions)>200:p.error('1..200 input actions required')
if any(set(x)!= {'n','keys'} or type(x['n']) is not int or not 1<=x['n']<=600 or type(x['keys']) is not int or not 0<=x['keys']<=1023 for x in actions):p.error('Each action needs n=1..600, keys=0..1023')
if sum(x['n'] for x in actions)>6000:p.error('Maximum 6000 frames per session')
a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
if any(a.output.iterdir()):p.error('Use a new empty output directory')
with socket.socket() as s:s.bind(('127.0.0.1',0));port=s.getsockname()[1]
def command(**args):
 with socket.create_connection(('127.0.0.1',port),timeout=45) as s:
  s.sendall((json.dumps(args)+'\n').encode());r=json.loads(s.makefile('rb').readline())
  if not r.get('ok'):raise RuntimeError(r)
  return r
env=os.environ.copy()
for name in ('GBARECOMP_INPUT_REPLAY','GBARECOMP_INPUT_RECORD','GBARECOMP_WS_WIP','GBARECOMP_WIDESCREEN'):env.pop(name,None)
env.update(GBARECOMP_STRICT_STATIC='1',GBARECOMP_FORCE_INTERP='0',GBARECOMP_BIOS_HLE='0',GBARECOMP_BIOS_SKIP_INTRO='1',SMA3_WIDE_PROBE_DIR=str(a.output),SMA3_AUTHORED_WIDE='1',SMA3_WIDE_OBJECT_BOUNDS='0')
args=[str(root/'build/sma3_runner'),'--config',str(root/'runtime.toml'),'--rom',str(a.rom.resolve()),'--bios',str(a.bios.resolve()),'--tcp',str(port),'--view-width',str(a.width),'--save-path',str(a.output/'fresh.sav')]
with (a.output/'run.log').open('w') as log:
 proc=subprocess.Popen(args,env=env,stdout=log,stderr=subprocess.STDOUT,cwd=root)
 try:
  for _ in range(50):
   try:command(cmd='ping');break
   except OSError:
    if proc.poll() is not None:raise RuntimeError('Runner exited; see run.log')
    time.sleep(.1)
  else:raise RuntimeError('Local debug server unavailable')
  command(cmd='savestate_load',path=str(a.state.resolve()))
  results=[]
  for i,action in enumerate(actions):
   r=command(cmd='run_frames',n=action['n'],keyinput=action['keys'])
   raw=bytes.fromhex(command(cmd='read_iwram',addr=0x03006d80,len=16)['data'])
   row=dict(action=action,frame=r['frame'],player_xy_velocity=[v/256 for v in struct.unpack('<iiii',raw)])
   ram=bytes.fromhex(command(cmd='read_iwram',addr=0x03000000,len=0x8000)['data'])
   row.update(level_id=struct.unpack_from('<H',ram,0x6288)[0],sublevel_id=struct.unpack_from('<H',ram,0x4cb8)[0],game_state=ram[0x6b05],transformation=struct.unpack_from('<H',ram,0x6db2)[0])
   results.append(row);print(json.dumps(row),flush=True)
   shot=command(cmd='screenshot')
   Image.frombytes('RGB',(shot['w'],shot['h']),bytes.fromhex(shot['data'])).save(a.output/f'step-{i:03}.png')
  command(cmd='savestate_save',path=str(a.output/'final.state'))
  (a.output/'session.json').write_text(json.dumps(results,indent=2)+'\n')
  command(cmd='quit');proc.wait(timeout=10)
 finally:
  if proc.poll() is None:
   proc.terminate()
   try:proc.wait(timeout=5)
   except subprocess.TimeoutExpired:proc.kill();proc.wait()
if proc.returncode:raise SystemExit('Runner failed; see run.log')
print('Saved bounded session; no full-game or Android qualification.')
