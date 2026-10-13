# SPDX-License-Identifier: GPL-2.0-only
import json,subprocess,datetime,hashlib
from pathlib import Path
exe=Path('build/CTRPort-infra/ctrport.exe')
r=subprocess.run([str(exe),'--gnm-eop-completion-control'],capture_output=True,text=True,timeout=20)
assert r.returncode==0,(r.returncode,r.stdout,r.stderr)
row=json.loads(r.stdout.strip());assert row['passed'] and row['actual_owned_guest_writes'] and row['synthetic_completion_provider'] and not row['GPU_execution']
report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'control':row,'R3':False,'engine_replacements':0,'limitation':'Authored synthetic completion provider only; actual checked guest write32/64 and failstop memory preservation. No game GPU completion provider, interrupt delivery, GPU clock/perf or game EOP completion.'}
Path('reports/reconstruction/infra/eop_completion_control.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(row))
