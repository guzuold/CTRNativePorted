# SPDX-License-Identifier: GPL-2.0-only
import subprocess,json,datetime,hashlib
from pathlib import Path
from tools.lane_lock import lane_lock
with lane_lock('shared','infra'):
 exe=Path('build/CTRPort-infra/ctrport.exe');controls=[]
 for flag in ['--graphics-viewport-transport-control','--graphics-push-resources-control','--graphics-draw-parameter-transport-control']:
  r=subprocess.run([str(exe),flag],capture_output=True,text=True,timeout=20)
  assert r.returncode==0,(flag,r.returncode,r.stdout,r.stderr)
  row=json.loads(r.stdout.strip());assert row['passed'] and row['invalid_atomic'];controls.append(row)
 report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'executable_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'controls':controls,'R3':False,'engine_replacements':0}
 Path('reports/reconstruction/infra/graphics_draw_state_control.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
