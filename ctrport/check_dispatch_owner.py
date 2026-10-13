# SPDX-License-Identifier: GPL-2.0-only
import subprocess,json,hashlib,datetime
from pathlib import Path
exe=Path('build/CTRPort-infra/ctrport.exe');rows=[]
for flag in ['--dispatch-packet-inputs-control','--graphics-submission-owner-control','--gnm-route-handoff-control']:
 result=subprocess.run([str(exe),flag],capture_output=True,text=True,timeout=20)
 assert result.returncode==0,(flag,result.returncode,result.stdout,result.stderr)
 event=json.loads(result.stdout.strip());assert event['passed'];rows.append(event)
report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'controls':rows,'synthetic':True,'GPU_execution':False,'R3':False,'engine_replacements':0}
Path('reports/reconstruction/infra/dispatch_owner_control.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(rows))
