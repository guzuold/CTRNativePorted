# SPDX-License-Identifier: GPL-2.0-only
"""Targeted offline service controls, no common suite or boot claim."""
import subprocess,json,hashlib
from pathlib import Path
exe=Path('build/CTRPort-infra/ctrport.exe')
rows=[]
for option in ['--compat-stubs-control','--user-name-control','--pad-read-state-control','--condition-destroy-control','--mutex-destroy-control','--video-gamma-control','--video-adjust-color-negative-control']:
 result=subprocess.run([str(exe),option],capture_output=True,text=True,timeout=10)
 assert result.returncode==0,(option,result.returncode,result.stderr)
 events=[]
 for line in result.stdout.splitlines():
  if line.startswith('{'):events.append(json.loads(line))
 rows.append({'option':option,'exit':result.returncode,'events':events})
sources=['share_play/shareplay.h','share_play/shareplay.cpp','system/userservice.cpp','system/userservice_error.h','np/np_manager.cpp','np/np_error.h','pad/pad.cpp','pad/pad.h','kernel/threads/condvar.cpp','kernel/threads/mutex.cpp','videoout/video_out.cpp','videoout/videoout_error.h']
report={'passed':True,'scope':'synthetic ABI/lifecycle plus actual Windows XInput disconnected polling; not original PS4 execution or engine reconstruction','exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'controls':rows,'active_compatibility_stubs':20,'user_exception':'sceSharePlayGetCurrentConnectionInfo Dormant explicitly authorized 2026-10-11','reference_revision':'e3ce810f3a653f43ac64ebab63023de281a4103a','reference_license':'GPL-2.0-or-later','references':[{'path':'engine/shadPS4/src/core/libraries/'+x,'sha256':hashlib.sha256((Path('engine/shadPS4/src/core/libraries')/x).read_bytes()).hexdigest()} for x in sources],'limitations':['PadReadState connected-device translation fails explicitly; only actually disconnected XInput is supported.','SharePlay is a declared R8 compatibility exception, not PS4 implementation.','No engine replacements.']}
Path('reports/reconstruction/infra/offline_startup_services_control.json').write_text(json.dumps(report,indent=2)+'\n')
print('offline controls passed,20 R8 active; no milestone claim')
