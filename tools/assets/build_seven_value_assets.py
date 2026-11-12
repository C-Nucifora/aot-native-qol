#!/usr/bin/env python3
"""Stage six exact existing values plus original-glyph Unlimited, target sentinel0.
All generated artwork remains private. Never installs files or changes timing.
"""
import argparse,base64,hashlib,importlib.util,io,json,struct,sys
from pathlib import Path
from PIL import Image
if sys.flags.optimize:raise SystemExit('Integrity assertions must remain enabled')
sys.dont_write_bytecode=True
HERE=Path(__file__).parent
spec=importlib.util.spec_from_file_location('six',HERE/'build_six_value_assets.py');six=importlib.util.module_from_spec(spec);spec.loader.exec_module(six)
CELLS={'U':(200,120),'n':(560,160),'l':(480,160),'i':(360,160),'m':(520,160),'t':(160,200),'e':(200,160),'d':(160,160)}
def sha(b):return hashlib.sha256(b).hexdigest()
def build(sources,out):
 r=six.build(sources,out);g=bytearray((out/'CmnParts.g1t').read_bytes());h=bytearray((out/'CmnPartsLayout.bin').read_bytes());previous_g=bytes(g);previous_h=bytes(h)
 font=sources['Font.g1t'];assert sha(font)==six.fps.EXPECTED['Font.g1t']
 a=six.fps.decode(font).getchannel('A');alpha=Image.new('L',(192,60));cursor=1;donors=[]
 for c in 'Unlimited':
  x,y=CELLS[c];cell=a.crop((x,y,x+40,y+40));bounds=cell.getbbox();assert bounds
  # Tight placement retains every original glyph pixel, without resampling or
  # overlap. It fits190 pixels in192, but leaves little focused-halo margin.
  glyph=cell.crop((bounds[0],0,bounds[2],40));alpha.paste(glyph,(cursor,11));donors.append(dict(glyph=c,cell=[x,y,x+40,y+40],ink_bbox=list(bounds),destination=[cursor,11],width=glyph.width));cursor+=glyph.width
 assert cursor==191 and alpha.getbbox()==(1,15,191,47)
 normal=six.fps.colored(alpha,(178,166,142));states=[six.fps.colored(alpha,(126,108,72)),normal,six.fps.focused(normal)]
 for state,(name,im) in enumerate(zip(six.STATES,states)):
  raw,decoded,checks=six.fps.encode(im,six.refine_value_alpha);filename='unlimited-'+name;(out/(filename+'.bc3')).write_bytes(raw);im.save(out/(filename+'-working.png'));decoded.save(out/(filename+'-roundtrip.png'))
  j=18+state;x=(j%10)*192;y=2048+(j//10)*64;rect=[x,y,192,60]
  for row in range(16):
   at=56+((y//4+row)*512+x//4)*16;g[at:at+768]=raw[row*768:(row+1)*768]
  h.extend(struct.pack('<4I',*rect));r['tiles'].append(dict(target=0,display_label='Unlimited',state=state,state_name=name,record=255+j,rect=rect,file=filename+'.bc3',sha256=sha(raw),bytes=len(raw),**checks))
 struct.pack_into('<I',h,20,276);assert len(h)==32+276*16
 assert g[:56+2048*2048]==previous_g[:56+2048*2048];assert h[32:32+273*16]==previous_h[32:]
 # Stronger than stock preservation: all18 prior six-value tiles stay identical.
 for tile in r['tiles']:
  x,y,w,_=tile['rect'];raw=(out/tile['file']).read_bytes();assert struct.unpack_from('<4I',h,32+tile['record']*16)==tuple(tile['rect'])
  for row in range(16):
   at=56+((y//4+row)*512+x//4)*16;assert g[at:at+768]==raw[row*768:(row+1)*768]
 (out/'CmnParts.g1t').write_bytes(g);(out/'CmnPartsLayout.bin').write_bytes(h)
 r.update(stage='private offline seven-target value art; sentinel0 denotes intended unbounded backend',targets=[60,90,120,144,165,240,0],display_labels=['60','90','120','144','165','240','Unlimited'],new_record_count=276,texture_sha256=sha(g),layout_sha256=sha(h),prior_six_value_tiles_preserved=True,unlimited_font_source_sha256=sha(font),unlimited_glyph_donors=donors,unlimited_geometry={'alpha_ink_bbox':[1,15,191,47],'ink_width':190,'source_width':192,'glyph_pixels_resampled':False,'glyph_pixels_overlapped':False,'spacing':'tight original ink bounds, no added tracking','focused_halo':'approximate; side halo clipped by192px source geometry'})
 r['limits'].append('Unlimited preserves all glyph pixels but uses tight190px ink placement; focus halo reaches/clips at192px source edges. It is a reviewable candidate, not proof of original native kerning.')
 (out/'manifest.json').write_text(json.dumps(r,indent=2)+'\n')
 panels=[]
 for target,label in zip(r['targets'],r['display_labels']):
  row=[]
  for name in r['state_order']:
   filename=('unlimited' if target==0 else str(target))+'-'+name;im=Image.open(out/(filename+'-roundtrip.png'));bg=Image.new('RGBA',im.size,(28,19,6,255));bg.alpha_composite(im);buf=io.BytesIO();bg.convert('RGB').resize((384,128),Image.Resampling.NEAREST).save(buf,format='PNG');row.append('<figure><figcaption>'+name+'</figcaption><img width="384" height="128" src="data:image/png;base64,'+base64.b64encode(buf.getvalue()).decode()+'"></figure>')
  panels.append('<h2>'+label+'</h2><div>'+''.join(row)+'</div>')
 (out/'review.html').write_text('<!doctype html><meta charset="utf-8"><title>Seven nativeFPS values</title><style>body{background:#111;color:#eee;font:16px system-ui;max-width:1240px;margin:30px auto}div{display:flex;flex-wrap:wrap}figure{margin:8px}img{max-width:100%;height:auto}</style><h1>Seven original-glyph FPS labels</h1><p>Private BC3 round trips at2x, dim/normal/focused. Unlimited uses stockFont glyphs with190px tight ink width inside192px source geometry. Glyph pixels are neither resampled nor overlapped; spacing is tight and focused side halo clips. Earlier six values are unchanged. Sentinel0 is a backend contract, not a claim that uncapping has been demonstrated here.</p>'+''.join(panels))
 return r
