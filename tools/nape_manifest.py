#!/usr/bin/env python3
import sys
if sys.flags.optimize: raise SystemExit('Optimized execution is unsupported')
from pathlib import Path
import hashlib,struct
p,out=map(Path,sys.argv[1:]);data=p.read_bytes()
if hashlib.sha256(data).hexdigest()!='0a2d9e152db3bfe298017029804fe172d2a20bc51573feee874040ca3b1684d7': raise SystemExit('Unsupported offline image')
pe=struct.unpack_from('<I',data,0x3c)[0];sec=pe+24+struct.unpack_from('<H',data,pe+20)[0]
def read(rva,n):
 for i in range(struct.unpack_from('<H',data,pe+6)[0]):
  _,start,size,raw=struct.unpack_from('<IIII',data,sec+40*i+8)
  if start<=rva and rva+n<=start+size:return data[raw+rva-start:raw+rva-start+n]
 raise SystemExit('Unbacked RVA')
site=read(0x1960c,6)
if hashlib.sha256(site).hexdigest()!='678fddc308af17ac1c3954abed3cd5aa7ae0813c0c76a27c6471ac5e1f48c087':raise SystemExit('Unsupported gate')
ranges=[(0x19420,0x263),(0xe9790,0x28),(0xe9817,0x41),(0xe9e4d,0x13),(0x10ddf3,0x22),(0x10e180,0x17b),(0x17700,0x1f5),(0xa89014,4)]
text='/* Generated locally; excluded from public source. */\ntypedef struct {unsigned rva,length;const uint8_t *bytes;} NapePrivateGuard;\n'
for i,(r,n) in enumerate(ranges):text+=f'static const uint8_t nape_g{i}[]={{'+','.join(hex(x) for x in read(r,n))+'};\n'
text+='static const NapePrivateGuard nape_guards[]={'+','.join('{'+f'{r},{n},nape_g{i}'+'}' for i,(r,n) in enumerate(ranges))+'};\n'
text+='static const uint8_t nape_site[6]={'+','.join(hex(x) for x in site)+'};\n'
out.mkdir(exist_ok=True,parents=True);(out/'nape-private-guards.h').write_text(text)
