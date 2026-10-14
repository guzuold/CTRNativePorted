"""Build a local, content-addressed UI archive; never change the retail dump.

Uses the retail IGZ10 object/fixup schema already covered by igz_asset_input.cpp.
Only the visually identified 512x263 button atlas is changed, not font metrics.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path
from PIL import Image
from ctr_nf_iga import Archive

ROOT = Path(__file__).resolve().parents[2]
ASSETS = ROOT / 'ctrport/assets/xbox-default'

def sha(data):
    return hashlib.sha256(data).hexdigest()

def font_layout(data):
    u = lambda o: struct.unpack_from('<I', data, o)[0]
    if (u(0), u(4), u(12)) != (0x49475a01, 10, 8):
        raise ValueError('Unsupported font format')
    chunks = []
    for o in range(20, 0x224, 16):
        c = struct.unpack_from('<4I', data, o)
        if not c[1]: break
        if c[1]+c[2] > len(data): raise ValueError('Chunk outside file')
        chunks.append(c)
    fixups = {}; o = chunks[0][1]
    for _ in range(u(16)):
        tag = data[o:o+4].decode(); n, size, header = struct.unpack_from('<3I',data,o+4)
        fixups[tag] = (n,o+header,size-header); o += size
    n,o,size = fixups['TMET']; types=[]
    for _ in range(n):
        end=data.index(0,o); types.append(data[o:end].decode()); o=end+1
        if data[o]==0: o+=1
    n,o,size=fixups['RVTB']; objects=[]; prev=delta=shift=0
    def absolute(ptr): return chunks[(ptr>>27)+1][1]+(ptr&0x7ffffff)
    for byte in data[o:o+size]:
        for nib in (byte&15,byte>>4):
            delta|=(nib&7)<<shift; shift+=3
            if not nib&8:
                prev+=delta*4;delta=shift=0
                pos=absolute(prev);objects.append((pos,types[u(pos)]))
                if len(objects)==n:break
        if len(objects)==n:break
    return u,objects,absolute

def patch_font(data):
    u,objects,absolute=font_layout(data)
    images=[p for p,t in objects if t=='igImage2' and struct.unpack_from('<HH',data,p+24)==(512,263)]
    lists=[p for p,t in objects if t=='igCharMetricsList' and u(p+16) in (13,14)]
    if len(images)!=1 or len(lists)!=1: raise ValueError('Button atlas/layout not recognized')
    pos=images[0]; start=absolute(u(pos+64)); size=u(pos+56)
    if size!=512*264*4: raise ValueError('Unexpected button texture extent')
    # Exact retail atlas fingerprint: do not project coordinates onto other data.
    original=data[start:start+size]
    expected=json.loads((ASSETS/'retail_atlas.json').read_text())
    is_update=sha(original) in expected['update_variants']
    if sha(original)!=expected['sha256'] and not is_update: raise ValueError('Unknown retail button atlas')
    if u(lists[0]+16)!=(14 if is_update else 13):raise ValueError('Unexpected metric layout')
    pixels=bytearray(size)
    def index(x,y):
        x0=x%8;y0=y%8
        morton=sum(((x0>>b)&1)<<(2*b) | ((y0>>b)&1)<<(2*b+1) for b in range(3))
        return (((y//8)*64+x//8)*64+morton)*4
    for y in range(264):
        for x in range(512):pixels[(y*512+x)*4:(y*512+x+1)*4]=original[index(x,y):index(x,y)+4]
    # Retail atlas is BGRA8 (the original Cross is blue in the running game).
    atlas=Image.frombytes('RGBA',(512,264),bytes(pixels),'raw','BGRA')
    # Indices established from retail metrics and visual inspection of this atlas.
    mapping={3:'LT',4:'RT',5:'LB',6:'RB',7:'B_Color',8:'Y_Color',9:'L',10:'R',11:'X_Color',12:'A_Color'}
    if is_update:mapping={3:'A_Color',4:'B_Color',5:'Y_Color',6:'X_Color',7:'LB',8:'RB',9:'LT',10:'RT',11:'L',12:'R',13:'Share'}
    table=absolute(u(lists[0]+32)); records=[]
    for i,name in mapping.items():
        metric=absolute(u(table+i*8)); uv=struct.unpack_from('<4f',data,metric+52)
        box=(round(uv[0]*512),round(uv[1]*263),round(uv[2]*512),round(uv[3]*263))
        x,y,r,b=box
        if not (0<=x<r<=512 and 0<=y<b<=263): raise ValueError('Invalid glyph rectangle')
        icon=Image.open(ASSETS/('T_X_'+name+'.png')).convert('RGBA')
        icon=icon.crop(icon.getbbox()); icon.thumbnail((r-x,b-y),Image.Resampling.LANCZOS)
        atlas.paste((0,0,0,0),box)
        atlas.paste(icon,(x+(r-x-icon.width)//2,y+(b-y-icon.height)//2))
        records.append({'metric_index':i,'icon':name,'rectangle':box})
    linear=atlas.tobytes('raw','BGRA'); tiled=bytearray(size)
    for y in range(264):
        for x in range(512):tiled[index(x,y):index(x,y)+4]=linear[(y*512+x)*4:(y*512+x+1)*4]
    result=data[:start]+tiled+data[start+size:]
    assert len(result)==len(data) and result[:start]==data[:start] and result[start+size:]==data[start+size:]
    return result,atlas,records

def main():
    parser=argparse.ArgumentParser();parser.add_argument('archive',type=Path);parser.add_argument('--staging',action='store_true');a=parser.parse_args()
    archive=Archive(a.archive); source_hash=sha(archive.data)
    if a.archive.name=='permanent.pak':names={'fonts/geekspeak_outline.igz','fonts/zoinks_outline.igz'}
    elif a.archive.name=='languagestartup.pak':names={'fonts/geekspeak.igz'}
    elif a.archive.name=='update.pak':names={'fonts/geekspeak.igz','fonts/geekspeak_outline.igz','fonts/zoinks_outline.igz'}
    else:raise ValueError('Only permanent.pak, languagestartup.pak and update.pak supported')
    entries=[f for f in archive.files if f['path'].lower() in names]
    if len(entries)!=len(names):raise ValueError('Expected retail button fonts missing')
    output=bytearray(archive.data);replacements={};fonts=[]
    for entry in entries:
        original=archive.extract_bytes(entry)
        replacement,preview,records=patch_font(original)
        replacements[entry['index']]=replacement
        fonts.append({'path':entry['path'],'source_sha256':sha(original),'replacements':records})
        offset=(len(output)+archive.sector_size-1)//archive.sector_size*archive.sector_size
        output.extend(bytes(offset-len(output)));output.extend(replacement)
        descriptor=56+archive.file_count*4+entry['index']*16
        struct.pack_into('<I',output,descriptor,offset)
        struct.pack_into('<I',output,descriptor+12,0xffffffff)
    target=ROOT/'runtime/user/mods'/('xbox-default-staging' if a.staging else 'xbox-default')/source_hash;target.mkdir(parents=True,exist_ok=True)
    pak=target/a.archive.name;pak.write_bytes(output)
    check=Archive(pak)
    for index,replacement in replacements.items():assert check.extract_bytes(check.files[index])==replacement
    # Every other entry retains its exact descriptor and data; appended replacement only.
    for f in archive.files:
        if f['index'] not in replacements:assert check.files[f['index']]==f
    assert sha(a.archive.read_bytes())==source_hash
    preview.save(target/'button-atlas.png')
    (target/(a.archive.name+'.sha256')).write_text(sha(output)+'\n')
    report={'source_sha256':source_hash,'output_sha256':sha(output),'source_unchanged':True,'fonts':fonts,'other_entries_unchanged':len(archive.files)-len(entries),'runtime_verified':False}
    (target/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({'output':str(pak),**report}))

if __name__=='__main__':main()
