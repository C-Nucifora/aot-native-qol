#!/usr/bin/env python3
"""Generate supported original-numeral resolution sidecars locally only."""
import sys
if sys.flags.optimize:raise SystemExit('Optimized execution is unsupported')
sys.dont_write_bytecode=True
from pathlib import Path
import argparse,hashlib,json
import glyph_helpers as glyph
import resolution_donors
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import build_resolution_assets as packer
ORDER=['2560-normal.bc3','2560-selected.bc3','3840-normal.bc3','3840-selected.bc3']
TILE_SHA=['004c0df22b7c9788512cfb4fb1b3d759065ce852b4193f1640004c8325d77e22','83e400b2c15bb054f080a8c8a5c31b170844aa9cfd61f6623b70a4d5a94fb8fe','1a847173b50a4fc7d8d42a5761ed019223854026f0720e217480d5a4384b399e','ed8ce1252362ed7916db3db34f1969ccdc18002efeb7c024e6a47ef75d5d1560']
PACKED_SHA=['181ca09afe62d8c5165d1dc7f741e4099c82650fb29ea26f54c8decbd69078ce','a5eb6855538f9d2ea69513648f2b0aca893fd0574f0a2fac1d31f1537100bc3d']
def sha(b):return hashlib.sha256(b).hexdigest()
def load(path,cap):
 with path.open('rb') as f:data=f.read(cap+1)
 if len(data)>cap:raise ValueError('Oversized input')
 return data
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--archive',type=Path);p.add_argument('--texture',type=Path);p.add_argument('--layout',type=Path);p.add_argument('--donor-tiles',type=Path,nargs=4,help='Optional exact existing tiles in normal/selected 1440p then2160p order');p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 if bool(a.archive)==bool(a.texture or a.layout) or (not a.archive and not (a.texture and a.layout)):p.error('Supply --archive OR both --texture and --layout')
 root=Path(__file__).resolve().parents[2];out=a.output.resolve()
 if (out==root or root in out.parents) and not any(x in out.parents for x in [root/'build',root/'private']):raise ValueError('Output must be private under build/private or outside source tree')
 if out.exists():raise ValueError('Existing output directory refused')
 if a.archive:
  texture=glyph.read_slot(a.archive,1702);layout=glyph.read_slot(a.archive,1703)
 else:texture=load(a.texture,glyph.MAX_ASSET_BYTES);layout=load(a.layout,glyph.MAX_ASSET_BYTES)
 if sha(texture)!=glyph.EXPECTED['texture'] or sha(layout)!=glyph.EXPECTED['layout']:raise ValueError('Unsupported donor texture/layout identity')
 supplied=None
 if a.donor_tiles:
  supplied=[load(path,352*64) for path in a.donor_tiles]
  if [sha(data) for data in supplied]!=TILE_SHA:raise ValueError('Unsupported donor tile identity/order')
 # Everything externally supplied is validated before creating private outputs.
 out.mkdir(parents=True);inputs=out/'inputs';inputs.mkdir();tiles=out/'tiles';tiles.mkdir()
 tex=inputs/'UIOption.g1t';lay=inputs/'UIOptionLayout.bin';tex.write_bytes(texture);lay.write_bytes(layout)
 if supplied:
  for name,data in zip(ORDER,supplied):(tiles/name).write_bytes(data)
 else:resolution_donors.build(texture,layout,tiles)
 paths=[tiles/name for name in ORDER]
 if [sha(path.read_bytes()) for path in paths]!=TILE_SHA:raise ValueError('Generated tiles differ from accepted art; retain privately, do not install')
 packer.build(tex,lay,paths,out/'aot-native-assets')
 generated=[(out/'aot-native-assets'/name).read_bytes() for name in ['UIOption.g1t','UIOptionLayout.bin']]
 if list(map(sha,generated))!=PACKED_SHA:raise ValueError('Packed resolution asset mismatch; do not install')
 # Meaningful stock preservation checks against exact supported inputs.
 if generated[0][56:56+len(texture)-56]!=texture[56:] or generated[1][32:32+61*16]!=layout[32:]:raise ValueError('Original blocks/records changed')
 print(json.dumps({'texture_sha256':sha(generated[0]),'layout_sha256':sha(generated[1]),'tiles':dict(zip(ORDER,TILE_SHA)),'original61_records_and_blocks_preserved':True},indent=2))
if __name__=='__main__':
 try:main()
 except (OSError,ValueError,AssertionError) as e:raise SystemExit(str(e))
