"""Private instruction manifest for the supported original-layout executable."""
import sys
if sys.flags.optimize:
    raise SystemExit('Optimized Python execution is unsupported; safety assertions must remain enabled')

import json
import re
import struct

BASE = 0x140000000
CONTEXT = 0xc9d9c0
CAPABILITY = 0xc9e274
# Reviewed register clamps and capability iteration bounds. These are instruction
# starts, not a search-and-replace of every immediate11 in the executable.
REGISTER_BOUNDS = {
    0xddd2b, 0x1a115e, 0x418d3f, 0x418e0d, 0x418e17,
    0x41976a, 0x4197ba, 0x41b0b4, 0x41d706, 0x41d70e,
}
CAPABILITY_LOOPS = {
    0x37cab2, 0x37cadb, 0x418dc2, 0x418df1, 0x418e93,
    0x418ec2, 0x418f75, 0x41a372, 0x41a50d, 0x41d762,
}
# All stores through encompassing singleton context + index*8, including clears.
CONTEXT_REFERENCES = {
    0x41a34f: (3, 2), 0x41a4c0: (3, 2),
    0x41a3ff: (4, 4), 0x41a407: (4, 0),
    0x41a426: (5, 0), 0x41a434: (5, 2), 0x41a43d: (4, 4),
}


def generate(data, read, disassembly, resolution, output):
    instructions = {}
    for line in disassembly.splitlines():
        m = re.match(r'\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} )+[0-9a-f]{2})\s+(.+)', line)
        if m:
            rva = int(m[1], 16)-BASE
            raw = bytes.fromhex(m[2])
            assert read(BASE+rva, len(raw)) == raw
            instructions[rva] = (raw, m[3])
    refs = [dict(r, table=0, context_rva=0) for r in resolution]
    fixed = []
    def reference(rva, offset, interior, context=0):
        raw, text = instructions[rva]
        old_origin = 0 if context == 0xffffffff else context if context else rva+len(raw)
        assert old_origin+struct.unpack_from('<i', raw, offset)[0] == CAPABILITY+interior
        refs.append(dict(rva=rva, expected=raw.hex(), length=len(raw),
                         displacement_offset=offset, target_offset=interior,
                         context_rva=context, table=1, semantic=text))
    direct = []
    for rva, (raw, text) in instructions.items():
        target = re.search(r'# 0x([0-9a-f]+)', text)
        if target and BASE+CAPABILITY <= int(target[1], 16) < BASE+CAPABILITY+88:
            assert text.startswith('lea '), (hex(rva), text)
            assert len(raw) == 7
            reference(rva, 3, int(target[1], 16)-BASE-CAPABILITY)
            direct.append(rva)
    assert set(direct) == {0x37c9ea, 0x418d6e, 0x418e98, 0x41d6c4}
    reference(0x37cea2, 3, 4, 0xffffffff)
    for rva, (offset, interior) in CONTEXT_REFERENCES.items():
        reference(rva, offset, interior, CONTEXT)
    global_bounds = set()
    for rva, (raw, text) in instructions.items():
        if re.fullmatch(r'cmp\s+DWORD PTR \[rip\+0x[0-9a-f]+\],0xb\s+# 0x140c48d84', text):
            global_bounds.add(rva)
    assert len(global_bounds) == 48, len(global_bounds)
    for rva in sorted(global_bounds | REGISTER_BOUNDS | CAPABILITY_LOOPS):
        raw, text = instructions[rva]
        assert text.startswith('cmp ') and raw[-1] == 11
        fixed.append(dict(rva=rva, expected=raw.hex(), replacement=(raw[:-1]+b'\x0d').hex(),
                          semantic='resolution index clamp' if rva in global_bounds | REGISTER_BOUNDS else 'capability entry iteration'))
    for rva, old, new, semantic in [
        (0x37cd6f, b'\xb8\x0b\x00\x00\x00', b'\xb8\x0d\x00\x00\x00', 'resolution value count'),
        (0x37cdd8, b'\xba\x0a\x00\x00\x00', b'\xba\x0c\x00\x00\x00', 'resolution input maximum'),
        (0xa6c3a4, struct.pack('<I',11), struct.pack('<I',13), 'saved descriptor exclusive bound'),
    ]:
        assert read(BASE+rva,len(old)) == old
        fixed.append(dict(rva=rva,expected=old.hex(),replacement=new.hex(),semantic=semantic))
    # Preserve default preference9 and descending fallback max10 at all3 sites.
    preserve = [(0x37ca3f,6),(0x418df8,6),(0x41d6f1,5),(0x4198e7,7)]
    report = dict(references=refs, fixed=fixed,
                  preserved=[dict(rva=r,expected=read(BASE+r,n).hex()) for r,n in preserve],
                  global_clamp_count=len(global_bounds), activation_approved=False)
    (output/'resolution-manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    def array(hexstr):return '{'+','.join('0x'+hexstr[i:i+2] for i in range(0,len(hexstr),2))+'}'
    def patch(p):return '{0x%x,%d,%s,%s}'%(p['rva'],len(p['expected'])//2,array(p['expected']),array(p.get('replacement','')))
    lines=['static const AotTableReference native_table_references[] = {']
    for r in refs:
        lines.append('{%s,%d,%d,%d,0x%x},'%(patch(r),r['table'],r['displacement_offset'],r['target_offset'],r['context_rva']))
    lines+=['};','static const AotPatch native_fixed_patches[] = {']
    lines.extend(patch(p)+',' for p in fixed)
    lines+=['};','static const AotPatch native_preserved_guards[] = {']
    lines.extend(patch(p)+',' for p in report['preserved'])
    lines+=['};']
    (output/'resolution_manifest.h').write_text('\n'.join(lines)+'\n')
    asset_calls=[]
    for rva in (0x3e6982,0x3e69d1):
        raw,text=instructions[rva]
        assert raw[0]==0xe8 and len(raw)==5
        assert rva+5+struct.unpack_from('<i',raw,1)[0]==0x40ffd0
        asset_calls.append(dict(rva=rva,expected=raw.hex()))
    (output/'asset_guards.h').write_text('static const AotPatch asset_call_guards[] = {\n'+
        '\n'.join(patch(p)+',' for p in asset_calls)+'\n};\n')

    asset_preserved=[dict(rva=r,expected=read(BASE+r,n).hex()) for r,n in (
        (0x40ffd0,14),(0x410023,7),(0x41003e,4),(0x410058,4),
        (0x3e6996,5),(0x3e69b8,3),(0x3e6b1b,3),
        (0x3e63f4,9*4),(0x3e6454,9*4))]
    # Jump tables are guards only and may exceed patch payload size; split them.
    for p in asset_preserved:
        if len(p['expected'])>32:
            raw=p['expected'];rva=p['rva'];p['expected']=raw[:32]
            asset_preserved.extend(dict(rva=rva+off//2,expected=raw[off:off+32])
                                   for off in range(32,len(raw),32))
    with (output/'asset_guards.h').open('a') as f:
        f.write('static const AotPatch asset_preserved_guards[] = {\n'+
                '\n'.join(patch(p)+',' for p in asset_preserved)+'\n};\n')
    print(f'Resolution manifest: {len(refs)} table operands, {len(fixed)} classified bounds; default guards preserved.')
