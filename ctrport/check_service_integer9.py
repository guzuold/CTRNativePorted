# SPDX-License-Identifier: GPL-2.0-only
import subprocess,json,hashlib
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.lane_lock import lane_lock
exe=Path('build/CTRPort-infra/ctrport-host-compiler.exe');rows=[]
for name,expected in [('service-integer9-control',0),('service-integer9-dll-control',0),('service-integer9-failure-control',4),('service-integer9-signature-control',4),('service-integer9-stale-control',4),('service-abi-control',0),('service-import-record-control',0),('service-transport-control',0),('service-generation-control',0)]:
 args=[str(exe),'--'+name]
 if name=='service-integer9-dll-control':args+=['build/CTRPort-infra']
 r=subprocess.run(args,capture_output=True,text=True,timeout=8);assert r.returncode==expected,(name,r.returncode,r.stderr)
 rows.append({'control':name,'exit':r.returncode,'stdout':r.stdout.strip(),'stderr':r.stderr.strip()})
with lane_lock('shared','infra'):
 Path('reports/reconstruction/infra/service_integer9_control.json').write_text(json.dumps({'scope':'resident_SysV9_Win64_transport_controls','exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'cases':rows,'synthetic':True,'PS4_service_implemented':False,'R3':False,'engine_replacements':0,'repeat':'python -B ctrport/check_service_integer9.py'},indent=2)+'\n')
print('9 affected controls passed including3 fail-loud process cases')
