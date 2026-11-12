#!/usr/bin/env python3
"""Local-only FPS build guards and generated-asset identity. No game files shipped."""
import sys
if sys.flags.optimize:raise SystemExit('Optimized execution is unsupported')
import argparse,hashlib,struct
from pathlib import Path
DIALOG_RVAS=[0x3cdf80,0x3cdfa0,0x3cdfb9,0x3cdfe4,0x3ce1b0,0x3ced80,0x3ce4b0,0x3ce4d1,0x37c887]
DIALOG_TARGETS=[0x3cece0,0x3ced80,0x345f40,0x3cdf80,0x3ce1b0,0x3ce190,0x3cdda0,0x3cdfa0,0x3ce020,0x3ce200]
RVAS=[3024835, 3653360, 3653520, 3653712, 3654992, 3655120, 3655456, 3656800, 4026480, 4026736, 4027056, 4027200, 4037744, 4038128, 4038784, 3629760, 3629504, 2998512, 3657088, 3657520, 3629952, 2555760, 3937232, 4031200, 4031242, 4031616, 4031648, 4031906, 4442448, 4442661, 8440164, 4029668]
TARGETS=[3653520, 4037744, 4038128, 4038240, 4038352, 4038480, 3653712, 3654992, 3655120, 3655456, 3047184, 4038336, 4038544, 4038608, 3629664, 3629728, 3154384, 3656800, 3656944, 3655856, 3655952, 3656032, 3656128, 3656480, 3656720, 11234912]
EXTRA=[0x3780a0,0x378846,0x418fb0,0x3e6996,0x3e6b73,0x3e6b7f,0x2df8c3,0x2e0e58,0x2df97d,0x3dfde0,0x2e0520]
def main():
 p=argparse.ArgumentParser();p.add_argument('exe',type=Path);p.add_argument('--assets',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 data=a.exe.read_bytes()
 if hashlib.sha256(data).hexdigest()!='0a2d9e152db3bfe298017029804fe172d2a20bc51573feee874040ca3b1684d7':raise SystemExit('Unsupported offline executable')
 pe=struct.unpack_from('<I',data,0x3c)[0];sections=[]
 if data[pe:pe+4]!=b'PE\0\0' or struct.unpack_from('<H',data,pe+4)[0]!=0x8664:raise SystemExit('Unsupported PE')
 for i in range(struct.unpack_from('<H',data,pe+6)[0]):
  off=pe+24+struct.unpack_from('<H',data,pe+20)[0]+40*i
  _,rva,size,raw=struct.unpack_from('<IIII',data,off+8);sections.append((rva,size,raw))
 def read(rva,n):
  for start,size,raw in sections:
   if start<=rva and rva+n<=start+size:return data[raw+rva-start:raw+rva-start+n]
  raise ValueError('Unbacked RVA')
 def initializer(xs):return ','.join('0x%02x'%x for x in xs)
 a.output.mkdir(parents=True,exist_ok=True)
 text='/* Generated privately from supported local executable. */\nstatic const struct {size_t rva;uint8_t bytes[16];} guards[]={\n'
 text+=''.join('{0x%x,{%s}},\n'%(r,initializer(read(r,16))) for r in RVAS+DIALOG_RVAS)+'};\n'
 text+='static const size_t menu_targets[26]={'+','.join(hex(t) for t in TARGETS)+'};\n'
 text+='static const size_t dialog_targets[10]={'+','.join(hex(t) for t in DIALOG_TARGETS)+'};\n'
 (a.output/'aot_fps_menu_guards.h').write_text(text)
 text='static const struct {uint32_t rva;uint8_t bytes[16];} fps_stage_guards[]={\n'
 text+=''.join('{0x%x,{%s}},\n'%(r,initializer(read(r,16))) for r in EXTRA)+'};\n'
 for name,var,sha in [('CmnParts.g1t','fps_texture_hash','46ab9e4274c50c382285a8579783d3149f124ed9b69264f02083fb93032ae45c'),('CmnPartsLayout.bin','fps_layout_hash','a8ffad74ab3d5e47a5e1e16490b2919deb8c9e04b132c67c59eb0c9944e728d5')]:
  raw=(a.assets/name).read_bytes();digest=hashlib.sha256(raw).digest()
  if digest.hex()!=sha:raise SystemExit('Unsupported private-v3-seven-values asset: '+name)
  text+='static const uint8_t '+var+'[32]={'+initializer(digest)+'};\n'
 (a.output/'fps_stage_guards.h').write_text(text)
 print('Private FPS guards generated, supported assets verified; no installation')
if __name__=='__main__':main()
