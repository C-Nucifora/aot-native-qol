#!/usr/bin/env python3
"""Emit private, fixed-build guards into this research directory only."""
from pathlib import Path
import hashlib,struct,sys
if sys.flags.optimize:raise SystemExit("Optimized execution is unsupported")

EXPECTED='0a2d9e152db3bfe298017029804fe172d2a20bc51573feee874040ca3b1684d7'
def main():
 exe,output=map(Path,sys.argv[1:]);data=exe.read_bytes();output.parent.mkdir(parents=True,exist_ok=True)
 if hashlib.sha256(data).hexdigest()!=EXPECTED:raise RuntimeError('Unsupported research image')
 def read(rva,n):
  if rva<0x865000:raw=rva-0x1000+0x400
  elif rva<0xb8a000:raw=rva-0x865000+0x864200
  else:raise RuntimeError('Outside reviewed code/rdata')
  b=data[raw:raw+n]
  if len(b)!=n:raise RuntimeError('Short read')
  return b
 def array(b):return '{'+','.join(f'0x{x:02x}' for x in b)+'}'
 flow=[(0xddf24,47),(0x439c40,148),(0x439de0,150),(0x442600,128),(0xde6a0,72),(0xde760,68)]
 sites=[0xdcd3a,0xdcd48,0xdd0e8,0xdd472,0xde837,0xde843,0xde859,0xde899,0x2db0d2,0x2db3fa,0x2de5e1,0x2de679]
 declarations=['/* PRIVATE game guards. Do not publish or commit. */','#include "native_backend.h"']
 for i,(rva,length) in enumerate(flow):declarations.append(f'static const uint8_t fps_flow_{i}[{length}]={array(read(rva,length))};')
 fields=[('timing',0xde081,8),('clock',0xde04d,5),('timestamp',0xde06b,4),('wait_call',0xddf40,6),('simulation_cap',0xa8914c,4)]
 declarations.append('static const FpsPrivateGuards fps_private_guards={')
 for name,rva,length in fields:declarations.append(f' .{name}={array(read(rva,length))},')
 declarations.append(' .flow={'+','.join(f'{{0x{rva:x},{length},fps_flow_{i}}}' for i,(rva,length) in enumerate(flow))+'},')
 calls=[]
 for i,rva in enumerate(sites):
  b=read(rva,5);target=0xde760 if i in (0,4,7) else 0xde6a0
  if b[0]!=(0xe9 if i in (4,5) else 0xe8) or rva+5+struct.unpack_from('<i',b,1)[0]!=target:raise RuntimeError('Unexpected mode transfer')
  calls.append(f'{{0x{rva:x},{array(b)}}}')
 declarations.append(' .mode_calls={'+','.join(calls)+'}\n};\n')
 output.write_text('\n'.join(declarations));output.chmod(0o600)
 print(f'Private header generated:6 flow regions,12 mode transfers,13 combined hooks; SHA256 {hashlib.sha256(output.read_bytes()).hexdigest()}')
if __name__=='__main__':main()
