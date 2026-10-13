# SPDX-License-Identifier: GPL-2.0-only
"""Targeted original startup block comparison; never runs the common suite."""
from pathlib import Path
import subprocess, os, json, hashlib
root=Path('../Staging/NitroFueled_GameFiles/CUSA14876')
exe=Path('build/CTRPort-infra/ctrport.exe')
args=[str(exe),'--entry-after-libc-init-images',str(root/'eboot.bin')]+[str(p) for p in sorted((root/'sce_module').glob('*.prx'))]
r={'scope':'original_startup_mutex_reload_block_comparison','exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'runs':[], 'R3':False,'engine_replacements':0}
for reload in (False, True):
 env=os.environ.copy()
 for key in ('CTR_STARTUP_WRITE_DLL_DIR','CTR_STARTUP_MUTEX_DLL_DIR'):env.pop(key,None)
 if reload:env['CTR_STARTUP_MUTEX_DLL_DIR']=str(exe.parent.resolve())
 mode='reload' if reload else 'cold'
 logs=Path('runtime/user/logs');out=logs/('startup_mutex_'+mode+'.stdout.log');err=logs/('startup_mutex_'+mode+'.stderr.log')
 with out.open('wb') as o,err.open('wb') as e:
  p=subprocess.Popen(args,stdout=o,stderr=e,env=env)
  try:code=p.wait(timeout=25)
  except subprocess.TimeoutExpired:p.kill();p.wait();raise RuntimeError('Original startup timed out')
 s=out.read_text(errors='replace');t=err.read_text(errors='replace')
 events=[line for line in s.splitlines() if 'reload_admitted' in line or 'original_startup_mutex' in line or 'original_mutex_dll_adapter' in line]
 assert code==4 and 'module=0xe7' in t,(mode,code,t[-500:])
 if reload:
  assert any('wait_observed' in line for line in events)
  for generation in (1,2):assert any('adapter_return' in line and ('"generation":'+str(generation)) in line for line in events)
 r['runs'].append({'mode':mode,'exit_code':code,'stdout':str(out),'stderr':str(err),'events':events,'terminal_module':'0xe7'})
r['passed']=True
r['limitations']=['Concrete mutex-init service only; no generic import ABI support.','Publisher uses two prebuilt uniquely named DLLs; operator watcher and arbitrary successive services remain pending.','Json2 remains explicitly missing; no game frame and no engine reconstruction installed.']
Path('reports/reconstruction/infra/startup_mutex_reload.json').write_text(json.dumps(r,indent=2)+'\n')
print(json.dumps(r,indent=2))
