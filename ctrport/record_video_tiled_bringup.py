# SPDX-License-Identifier: GPL-2.0-only
"""Record retained cold/host observations; does not launch or repeat tests."""
from pathlib import Path
import json,hashlib,datetime
root=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256((root/p).read_bytes()).hexdigest()
def read(p):return json.loads((root/p).read_text())
cold=read('reports/reconstruction/infra/json2_compat_startup.json')
control=read('runtime/user/logs/video_tiled_guest_control.json')
assert control['exit_code']==0
assert cold['exe_sha256']==sha('build/CTRPort-infra/ctrport.exe')
assert cold['terminal_boundary']['nid']=='N1bEoJ4SRw4'
returns=cold['video_service_returns']
assert any(x['scope']=='original_VideoOutRegisterBuffers_return' and x['result']==0 and x['count']==2 for x in returns)
assert any(x['scope']=='original_VideoOutModeSetAny_return' and x['bytes']==32 for x in returns)
r={
 'checked_at':datetime.datetime.now(datetime.timezone.utc).isoformat(),'supervision':'SUP-028',
 'scope':'original retail startup advances through real tiled registration and mode initialization',
 'eboot_sha256':'ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1',
 'build':'MSVC /W4 /WX, build-infra lock, data companion same MSVC/CRT',
 'services':['sceVideoOutRegisterBuffers','sceVideoOutGetBufferLabelAddress','sceVideoOutModeSetAny_','sceVideoOutUnregisterBuffers'],
 'original_calls':returns,'next_boundary':cold['terminal_boundary'],
 'mode_metadata':cold['video_registration_observations'],
 'host_control':control,
 'mode_control_observed':{'command':'build/CTRPort-infra/ctrport.exe --video-mode-any-control','exit_code':0,'sizes':[0,1,3,4,32,48],'sentinel_preserved':True,'negative_rejections':2,'scope':'synthetic through actual SysV bridge, executed during this session; only size32 also observed on original retail'},
 'source_provenance':{'origin':'engine/shadPS4/src/core/libraries/videoout','revision':'e3ce810f3a653f43ac64ebab63023de281a4103a','license':'GPL-2.0-or-later','files':['video_out.cpp','driver.cpp','videoout_error.h'],'use':'public signatures, error constants, whole mode memset/write behavior; new infra owner transport/bridges'},
 'hashes':{p:sha(p) for p in ['ctrport/video_session_owner.hpp','ctrport/kernel_services.asm','ctrport/entry_boundary.hpp','ctrport/main.cpp','ctrport/video/tiled_session.hpp','build/CTRPort-data/ctr_video.lib','build/CTRPort-infra/ctrport.exe']},
 'active_compatibility_stubs':cold['active_stubs'],'actually_called_compatibility_stubs':cold['called_stubs'],
 'runtime':{'R0':True,'R1':True,'R2_historically_accepted':True,'R3':False,'game_frames':0,'title':False,'menu':False,'track':False,'race':False},
 'reconstruction':{'engine_replacements':0},
 'limits':['Registration creates retained Vulkan image resources with undefined contents; not rendering or GPU coherence.','GetBufferLabelAddress and UnregisterBuffers verified synthetically only; not yet called in retail.','ModeSetAny preserves public below4 behavior and sizes beyond32; checked transport rejects above16MiB explicitly.','ConfigureOutputMode remains missing pending real presenter SDR Any admission from data.','Unsupported dimensions/formats, negative/empty registration domains and host failures fail loudly, not success.','No baseline, game dump, parent input or prof material modified. No common suite started.']}
(root/'reports/reconstruction/infra/videoout_tiled_guest_bringup.json').write_text(json.dumps(r,indent=2)+'\n')
print(json.dumps({'services':len(r['services']),'next':r['next_boundary']['nid'],'R3':False}))
