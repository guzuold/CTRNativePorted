# SPDX-License-Identifier: GPL-2.0-only
"""Authored atomic buffer-token input joins, no GPU descriptor writes."""
import datetime,hashlib,json,subprocess
from pathlib import Path
exe=Path('build/CTRPort-infra/ctrport-host-compiler.exe')
run=subprocess.run([str(exe),'--graphics-buffer-writes-control'],capture_output=True,text=True,timeout=10)
assert run.returncode==0,(run.returncode,run.stderr,run.stdout)
event=json.loads(run.stdout)
assert event['negative_atomic']==6 and event['sharp_collision_resolved'] and event['explicit_null_clip_planes']
assert event['synthetic'] and not event['descriptor_writes']
report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'control':event,'exit_code':0,'R3':False,'engine_replacements':0,'GPU_execution':False}
Path('reports/reconstruction/infra/graphics_buffer_write_inputs_control.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(event))
