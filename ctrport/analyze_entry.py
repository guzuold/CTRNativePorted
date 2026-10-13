# SPDX-License-Identifier: GPL-2.0-only
"""Retail entry control-flow evidence only; no original execution or main credit."""
import argparse,json,struct,sys,hashlib
from pathlib import Path
from inventory_imports import ROOT,PINS
sys.path.insert(0,str(ROOT/'tools/harness'))
sys.path.insert(0,str(ROOT.parent/'Tools/vendor/native_probe'))
from images import OriginalImage
import capstone

def analyze(path,version):
 image=OriginalImage(path)
 if image.sha256!=PINS[version]:raise ValueError('Entry image SHA differs')
 entry=struct.unpack_from('<Q',image.data,image.elf_offset+24)[0]
 header=next(h for h in image.loads if h[1]&1 and h[3]<=entry<h[3]+h[5])
 code=image.read_file(header[2]+entry-header[3],min(512,header[3]+header[5]-entry))
 decoder=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
 thunks=image.thunks();calls=[];instructions=[]
 for instruction in decoder.disasm(code,entry):
  instructions.append({'address':hex(instruction.address),'mnemonic':instruction.mnemonic,'operands':instruction.op_str})
  if instruction.mnemonic=='call' and instruction.op_str.startswith('0x'):
   target=int(instruction.op_str,16);symbol=thunks.get(target)
   calls.append({'caller':hex(instruction.address),'target':hex(target),'import':{k:symbol[k] for k in ('nid','library','module','type')} if symbol else None})
  if instruction.mnemonic in ('ret','ud2'):break
 if not instructions or instructions[-1]['mnemonic'] not in ('ret','ud2'):raise ValueError('Entry bounded disassembly did not terminate')
 return {'version':version,'eboot_sha256':image.sha256,'entry':hex(entry),'instructions':instructions,'direct_calls':calls,'main_reached':False,'retail_executed':False,'ABI_status':'register and startup block requirements need original execution evidence'}

def main():
 p=argparse.ArgumentParser();p.add_argument('--base',type=Path,required=True);p.add_argument('--patch',type=Path,required=True);a=p.parse_args()
 report={'status':'static_analysis_complete','scope':'Pinned retail entry instruction/control-flow analysis, no game bytes saved','versions':[analyze(a.base/'eboot.bin','01.00'),analyze(a.patch/'eboot.bin','01.21')],'decoder_version':capstone.__version__,'decoder_sha256':hashlib.sha256(Path(capstone.__file__).read_bytes()).hexdigest(),'R1':False,'R2':False,'runtime_replacements':0}
 (ROOT/'reports/reconstruction/infra/ctrport_entry_analysis.json').write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps([{'version':v['version'],'entry':v['entry'],'calls':v['direct_calls']} for v in report['versions']]))
if __name__=='__main__':main()
