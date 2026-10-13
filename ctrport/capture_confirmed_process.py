# SPDX-License-Identifier: GPL-2.0-only
# Narrow external-helper check: exact cold PID, executable hash, foreground client only.
import json,sys,time,ctypes,subprocess,hashlib,datetime
from pathlib import Path
pid=int(sys.argv[1]);active=json.loads(Path('runtime/user/logs/infra_cold_active.json').read_text());assert active['pid']==pid
exe=Path(active['exe']);assert hashlib.sha256(exe.read_bytes()).hexdigest()==active['exe_sha256']
api=ctypes.WinDLL('user32');api.GetForegroundWindow.restype=ctypes.c_void_p
kernel=ctypes.WinDLL('kernel32');kernel.OpenProcess.restype=ctypes.c_void_p
handle=kernel.OpenProcess(0x1000,False,pid);assert handle,'exact runtime PID unavailable'
kernel.QueryFullProcessImageNameW.argtypes=[ctypes.c_void_p,ctypes.c_ulong,ctypes.c_wchar_p,ctypes.POINTER(ctypes.c_ulong)]
name=ctypes.create_unicode_buffer(32768);length=ctypes.c_ulong(len(name));assert kernel.QueryFullProcessImageNameW(handle,0,name,ctypes.byref(length))
assert Path(name.value).resolve()==exe.resolve()
results=[];seen=0;deadline=time.monotonic()+160
log=Path('runtime/user/logs/json2_compat.stdout.log');position=0;pending=b''
while time.monotonic()<deadline and seen<2:
 with log.open('rb') as f:f.seek(position);pending+=f.read();position=f.tell()
 lines=pending.split(b'\n');pending=lines.pop()
 for line in lines:
  if b'"scope":"frame_observation_hold"' not in line:continue
  event=json.loads(line);assert event['real_present'];seen+=1
  foreground=ctypes.c_ulong();api.GetWindowThreadProcessId(api.GetForegroundWindow(),ctypes.byref(foreground))
  identity={'frame':seen,'pid':pid,'exe_sha256':active['exe_sha256'],'UTC':datetime.datetime.now(datetime.timezone.utc).isoformat(),'foreground_pid':foreground.value,'focus_changed':False,'trigger':'same cold real_present observation hold','execution_context':'narrow helper outside sandbox; no elevation/admin'}
  if foreground.value!=pid:results.append(dict(identity,status='not_foreground_no_capture'));continue
  destination=Path('runtime/user/captures')/f'infra_external_cold_{pid}_frame_{seen}.bmp'
  r=subprocess.run([str(exe.parent/'capture_client.exe'),str(pid),str(destination)],capture_output=True,text=True,timeout=8)
  results.append(dict(identity,exit=r.returncode,stderr=r.stderr[:1024],path=str(destination) if not r.returncode else None,sha256=hashlib.sha256(destination.read_bytes()).hexdigest() if not r.returncode else None))
  Path(f'runtime/user/logs/infra_external_capture_{pid}.json').write_text(json.dumps(results,indent=2)+'\n',encoding='utf8')
 time.sleep(.2)
Path(f'runtime/user/logs/infra_external_capture_{pid}.json').write_text(json.dumps(results,indent=2)+'\n',encoding='utf8')
print(json.dumps(results))
