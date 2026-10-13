# SPDX-License-Identifier: GPL-2.0-only
"""Bounded diagnostic: missing SharePlay call never returns; other workers progress."""
import subprocess,os,json,hashlib,sys,datetime
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent.parent))
from tools.lane_lock import lane_lock
root=Path('../Staging/NitroFueled_GameFiles/CUSA14876');logs=Path('runtime/user/logs')
exe=Path(os.environ.get('CTR_PENDING_EXECUTABLE','build/CTRPort-infra/ctrport.exe'));env=os.environ.copy();env.setdefault('CTR_VIDEO_TILING_SHADER_DIR','build/CTRPort-data/tiling-glslang-ninja');env['CTR_PARK_MISSING_NID']='+MCXJlWdi+s'
timeout=int(env.get('CTR_PENDING_TIMEOUT_SECONDS','12'))
assert 12<=timeout<=120, 'Diagnostic timeout must stay bounded to12..120seconds'
for name in ['CTR_STARTUP_WRITE_DLL_DIR','CTR_STARTUP_MUTEX_DLL_DIR']:env.pop(name,None)
assert exe.resolve().parent==Path('build/CTRPort-infra').resolve() and exe.suffix=='.exe', 'Diagnostic executable must stay in infra build directory'
control=subprocess.run([str(exe),'--compat-stubs-control'],capture_output=True,text=True,timeout=5)
assert control.returncode==0
registry=json.loads(Path('ctrport/compat_stubs.json').read_text())['entries']
checks=[json.loads(x) for x in control.stdout.splitlines()];assert len(checks)==len(registry)
for i,(entry,event) in enumerate(zip(registry,checks)):
 assert event['registry_index']==i and all(event[k]==entry[k] for k in ['nid','library','returned_value'])
out=logs/'shareplay_park.stdout.log';err=logs/'shareplay_park.stderr.log'
ordered_path=logs/'submit_flip_ordered.json'
if ordered_path.exists():ordered_path.unlink()
(logs/'submit_flip_graphics.jsonl').unlink(missing_ok=True)
(logs/'submit_flip_census.jsonl').unlink(missing_ok=True)
launch_sha256=hashlib.sha256(exe.read_bytes()).hexdigest()
checkpoint_status=None
checkpoint_path=logs/'infra_thread_checkpoint.jsonl'
if env.get('CTR_THREAD_CHECKPOINT')=='1':checkpoint_path.unlink(missing_ok=True)
started=datetime.datetime.now(datetime.timezone.utc).isoformat()
with out.open('wb') as o,err.open('wb') as e:
 p=subprocess.Popen([str(exe),'--entry-after-libc-init-images',str(root/'eboot.bin')]+[str(x) for x in sorted((root/'sce_module').glob('*.prx'))],stdout=o,stderr=e,env=env)
 try:code=p.wait(timeout=timeout)
 except subprocess.TimeoutExpired:
  if env.get('CTR_THREAD_CHECKPOINT')=='1':
   previous=None;guest_base=None
   for line in out.read_text(errors='replace').splitlines():
    if not line.startswith('{') or len(line)>4096:continue
    try:event=json.loads(line)
    except ValueError:continue
    if event.get('scope')=='original_startup_libc_dependency_boundary' and event.get('library')=='libSceSharePlay' and previous and previous.get('scope')=='original_caller_location' and previous.get('image_sha256')==event.get('source_image_sha256'):
     guest_base=event['caller_address']-previous['offset'];break
    previous=event
   if guest_base:
    try:
     checkpoint=subprocess.run([str(exe.parent/'thread_checkpoint.exe'),str(p.pid),str(guest_base),str(0x4000000)],capture_output=True,text=True,timeout=10)
     checkpoint_path.write_text(checkpoint.stdout);checkpoint_status=checkpoint.returncode
    except Exception as error:checkpoint_status=type(error).__name__
  p.kill();p.wait();code='diagnostic_timeout_killed' 
finished=datetime.datetime.now(datetime.timezone.utc).isoformat()
assert hashlib.sha256(exe.read_bytes()).hexdigest()==launch_sha256, 'Executable changed during retail replay; evidence rejected'
events=[];malformed=[];active_announcements=[]
for line in out.read_text(errors='replace').splitlines():
 if not line.startswith('{'):continue
 try:event=json.loads(line)
 except ValueError:malformed.append({'length':len(line),'sha256':hashlib.sha256(line.encode()).hexdigest()});continue
 if event.get('scope')=='compatibility_stubs_active':active_announcements.append(event)
 if event.get('scope','').startswith(('original_Gnm','original_VideoOut','original_Semaphore','original_startup_libc_dependency_boundary','declared_compatibility_stub','original_caller_location')):events.append(event)
assert len(active_announcements)==1 and active_announcements[0]['count']==len(registry), 'Runtime active-stub announcement differs from registry'
if ordered_path.exists():events.append(json.loads(ordered_path.read_text()))
graphics_path=logs/'submit_flip_graphics.jsonl'
if graphics_path.exists():
 events=[e for e in events if e.get('scope')!='original_GnmSubmitFlip_first_graphics']
 events.extend(json.loads(line) for line in graphics_path.read_text().splitlines())
census_path=logs/'submit_flip_census.jsonl'
if census_path.exists():
 census=[json.loads(line) for line in census_path.read_text().splitlines()]
 scopes={e['scope'] for e in census}
 events=[e for e in events if e.get('scope') not in scopes]+census
canonical=logs/'compat_calls.jsonl'
called=[json.loads(line) for line in canonical.read_text().splitlines()]
assert called and all(all(event[k]==registry[event['registry_index']][k] for k in ['nid','library','returned_value']) for event in called)
r={'thread_checkpoint_status':checkpoint_status,'process_id':p.pid,'started_UTC':started,'finished_UTC':finished,'timeout_seconds':timeout,'graphics_census_stages':env.get('CTR_GRAPHICS_CENSUS_STAGES','0'),'canonical_compatibility_log':str(canonical),'called_stubs':called,'pending_lines':[line for line in err.read_text(errors='replace').splitlines() if line.startswith('PENDING')],'scope':'missing_call_pending_no_return_diagnostic','exit':code,'exe_path':str(exe),'exe_sha256':launch_sha256,'active_stubs':len(registry),'active_stub_announcements':active_announcements,'events':events,'runtime_failures':[line for line in err.read_text(errors='replace').splitlines() if line.startswith('NID=')], 'stderr_tail':err.read_text(errors='replace').splitlines()[-12:],'malformed':malformed,'R3':False,'engine_replacements':0,'limitation':'Pending SharePlay caller has not returned; independent worker progress cannot count as a normal boot or completed service.'}
with lane_lock('shared','infra'):
 Path('reports/reconstruction/infra/shareplay_pending_startup.json').write_text(json.dumps(r,indent=2)+'\n')
print(json.dumps({'exit':code,'tail':r['stderr_tail'],'last_events':[{k:(len(v) if k=='packets' and isinstance(v,list) else v) for k,v in event.items()} for event in events[-5:]]}))
