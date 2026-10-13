# SPDX-License-Identifier: GPL-2.0-only
from pathlib import Path
import json,hashlib,datetime
r=json.loads(Path('reports/reconstruction/infra/json2_compat_startup.json').read_text())
rows=[]
for line in Path('runtime/user/logs/submit_flip_census.jsonl').read_text(errors='replace').splitlines():
 if line.startswith('{') and len(line)<3000:
  try:rows.append(json.loads(line))
  except ValueError:pass
samples=[x for x in rows if x.get('scope')=='compute_output_depth_GPU_snapshot']
assert samples and all(x['word']==755 and x['status']==0 and x['converted_depth_bytes']==17694720 and not x['guest_publication'] and not x['guard_retirement'] for x in samples),samples
assert not r['original_access_faults']
exe=Path('build/CTRPort-infra/ctrport.exe')
assert hashlib.sha256(exe.read_bytes()).hexdigest()==r['exe_sha256']
report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'scope':'actual_original_retained_GPU_depth_extraction_and_retiled_snapshot','passed':True,'retail_snapshots':samples,'source_sha256':{p:hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in ['ctrport/depth_gpu_snapshot.hpp','ctrport/entry_boundary.hpp','ctrport/video/compute_depth_ms_full_control.comp']},'shader_sha256':hashlib.sha256(Path('build/CTRPort-data/compute_depth_ms_full_control.spv').read_bytes()).hexdigest(),'exe_sha256':r['exe_sha256'],'active_stubs':r['active_stubs'],'called_stubs':{str(x['registry_index']):x['public_name'] for x in r['called_stubs']},'provenance':{'depth_gpu_snapshot.hpp':'Original infra orchestration of existing project GPL-2.0-only C APIs and authored compute_depth_ms_full_control.comp, no prof input or new upstream code.','tiling':'Existing data provider public shadPS4 tiling at pinned e3ce810f3a653f43ac64ebab63023de281a4103a, GPL-2.0-or-later; file provenance remains with data.'},'limits':'Actual GPU extraction/retile only. No independent pixel equivalence oracle on this retail owner; authored provider controls separate. No stencil conversion, guest publication, guard retirement or compute755 completion. No R4/R5/audio/navigation credit.','R3':True,'R4':False,'R5':False,'engine_replacements':0}
Path('reports/reconstruction/infra/depth755_GPU_snapshot.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'passed':True,'snapshots':samples}))
