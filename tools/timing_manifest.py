#!/usr/bin/env python3
"""Generate private timing guards from the exact supported local research input."""
import sys
if sys.flags.optimize:raise SystemExit('Optimized execution is unsupported')
from pathlib import Path
import hashlib,struct,argparse
p=argparse.ArgumentParser();p.add_argument('exe',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
b=a.exe.read_bytes()
if hashlib.sha256(b).hexdigest()!='0a2d9e152db3bfe298017029804fe172d2a20bc51573feee874040ca3b1684d7':raise SystemExit('Unsupported research image')
pe=struct.unpack_from('<I',b,60)[0];sections=[]
for i in range(struct.unpack_from('<H',b,pe+6)[0]):
 q=pe+24+struct.unpack_from('<H',b,pe+20)[0]+40*i
 v,n,r=struct.unpack_from('<III',b,q+12);sections.append((v,n,r))
def read(at,size):
 for v,n,r in sections:
  if v<=at and at+size<=v+n:return b[r+at-v:r+at-v+size]
 raise SystemExit('Unbacked timing guard')
def init(data):return ','.join('0x%02x'%x for x in data)
a.output.mkdir(parents=True,exist_ok=True)
s='/* Private original-image bytes, never public source. */\nstatic const CameraPrivateGuards camera_private_guards={\n'
for name,rva,size in [('yaw',0xf453a,32),('pitch',0xf463b,32),('sensitivity',0x1101a0,16)]:s+='.'+name+'={'+init(read(rva,size))+'},\n'
(a.output/'camera-private-guards.h').write_text(s+'};\n')
s='typedef struct {unsigned rva,length;uint8_t bytes[128];} RepeatPrivateGuard;\nstatic const RepeatPrivateGuard repeat_guards[]={\n'
for rva,size in [(0x3f303d,13),(0x3f2fee,22),(0x3f2d12,47),(0x3f3902,98),(0xde03d,46)]:s+='{0x%x,%d,{%s}},\n'%(rva,size,init(read(rva,size)))
(a.output/'repeat-private-guards.h').write_text(s+'};\n')
print('Private keyboard/controller/camera guard spans generated')

s='/* Private original-image UI-root guards. */\nstatic const UiClockPrivateGuards ui_clock_private_guards={.roots={\n'
for rva in (0x2dddb6,0x2dddce,0x2dddde):s+='{'+init(read(rva,32))+'},\n'
s+='},.divide={'+init(read(0x2dddbd,16))+'},.clock={'+init(read(0xde0d6,32))+'}};\n'
(a.output/'ui-clock-private-guards.h').write_text(s)
