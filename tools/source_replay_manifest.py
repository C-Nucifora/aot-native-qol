#!/usr/bin/env python3
"""Read exact validation/replay bytes from a supported local image only."""
import sys
if sys.flags.optimize:raise SystemExit('Optimized execution is unsupported')
from pathlib import Path
import hashlib,struct
image,output=map(Path,sys.argv[1:]);data=image.read_bytes()
if hashlib.sha256(data).hexdigest()!='0a2d9e152db3bfe298017029804fe172d2a20bc51573feee874040ca3b1684d7':raise SystemExit('Unsupported local image')
pe=struct.unpack_from('<I',data,60)[0];sections=pe+24+struct.unpack_from('<H',data,pe+20)[0]
def read(rva,count):
 for i in range(struct.unpack_from('<H',data,pe+6)[0]):
  _,base,size,raw=struct.unpack_from('<IIII',data,sections+40*i+8)
  if base<=rva and rva+count<=base+size:
   result=data[raw+rva-base:raw+rva-base+count]
   if len(result)==count:return result
 raise SystemExit('Unbacked private image range')
fields=[('AOT_NAPE_SITE_BYTES',0x1960c,6),('AOT_UI_FIRST_BYTES',0x2dddc8,6),('AOT_UI_LATER_BYTES',0x2dddd8,6),('AOT_UI_REPLAY_BYTES',0x2dddc8,3),('AOT_FPS_CONSTRUCTOR_BYTES',0x2e27c3,5)]
text='/* PRIVATE local game validation/replay bytes. Never distribute. */\n'
for name,rva,count in fields:text+='#define '+name+' '+','.join(hex(x) for x in read(rva,count))+'\n'
output.mkdir(parents=True,exist_ok=True);(output/'source-replay-private.h').write_text(text)
