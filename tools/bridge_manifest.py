#!/usr/bin/env python3
"""Generate private startup call guards from the supported offline image."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
p=argparse.ArgumentParser();p.add_argument('exe',type=Path);p.add_argument('--output',type=Path,default=Path('generated'));a=p.parse_args()
b=a.exe.read_bytes()
if hashlib.sha256(b).hexdigest()!='0a2d9e152db3bfe298017029804fe172d2a20bc51573feee874040ca3b1684d7':raise SystemExit('Unsupported build')
pe=struct.unpack_from('<I',b,60)[0];opt=pe+24;sh=opt+struct.unpack_from('<H',b,pe+20)[0]
sections=[struct.unpack_from('<3I',b,sh+40*i+12) for i in range(struct.unpack_from('<H',b,pe+6)[0])]
def read(rva,n):
    for va,size,raw in sections:
        if va<=rva and rva+n<=va+size:return b[raw+rva-va:raw+rva-va+n]
    raise ValueError('Unbacked guard')
records=[]
for site,target in [(0x419ed4,0x41dca0),(0x41dd0e,0x41e180),(0x41dc73,0x41e080),(0x41e58b,0x419600),(0x37cbe3,0x41e080)]:
    raw=read(site,5)
    if raw[0]!=0xe8 or site+5+struct.unpack_from('<i',raw,1)[0]!=target:raise ValueError('Unexpected call')
    records.append(dict(rva=site,expected=raw.hex(),target=target))
preserved=[]
for rva,n in [(0x41dca0,16),(0x41e180,16),(0x41e080,16),(0x419600,16),
    (0xdd5b0,0xd9),(0xdd59e,3),(0xdea5e,3),(0x41e581,10),(0x4188d2,17),
    (0xa6c3a0,16),(0x8959b0,66)]:
    for off in range(0,n,16):preserved.append(dict(rva=rva+off,expected=read(rva+off,min(n-off,16)).hex()))
def format_guard(x):return '{0x%x,%d,{%s},{0}},'%(x['rva'],len(x['expected'])//2,','.join('0x'+x['expected'][i:i+2] for i in range(0,len(x['expected']),2)))
a.output.mkdir(parents=True,exist_ok=True)
(a.output/'bridge_guards.h').write_text('static const AotPatch bridge_calls[]={\n'+'\n'.join(map(format_guard,records))+'\n};\nstatic const AotPatch bridge_preserved[]={\n'+'\n'.join(map(format_guard,preserved))+'\n};\n')
(a.output/'bridge-manifest.json').write_text(json.dumps(dict(calls=records,preserved=preserved,installed=False),indent=2)+'\n')
