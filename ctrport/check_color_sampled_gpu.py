# SPDX-License-Identifier: GPL-2.0-only
import datetime,hashlib,json,subprocess
from pathlib import Path
exe=Path('build/CTRPort-infra/ctrport-host-compiler.exe')
r=subprocess.run([str(exe),'--color-sampled-gpu-control'],capture_output=True,text=True,timeout=45)
assert r.returncode==0,(r.returncode,r.stdout,r.stderr)
e=next(json.loads(x) for x in r.stdout.splitlines() if x.startswith('{'))
assert e['completed_writers']==e['transfers']==3 and not e['pixel_differences'] and not e['guard_differences'] and not e['game_draw']
report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'control':e,'R3':False,'engine_replacements':0,'limitation':'Authored UNORM128x64 color uploads and completed fences, not original game draw writers; actual transfer and sampled readback through public data-owned API.'}
Path('reports/reconstruction/infra/color_sampled_gpu_control.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(e))
