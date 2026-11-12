"""Original resolution donor composition; outputs must remain private."""
import sys
if sys.flags.optimize:raise SystemExit('Optimized execution is unsupported')
import base64,io,json,struct
from PIL import Image,ImageFilter
from glyph_helpers import EXPECTED,LABELS,DONORS,sha,dds,refine_bc3_alpha,spans
def build(texture,layout,out):
 assert sha(texture)==EXPECTED['texture'],'Unknown texture build'
 assert sha(layout)==EXPECTED['layout'],'Unknown layout build'
 assert len(texture)==56+2048*1024 and texture[37:39]==b'\x5b\xab'
 assert layout[:4]==b'HWYX' and len(layout)==32+61*16
 atlas=Image.open(io.BytesIO(dds(texture[56:],2048,1024))).convert('RGBA')
 rows=[];boxes=[];provenance=[]
 for i,text in enumerate(LABELS):
  pair=[]
  for state in [0,1]:
   rect=struct.unpack_from('<4I',layout,32+(39+2*i+state)*16);x,y,w,h=rect;assert (w,h)==(348,60)
   pair.append(atlas.crop((x,y,x+w,y+h)))
  rows.append(pair);s=spans(pair[0]);assert len(s)==len(text),(i,text,s);boxes.append(s)
 for c,(row,index) in DONORS.items():
  assert LABELS[row][index]==c
  provenance.append(dict(glyph=c,stock_label=LABELS[row],normal_record=39+2*row,selected_reference_record=40+2*row,column_span=boxes[row][index]))
 target_spans=boxes[9];centers=[(a+b-1)/2 for a,b in target_spans]
 # Keep the stock separator and third-value spacing; replace digit cells only.
 boundaries=[0]+[round((centers[i-1]+centers[i])/2) for i in range(1,len(centers))]+[348]
 results=[];images=[]
 for number,text in [('2560','2560×1440/1440'),('3840','3840×2160/2160')]:
  assert len(text)==len(LABELS[9])
  for state,name in [(0,'normal'),(1,'selected')]:
   tile=Image.new('RGBA',(352,64));tile.paste(rows[9][state],(0,0));changes=[]
   for i,c in enumerate(text):
    if c==LABELS[9][i]:continue
    assert c.isdigit() and c in DONORS
    row,index=DONORS[c];left,right=boxes[row][index];source_center=(left+right-1)/2
    shift=round(centers[i]-source_center);a,b=boundaries[i:i+2]
    patch=Image.new('RGBA',(b-a,60))
    # Original numeral masks are translated at integer offsets, never resampled.
    source=rows[row][0]
    for x in range(a,b):
     sx=x-shift
     for y in range(60):
      if not 0<=sx<348:continue
      if not left-1<=sx<=right:continue
      patch.putpixel((x-a,y),source.getpixel((sx,y)))
    tile.paste(patch,(a,0));changes.append(dict(position=i,glyph=c,donor_row=row,donor_index=index,integer_shift=shift))
   if not state:
    normal_tile=tile.copy()
   else:
    # Whole-label reconstruction avoids donor-neighbor glow seams. This
    # fitted Gaussian is an approximation, never an exact stock-glow claim.
    core_alpha=normal_tile.getchannel('A').crop((0,0,348,60))
    halo=Image.new('RGBA',(348,60),(255,77,57,0))
    halo.putalpha(core_alpha.filter(ImageFilter.GaussianBlur(4)).point(lambda x:min(255,round(x*1.5))))
    core=Image.new('RGBA',(348,60),(255,255,255,0));core.putalpha(core_alpha)
    tile=Image.new('RGBA',(352,64));tile.paste(Image.alpha_composite(halo,core),(0,0))
   assert tile.crop((348,0,352,64)).getchannel('A').getbbox() is None
   assert tile.crop((0,60,352,64)).getchannel('A').getbbox() is None
   filename=f'{number}-{name}';tile.save(out/(filename+'-working.png'))
   buffer=io.BytesIO();tile.save(buffer,format='DDS',pixel_format='DXT5');encoded=buffer.getvalue();assert encoded[:4]==b'DDS ' and encoded[84:88]==b'DXT5'
   raw=refine_bc3_alpha(encoded[128:],tile);assert len(raw)==352*64
   (out/(filename+'.bc3')).write_bytes(raw);decoded=Image.open(io.BytesIO(dds(raw,352,64))).convert('RGBA');decoded.save(out/(filename+'-roundtrip.png'))
   source_bytes=tile.tobytes();decoded_bytes=decoded.tobytes();alpha=[abs(a-b) for a,b in zip(source_bytes[3::4],decoded_bytes[3::4])]
   # Compression must preserve threshold silhouette to a tight bounded error.
   inter=sum(a>=128 and b>=128 for a,b in zip(source_bytes[3::4],decoded_bytes[3::4]));union=sum(a>=128 or b>=128 for a,b in zip(source_bytes[3::4],decoded_bytes[3::4]));iou=inter/union
   assert iou>.995 and sum(alpha)/len(alpha)<1.5,(filename,iou)
   results.append(dict(file=filename+'.bc3',label=text.replace('/',' / '),state=name,width=352,height=64,label_rect=[0,0,348,60],bytes=len(raw),sha256=sha(raw),alpha_mean_error=sum(alpha)/len(alpha),alpha_max_error=max(alpha),compression_silhouette_iou=iou,changes=changes))
   images.append((filename,decoded))
 manifest=dict(stage='private offline tiles; original numeral masks with approximate fitted selected glow',encoder='Pillow DXT5 color with local BC3 alpha endpoint refinement',source_sha256=EXPECTED,format='raw BC3/DXT5; 16 bytes per 4x4 block; no header',order=[r['file'] for r in results],glyph_provenance=provenance,outputs=results,selected_glow_limit='Normal numeral alpha masks are copied unchanged. Selected color/glow is reconstructed with Gaussian radius4 gain1.5 and red255,77,57 plus white core. Fitted against stock record58; approximate, not pixel-identical.',public_code_only=True,installed=False)
 (out/'label-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
 panels=[]
 for filename,im in images:
  bg=Image.new('RGBA',im.size,(28,19,6,255));bg.alpha_composite(im);buf=io.BytesIO();bg.convert('RGB').resize((1056,192),Image.Resampling.NEAREST).save(buf,format='PNG')
  panels.append('<h2>'+filename+'</h2><img width="1056" height="192" src="data:image/png;base64,'+base64.b64encode(buf.getvalue()).decode()+'">')
 (out/'review.html').write_text('<!doctype html><meta charset="utf-8"><title>Stock numeral composition</title><style>body{background:#111;color:#eee;font:16px system-ui;max-width:1100px;margin:30px auto}img{max-width:100%;height:auto}h2{font-size:18px}</style><h1>Original stock numeral composition</h1><p>Private offline BC3 round trips, enlarged3x with nearest sampling. Original numeral pixels; no replacement font or invented strokes. Third value and spacing follow the stock normal-height resolution format. Selected-state glow is a fitted whole-label Gaussian approximation, not the original baked pixels. This avoids donor-cell seams and requires visual approval. Nothing installed.</p>'+''.join(panels))
 return manifest
