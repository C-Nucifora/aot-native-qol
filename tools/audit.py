#!/usr/bin/env python3
"""Generate private build inputs from a user's unpacked AOT executable."""
import sys
if sys.flags.optimize:
    raise SystemExit('Optimized Python execution is unsupported; safety assertions must remain enabled')

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess


def audit(exe, output):
    data = exe.read_bytes()
    assert hashlib.sha256(data).hexdigest() == "0a2d9e152db3bfe298017029804fe172d2a20bc51573feee874040ca3b1684d7", "Unsupported offline executable; do not use a packed or modified copy"
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    assert data[pe:pe+4] == b'PE\0\0'
    assert struct.unpack_from('<H', data, pe+4)[0] == 0x8664
    section_count = struct.unpack_from('<H', data, pe+6)[0]
    optional_size = struct.unpack_from('<H', data, pe+20)[0]
    image_base = struct.unpack_from('<Q', data, pe+48)[0]
    sections = []
    for i in range(section_count):
        p = pe+24+optional_size+i*40
        virtual_size, rva, raw_size, raw = struct.unpack_from('<4I', data, p+8)
        sections.append((rva, max(virtual_size, raw_size), raw, raw_size))

    def read(va, size):
        wanted = va-image_base
        for rva, length, raw, raw_size in sections:
            if rva <= wanted and wanted+size <= rva+raw_size:
                return data[raw+wanted-rva:raw+wanted-rva+size]
        raise ValueError(f'Address outside backed PE sections: {va:x}')

    dis = subprocess.check_output(['objdump', '-d', '-Mintel', '--insn-width=16', str(exe)], text=True)
    references = []
    capability = []
    for line in dis.splitlines():
        match = re.match(r'\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} )+[0-9a-f]{2})\s+(.+)', line)
        if not match:
            continue
        va = int(match[1], 16)
        instruction = match[3]
        target = re.search(r'# 0x([0-9a-f]+)', instruction)
        if target and int(target[1], 16) in (image_base+0x8959b0, image_base+0x8959b2, image_base+0x8959b4):
            raw = bytes.fromhex(match[2])
            assert instruction.startswith('lea '), (hex(va), instruction)
            assert len(raw) == 7 and raw[1] == 0x8d and raw[2] & 0xc7 == 0x05
            assert read(va, 7) == raw
            destination = int(target[1], 16)
            assert va+7+struct.unpack_from('<i', raw, 3)[0] == destination
            references.append({'rva': va-image_base, 'expected': raw.hex(),
                               'displacement_offset': 3, 'length': 7,
                               'target_offset': destination-image_base-0x8959b0})
        # Include whole-entry and interior forms and encompassing-context forms.
        if re.search(r'0x(?:140)?c9e2[789ab][0-9a-f]\b', instruction):
            capability.append({'rva': va-image_base, 'instruction': instruction,
                               'expected_prefix': match[2].replace(' ', '')})
    counts = {offset: sum(r['target_offset'] == offset for r in references) for offset in (0, 2, 4)}
    assert counts == {0: 45, 2: 1, 4: 2}, counts
    original = [struct.unpack('<3H', read(image_base+0x8959b0+6*i, 6)) for i in range(11)]
    assert original[3] == (1280, 720, 720) and original[9] == (1920, 1080, 1080)
    expected = [(640,360,480),(800,450,600),(1024,576,768),(1280,720,720),
                (1280,720,768),(1280,720,960),(1280,720,1024),(1600,900,900),
                (1600,900,1200),(1920,1080,1080),(1920,1080,1200)]
    assert original == expected
    report = {'sha256': hashlib.sha256(data).hexdigest(), 'image_base': image_base,
              'approved_for_install': False, 'resolution_leas': references,
              'resolution_counts': counts, 'capability_candidates': capability,
              'capability_base_rva': 0xc9e274, 'capability_stride': 8,
              'capability_neighbor_rva': 0xc9e2cc, 'default_resolution_index': 9,
              'blocking_audits': ['capability producers/context aliases', 'resolution bounds classification',
                                  'menu lifecycle implementation and review', 'final Apply/recovery FPS persistence',
                                  'render and gameplay equivalence live validation']}
    output.mkdir(parents=True, exist_ok=True)
    # Local report may contain private instruction text. Never commit generated/.
    (output/'audit.json').write_text(json.dumps(report, indent=2)+'\n')
    guards = '\n'.join('{0x%x,7,{%s},{0}},' % (r['rva'], ','.join('0x'+r['expected'][i:i+2] for i in range(0,14,2))) for r in references)
    (output/'guards.h').write_text('static const AotPatch resolution_guards[] = {\n'+guards+'\n};\n')
    from resolution_manifest import generate
    generate(data, read, dis, references, output)
    print(f'Validated {len(references)} resolution LEAs: {counts}; install approval remains false.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('exe', type=Path)
    parser.add_argument('--output', type=Path, default=Path('generated'))
    args = parser.parse_args()
    audit(args.exe, args.output)
