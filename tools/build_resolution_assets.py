#!/usr/bin/env python3
"""Append private resolution labels without redistributing original game art.

Input tiles are raw BC3 blocks, each352x64, containing a348x60 label at0,0.
Order:1440p normal,1440p selected,2160p normal,2160p selected.
The UI investigator owns label rendering/style; this tool only packs supplied tiles.
"""
import sys
if sys.flags.optimize:
    raise SystemExit('Optimized Python execution is unsupported; safety assertions must remain enabled')

import argparse
import hashlib
import json
from pathlib import Path
import struct
from asset_format import validate


def build(texture, layout, tiles, output):
    g=bytearray(texture.read_bytes());h=bytearray(layout.read_bytes())
    validate(g,h)
    payload=bytearray(2048*2048);payload[:len(g)-56]=g[56:]
    for i,tile in enumerate(tiles):
        blocks=tile.read_bytes();assert len(blocks)==352*64
        for row in range(16):
            target=((1024//4+row)*(2048//4)+(i*352)//4)*16
            payload[target:target+88*16]=blocks[row*88*16:(row+1)*88*16]
        h.extend(struct.pack('<4I',i*352,1024,348,60))
    g=g[:56]+payload;g[38]=0xbb
    struct.pack_into('<I',g,8,len(g));struct.pack_into('<I',h,20,65)
    validate(g,h,expanded=True)
    output.mkdir(parents=True,exist_ok=True)
    (output/'UIOption.g1t').write_bytes(g)
    (output/'UIOptionLayout.bin').write_bytes(h)
    (output/'asset-manifest.json').write_text(json.dumps({
        'records':65,'width':2048,'height':2048,'original_records_unchanged':True,
        'original_texture_blocks_unchanged':True,
        'texture_sha256':hashlib.sha256(g).hexdigest(),
        'layout_sha256':hashlib.sha256(h).hexdigest(),
        'private_generated_assets':True},indent=2)+'\n')


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--texture',type=Path,required=True)
    p.add_argument('--layout',type=Path,required=True)
    p.add_argument('--tiles',type=Path,nargs=4,required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();build(a.texture,a.layout,a.tiles,a.output)
