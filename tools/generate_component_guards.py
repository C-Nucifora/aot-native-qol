#!/usr/bin/env python3
"""Read exact validation/replay bytes from a supported local image only."""
import sys
if sys.flags.optimize:raise SystemExit('Optimized execution is unsupported')
from pathlib import Path
import hashlib,struct,json
image,spec=map(Path,sys.argv[1:]);output=spec.parent;data=image.read_bytes()
if hashlib.sha256(data).hexdigest()!='0a2d9e152db3bfe298017029804fe172d2a20bc51573feee874040ca3b1684d7':raise SystemExit('Unsupported local image')
pe=struct.unpack_from('<I',data,60)[0];sections=pe+24+struct.unpack_from('<H',data,pe+20)[0]
def read(rva,count):
 for i in range(struct.unpack_from('<H',data,pe+6)[0]):
  _,base,size,raw=struct.unpack_from('<IIII',data,sections+40*i+8)
  if base<=rva and rva+count<=base+size:
   result=data[raw+rva-base:raw+rva-base+count]
   if len(result)==count:return result
 raise SystemExit('Unbacked private image range')
specification=json.loads(spec.read_text())
text='/* PRIVATE local game validation bytes. Never distribute. */\n'
for item in specification.get('arrays',[]):
 text+='static const uint8_t '+item['name']+'[]={' + ','.join(hex(x) for x in read(item['rva'],item['size']))+'};\n'
for item in specification.get('macros',[]):
 text+='#define '+item['name']+' '+','.join(hex(x) for x in read(item['rva'],item['size']))+'\n'
(output/specification['output']).write_text(text)
