# SPDX-License-Identifier: GPL-2.0-only
"""Retail Gnm call redirected to resident signature9; no renderer or PS4 success."""
import os,json,hashlib,subprocess
from pathlib import Path
root=Path('../Staging/NitroFueled_GameFiles/CUSA14876')
exe=Path('build/CTRPort-infra/ctrport-host-compiler.exe')
assert not list(Path('runtime/user/services/gnm').glob('*.dll')), 'Control requires no real service generation present'
logs=Path('runtime/user/logs')
# Keep this separate control from replacing the canonical capture of another run.
import atexit
canonical_names=('submit_flip_graphics.jsonl','submit_flip_census.jsonl','submit_flip_ordered.json','compat_calls.jsonl')
previous={name:(logs/name).read_bytes() if (logs/name).exists() else None for name in canonical_names}
def restore_canonical():
 for name,data in previous.items():
  path=logs/name
  if data is None:path.unlink(missing_ok=True)
  else:path.write_bytes(data)
atexit.register(restore_canonical)
out=logs/'gnm_service_pending_stdout.log';err=logs/'gnm_service_pending_stderr.log'
env=os.environ.copy();env.setdefault('CTR_VIDEO_TILING_SHADER_DIR','build/CTRPort-data/tiling-glslang-ninja');env.update(CTR_PARK_MISSING_NID='+MCXJlWdi+s',CTR_WAIT_GNM_SERVICE='1',CTR_GRAPHICS_CENSUS_STAGES='84')
for key in ('CTR_STARTUP_WRITE_DLL_DIR','CTR_STARTUP_MUTEX_DLL_DIR'):env.pop(key,None)
with out.open('wb') as o,err.open('wb') as e:
 p=subprocess.Popen([str(exe),'--entry-after-libc-init-images',str(root/'eboot.bin')]+[str(x) for x in sorted((root/'sce_module').glob('*.prx'))],stdout=o,stderr=e,env=env)
 try:code=p.wait(timeout=20)
 except subprocess.TimeoutExpired:p.kill();p.wait();code='owned_process_timeout_killed'
text=err.read_text(errors='replace');pending=[x for x in text.splitlines() if x.startswith('PENDING service9 NID=')]
assert code=='owned_process_timeout_killed' and len(pending)==1
assert 'PENDING service9 resident NID=xbxNatawohc library=libSceGnmDriver' in text and 'owned_arguments=9' in text
assert 'service9 NID=xbxNatawohc' in pending[0] and 'no return' in pending[0]
assert 'Gnm pending admission failed' not in text and 'Nine-argument service transport failed' not in text
owned=[x for x in text.splitlines() if x.startswith('PENDING Gnm owned commands ')]
assert len(owned)==1 and 'buffers=1' in owned[0] and 'words=0 ' not in owned[0]
census=[json.loads(x) for x in (logs/'submit_flip_census.jsonl').read_text().splitlines()]
batch=next(x for x in census if x.get('scope')=='original_GnmSubmitFlip_owned_batch')
assert batch['retained_graphics_stages']==82 and batch['retained_routes']==58
called=[json.loads(x) for x in (logs/'compat_calls.jsonl').read_text().splitlines()]
report={'scope':'retail_Gnm_signature9_pending','exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'eboot_sha256':'ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1','exit':code,'pending':pending,'resident_owned_commands':owned,'diagnostic_batch':batch,'active_stubs':len(json.loads(Path('ctrport/compat_stubs.json').read_text())['entries']),'called_stubs':called,'real_Dll_published':False,'service_returned':False,'R3':False,'engine_replacements':0,'payload_persisted':False,'limitation':'SharePlay caller also parked; independent Gnm worker diagnostics complete, then context redirects to resident nine-argument bridge and waits for a real generation. Resident command snapshots now survive the wait; compiled-stage/resource lifetime and GPU coherence remain missing. No PS4 implementation, normal boot or frame claim. Owned scalar argument transport is separately exercised through synthetic two-generation DLL controls.','repeat':'python -B ctrport/check_gnm_service_pending.py'}
Path('reports/reconstruction/infra/gnm_service_pending.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'pending':len(pending),'routes':58,'return':False,'R3':False}))
