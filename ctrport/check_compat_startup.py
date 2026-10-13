# SPDX-License-Identifier: GPL-2.0-only
"""Declared compatibility controls and one cold original startup block, not suite."""
from pathlib import Path
import subprocess,json,hashlib,os,threading,time,ctypes,datetime
from transfer_timing_report import aggregate_transfer_phases
exe=Path('build/CTRPort-infra/ctrport.exe');root=Path('../Staging/NitroFueled_GameFiles/CUSA14876')
registry=json.loads(Path('ctrport/compat_stubs.json').read_text())['entries']
c=subprocess.run([str(exe),'--compat-stubs-control'],capture_output=True,text=True,timeout=5)
assert c.returncode==0,(c.returncode,c.stderr)
control=[json.loads(l) for l in c.stdout.splitlines()]
assert len(control)==len(registry)
for index,(entry,event) in enumerate(zip(registry,control)):
 assert event["registry_index"]==index
 assert all(event[k]==entry[k] for k in ('nid','library','returned_value'))
 assert event['caller']!=0 and event['tag']=='R8'
 if entry.get('module_id')==231:assert event['caller']==0x12345678
logs=Path('runtime/user/logs');out=logs/'json2_compat.stdout.log';err=logs/'json2_compat.stderr.log'
# A boot can stop before GNM. Preserve earlier evidence but never leave its
# census at the current-run path where it could be mistaken for fresh output.
census=logs/'submit_flip_census.jsonl'
if census.exists():census.rename(logs/f'submit_flip_census.previous.{time.time_ns()}.jsonl')
env=os.environ.copy()
for k in ['CTR_STARTUP_WRITE_DLL_DIR','CTR_STARTUP_MUTEX_DLL_DIR','CTR_PARK_MISSING_NID','CTR_PARK_MISSING_SERVICE']:env.pop(k,None)
timeout=int(env.get('CTR_COLD_TIMEOUT_SECONDS','30'))
# Long cold is opt-in for measured slow original intro batches; default stays30s.
assert 30<=timeout<=1800
capture_max=int(env.get('CTR_COLD_CAPTURE_MAX','32'))
assert 1<=capture_max<=128
captures=[];capture_stop=threading.Event()
capture_run_id=str(time.time_ns())
def capture_completed_frames(pid):
 seen=0;position=0;pending=b''
 wgc=env.get('CTR_COLD_CAPTURE_WGC')=='1'
 api=ctypes.WinDLL('user32');api.GetForegroundWindow.restype=ctypes.c_void_p
 api.GetWindowThreadProcessId.argtypes=[ctypes.c_void_p,ctypes.POINTER(ctypes.c_ulong)]
 while not capture_stop.wait(.2):
  with out.open('rb') as stream:stream.seek(position);chunk=stream.read();position=stream.tell()
  pending+=chunk;parts=pending.split(b'\n');pending=parts.pop()
  for line in parts:
   if b'"scope":"frame_observation_hold"' not in line:continue
   event=json.loads(line)
   if not event.get('real_present'):continue
   seen+=1
   if seen>capture_max:continue
   # The renderer pumps its window for30s (2s opt-in) after real presentation.
   # Capture during that interval, before cleanup/guest continuation can exit.
   if capture_stop.wait(.3):return
   foreground=ctypes.c_ulong();deadline=time.monotonic()+25
   while True:
    api.GetWindowThreadProcessId(api.GetForegroundWindow(),ctypes.byref(foreground))
    if wgc or foreground.value==pid or time.monotonic()>=deadline:break
    if capture_stop.wait(.2):return
   identity={'pid':pid,'UTC':datetime.datetime.now(datetime.timezone.utc).isoformat(),'foreground_pid':foreground.value}
   if not wgc and foreground.value!=pid:
    captures.append({'frame':seen,**identity,'status':'not_foreground_no_capture','focus_changed':False});continue
   path=Path('runtime/user/captures')/('infra_cold_'+str(pid)+'_'+capture_run_id+'_frame_'+str(seen)+'.bmp');path.parent.mkdir(parents=True,exist_ok=True)
   try:
    result=subprocess.run([str(exe.parent/('capture_window.exe' if wgc else 'capture_client.exe')),str(pid),str(path)],capture_output=True,text=True,timeout=8)
    captures.append({'frame':seen,**identity,'exit':result.returncode,'capture_stderr':result.stderr.strip()[:1024],'path':str(path) if result.returncode==0 else None,'capture':('WGC exact PID whole window; no focus change' if wgc else 'DXGI desktop-duplication client crop; verify visually for occlusion'),'trigger':'frame_observation_hold after real present','sha256':hashlib.sha256(path.read_bytes()).hexdigest() if result.returncode==0 else None,'focus_changed':False})
   except subprocess.TimeoutExpired:captures.append({'frame':seen,**identity,'status':'capture_timeout','focus_changed':False})
with out.open('wb') as o,err.open('wb') as e:
 p=subprocess.Popen([str(exe),'--entry-after-libc-init-images',str(root/'eboot.bin')]+[str(x) for x in sorted((root/'sce_module').glob('*.prx'))],stdout=o,stderr=e,env=env)
 Path("runtime/user/logs/infra_cold_active.json").write_text(json.dumps({"pid":p.pid,"exe":str(exe.resolve()),"exe_sha256":hashlib.sha256(exe.read_bytes()).hexdigest(),"UTC":datetime.datetime.now(datetime.timezone.utc).isoformat()},indent=2)+"\n",encoding="utf8")
 capture_thread=None
 if env.get("CTR_COLD_CAPTURE")=="1":
  capture_thread=threading.Thread(target=capture_completed_frames,args=(p.pid,));capture_thread.start()
 try:code=p.wait(timeout=timeout)
 except subprocess.TimeoutExpired:p.kill();p.wait();code='timeout'
 capture_stop.set()
 if capture_thread:capture_thread.join(timeout=6)
lines=out.read_text(errors='replace').splitlines()+err.read_text(errors='replace').splitlines()
malformed_json=[]
valid_lines=[]
for line in lines:
 if line.startswith('{'):
  try:json.loads(line)
  except json.JSONDecodeError:
   malformed_json.append({'length':len(line),'sha256':hashlib.sha256(line.encode()).hexdigest()});continue
 valid_lines.append(line)
lines=valid_lines
events=[json.loads(l) for l in (logs/'compat_calls.jsonl').read_text().splitlines()]
boundaries=[json.loads(l) for l in lines if l.startswith('{') and '"scope":"original_startup_libc_dependency_boundary"' in l]
assert any(e['nid']=='g8cM39EUZ6o' for e in events)
for event in events:
 entry=registry[event['registry_index']]
 assert all(event[k]==entry[k] for k in ('nid','library','returned_value','tag'))
 event['public_name']=entry['public_name']
 if 'module_id' in entry:event['module_id']=entry['module_id']

r={'capture_run_id':capture_run_id,'draw_timings':[json.loads(l) for l in lines if l.startswith('{') and '"scope":"original_GnmSubmitFlip_draw_timing"' in l],'resource_preparation_timings':[json.loads(l) for l in lines if l.startswith('{') and '"scope":"original_GnmSubmitFlip_resource_preparation_timing"' in l],'batch_timings':[json.loads(l) for l in lines if l.startswith('{') and '"scope":"original_GnmSubmitFlip_batch_timing"' in l],'captures':captures,'timeout_seconds':timeout,'diagnostic_parking':False,'scope':'original_cold_startup_with_declared_compatibility','exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'registry_sha256':hashlib.sha256(Path('ctrport/compat_stubs.json').read_bytes()).hexdigest(),'active_stubs':len(registry),'called_stubs':events,'font_backend_initializations':[json.loads(l) for l in lines if l.startswith('{') and '"scope":"font_backend_module_initialize"' in l],'control':{'passed':True,'logged_calls':len(control),'invalid_index_rejected':True,'scope':'synthetic calls through all registered SysV service bridges plus module activation; logging/return contract, not original Json2 behavior'},'exit_code':code,'terminal_boundary':boundaries[-1] if boundaries else None,'original_access_faults':[json.loads(l) for l in lines if l.startswith('{') and any(x in l for x in ['original_startup_unresolved_access','original_fault_registers','original_fault_frame_candidate'])],'clock_ajm_audio_returns':[json.loads(l) for l in lines if l.startswith('{') and any(x in l for x in ['original_KernelClockGettime_return','original_AjmInitialize_return','original_AjmModuleRegister_return','original_AudioOutInit_return','original_AudioOutOpen_return','original_AudioOutSetVolume_return','original_AudioOutOutput_return','original_PthreadSetprio_return','original_UserServiceGetEvent_return'])], 'video_service_returns':[json.loads(l) for l in lines if l.startswith('{') and any(x in l for x in ['"scope":"original_VideoOutOpen_return"','"scope":"original_VideoOutCapability_return"','"scope":"original_VideoOutSetFlipRate_return"','"scope":"original_VideoOutSetBufferAttribute_return"','"scope":"original_VideoOutRegisterBuffers_return"','"scope":"original_VideoOutGetBufferLabelAddress_return"','"scope":"original_VideoOutModeSetAny_return"','"scope":"original_VideoOutConfigureOutputMode_return"'])],'video_registration_observations':[json.loads(l) for l in lines if l.startswith('{') and any(x in l for x in ['original_VideoOutRegisterBuffers_metadata','original_VideoOutRegisterBuffers_address','original_VideoOutConfigureOutputMode_metadata'])],'gnm_submission_observations':[json.loads(l) for l in lines if l.startswith('{"scope":"original_GnmSubmit_')],'gnm_submit_returns':[json.loads(l) for l in lines if l.startswith('{\"scope\":\"original_GnmSubmit_return\"')],'batch_map_observations':[json.loads(l) for l in lines if l.startswith('{\"scope\":\"original_BatchMap_')],'gnm_shader_returns':[json.loads(l) for l in lines if l.startswith('{') and any(x in l for x in ['\"scope\":\"original_GnmSet','\"scope\":\"original_GnmUpdate'])], 'gnm_marker_returns':[json.loads(l) for l in lines if l.startswith('{') and any(x in l for x in ['original_GnmInsertPushMarker_return','original_GnmInsertPopMarker_return'])], 'gnm_dispatch_returns':[json.loads(l) for l in lines if l.startswith('{') and 'original_GnmDispatchDirect_return' in l], 'gnm_draw_returns':[json.loads(l) for l in lines if l.startswith('{') and '"scope":"original_GnmDrawIndexAuto_return"' in l], 'ime_backend_initializations':[json.loads(l) for l in lines if l.startswith('{') and '"scope":"ime_backend_module_initialize"' in l], 'pad_returns':[json.loads(l) for l in lines if l.startswith('{\"scope\":\"original_Pad')], 'gnm_init_returns':[json.loads(l) for l in lines if l.startswith('{') and '"scope":"original_Gnm_init350_return"' in l],'gnm_memory_observations':[json.loads(l) for l in lines if l.startswith('{') and '"scope":"original_gnm_init_destination_authority"' in l],'terminal_module_diagnostic':next((l for l in lines if l.startswith('NID=') and 'unsupported:' in l and ' module=' in l),None),'service_diagnostics':[l for l in lines if l.startswith('NID=') and 'unsupported:' in l],'metadata_events':[json.loads(l) for l in lines if l.startswith('{') and any(x in l for x in ['\"scope\":\"app_content_metadata_loaded\"','\"scope\":\"playgo_metadata_loaded\"'])],'stdout':str(out),'stderr':str(err),'malformed_json_lines':malformed_json,'R3':False,'engine_replacements':0,'limitation':'Compatibility returns are declared stubs, not implementations or reconstruction.'}
with err.open(errors='replace') as phase_log:
 r['transfer_phase_timing']=aggregate_transfer_phases(phase_log)
r['audio_PCM_activity']=[json.loads(l) for l in lines if l.startswith('{') and '"scope":"audio_native_PCM_activity"' in l]
# Keep exact counts and first/last event per scope in high-frequency service arrays.
# Complete raw logs remain in runtime/user; do not commit repetitive clock polling.
r['event_sampling']={}
for key in ['clock_ajm_audio_returns','pad_returns']:
 groups={}
 for event in r[key]:groups.setdefault(event['scope'],[]).append(event)
 r['event_sampling'][key]={scope:len(values) for scope,values in groups.items()}
 r[key]=[event for values in groups.values() for event in (values if len(values)<=2 else [values[0],values[-1]])]
r['gnm_census_sha256']=hashlib.sha256(census.read_bytes()).hexdigest() if census.exists() else None
Path('reports/reconstruction/infra/json2_compat_startup.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
