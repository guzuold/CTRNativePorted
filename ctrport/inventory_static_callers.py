# SPDX-License-Identifier: GPL-2.0-only
"""Retail direct call/tail-jump sites validated from FDE instruction starts."""
import argparse,bisect,hashlib,json,struct,sys
from pathlib import Path
from inventory_imports import ROOT,PINS
sys.path.insert(0,str(ROOT/'tools/harness'))
sys.path.insert(0,str(ROOT.parent/'Tools/vendor/native_probe'))
from images import OriginalImage
import capstone

def extract_uniform_cie(path,digest,output):
    # Retail-only adaptation of tools/symbols/eh_inventory.py. Initial unwind
    # instructions vary; only zR augmentation controls the address encoding.
    image=OriginalImage(path)
    if image.sha256!=digest:raise ValueError('Module SHA differs')
    h=next(h for h in image.headers if h[0]==0x6474e550)
    table=image.read_file(h[2],h[5])
    if table[:4]!=bytes.fromhex('011b033b'):raise ValueError('EH table encoding unsupported')
    count=struct.unpack_from('<I',table,8)[0]
    if 12+count*8>len(table):raise ValueError('EH table truncated')
    rows=[];cies={};previous=-1
    for i in range(count):
        relative_start,relative_fde=struct.unpack_from('<ii',table,12+i*8)
        start,fde=h[3]+relative_start,h[3]+relative_fde
        length,backward,relative_pc,size=struct.unpack('<IIiI',image.virtual(fde,16))
        cie=fde+4-backward
        if cie not in cies:
            cie_length=struct.unpack('<I',image.virtual(cie,4))[0]
            if cie_length<14 or cie_length>4096:raise ValueError('CIE extent unsupported')
            header=image.virtual(cie,cie_length+4)
            if header[4:17]==bytes.fromhex('00000000017a5200017810011b'):
                cies[cie]={'length':cie_length,'version':1,'augmentation':'zR','code_alignment':1,'data_alignment':-8,'return_register':16,'pointer_encoding':'pcrel_sdata4','initial_unwind_program_bytes':cie_length+4-17}
            elif header[4:18]==bytes.fromhex('00000000017a504c520001781007') and header[18]==0x9b and header[23:25]==bytes.fromhex('1b1b'):
                personality_cell=cie+19+struct.unpack_from('<i',header,19)[0]
                image.virtual(personality_cell,8) # Backed indirect personality cell; no runtime dereference.
                cies[cie]={'length':cie_length,'version':1,'augmentation':'zPLR','code_alignment':1,'data_alignment':-8,'return_register':16,'pointer_encoding':'pcrel_sdata4','personality_encoding':'indirect_pcrel_sdata4','personality_cell':hex(personality_cell),'LSDA_encoding':'pcrel_sdata4','initial_unwind_program_bytes':cie_length+4-25}
            else:
                raise ValueError('CIE semantic prefix unsupported at '+hex(cie)+' version='+str(header[8])+' augmentation='+header[9:header.index(0,9)].decode('ascii'))
        if length<12 or fde+8+relative_pc!=start or start<previous or not any(lo<=start and start+size<=hi for lo,hi in image.code_ranges):raise ValueError('FDE/table/executable bounds differ')
        image.virtual(fde,length+4)
        rows.append({'start':hex(start),'length':size,'fde':hex(fde)})
        previous=start
    result={'eboot_sha256':digest,'count':count,'cie_count':len(cies),'method':'EH table checked against FDE initial PC/range; semantic zR/zPLR CIE augmentation independent of initial unwind program','cies':{hex(k):v for k,v in cies.items()},'functions':rows}
    output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(result,indent=2)+'\n')

def analyze(path,version,invpath=None,pin=None):
 image=OriginalImage(path)
 if image.sha256!=(pin or PINS[version]):raise ValueError('Pinned eboot SHA differs')
 invpath=invpath or ROOT/('coordination/inventory/eh_frame_'+version.lstrip('0').replace('.','_')+'.json')
 inventory=json.loads(invpath.read_text())
 if inventory['eboot_sha256']!=image.sha256:raise ValueError('FDE inventory SHA differs')
 spans=sorted((int(f['start'],16),f['length']) for f in inventory['functions']);starts=[s for s,n in spans]
 thunks=image.thunks();slots={int(x['slot'],16) if isinstance(x['slot'],str) else x['slot']:x for x in image.imports}
 candidates=[]
 for h in image.loads:
  if not h[1]&1:continue
  code=image.read_file(h[2],h[5]);base=h[3]
  for opcode,size in [(b'\xe8',5),(b'\xe9',5),(b'\xff\x15',6),(b'\xff\x25',6)]:
   at=code.find(opcode)
   while at>=0:
    if at+size<=len(code):
     target=base+at+size+struct.unpack_from('<i',code,at+size-4)[0]
     symbol=thunks.get(target) if size==5 else slots.get(target)
     if symbol:candidates.append((base+at,size,symbol,opcode.hex()))
    at=code.find(opcode,at+1)
 decoder=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);cache={};accepted=[];rejected=0
 for address,size,symbol,opcode in candidates:
  index=bisect.bisect_right(starts,address)-1
  if index<0 or address>=spans[index][0]+spans[index][1]:rejected+=1;continue
  start,length=spans[index]
  if start not in cache:
   try:cache[start]={a:(n,m) for a,n,m,op in decoder.disasm_lite(image.virtual(start,length),start)}
   except ValueError:cache[start]={}
  decoded=cache[start].get(address)
  if not decoded or decoded[0]!=size or decoded[1] not in ('call','jmp'):rejected+=1;continue
  accepted.append({'caller':hex(address),'containing_function':hex(start),'kind':'call' if decoded[1]=='call' else 'tail_jump','slot':symbol['slot'],'nid':symbol['nid'],'library':symbol['library'],'module':symbol['module']})
 return {'version':version,'eboot_sha256':image.sha256,'fde_inventory_sha256':hashlib.sha256(invpath.read_bytes()).hexdigest(),'raw_candidates':len(candidates),'boundary_rejected':rejected,'validated_sites':len(accepted),'sites':accepted,'limitations':['Static instruction boundary, not execution evidence.','Register-indirect calls and module caller inventories pending.','FDE linear decode does not prove branch reachability.']}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--base',type=Path,required=True);p.add_argument('--patch',type=Path,required=True);p.add_argument('--modules-only',action='store_true');a=p.parse_args()
 report_path=ROOT/'reports/reconstruction/infra/ctrport_static_callers.json'
 if a.modules_only:
  r=json.loads(report_path.read_text())
  sys.path.insert(0,str(ROOT/'tools/symbols'))
  from eh_inventory import extract
  census=json.loads((ROOT/'reports/reconstruction/infra/ctrport_imports_static.json').read_text())
  module_cache={}
  for version in census['versions']:
   rows=[]
   for module in version['modules']:
    digest=module['sha256']
    if digest not in module_cache:
     path=(a.patch if module['overlay_layer']=='patch' else a.base)/module['file']
     inventory_path=ROOT/'reports/reconstruction/infra/module_fde'/ (digest+'.json')
     try:
      try:extract(path,digest,inventory_path)
      except ValueError as error:
       if str(error)!='Unsupported CIE; do not guess FDE encoding':raise
       extract_uniform_cie(path,digest,inventory_path)
      measured=analyze(path,version['version'],inventory_path,digest)
      measured['image_sha256']=measured.pop('eboot_sha256')
      module_cache[digest]=measured
     except (ValueError,StopIteration) as error:
      module_cache[digest]={'image_sha256':digest,'status':'missing_supported_FDE_inventory','diagnostic':str(error) or 'PT_GNU_EH_FRAME absent','sites':[]}
    rows.append(dict(module_cache[digest],file=module['file']))
   next(v for v in r['versions'] if v['version']==version['version'])['modules']=rows
 else:
  r={'status':'static_sites_measured','versions':[analyze(a.base/'eboot.bin','01.00'),analyze(a.patch/'eboot.bin','01.21')],'decoder_version':capstone.__version__,'R1':False,'R2':False,'runtime_replacements':0}
 report_path.write_text(json.dumps(r,indent=2)+'\n')
 print(json.dumps([{'version':v['version'],'eboot_sites':v['validated_sites'],'modules':[{k:m[k] for k in ['file','image_sha256'] }|{'sites':len(m['sites']),'status':m.get('status','static_sites_measured')} for m in v.get('modules',[])]} for v in r['versions']]))
