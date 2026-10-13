# SPDX-License-Identifier: GPL-2.0-only
"""Actual selected-device lifetime checks with authored failure inputs."""
import datetime,hashlib,json,subprocess
from pathlib import Path
exe=Path('build/CTRPort-infra/ctrport-host-compiler.exe')
run=subprocess.run([str(exe),'--video-buffer-batch-control'],capture_output=True,text=True,timeout=30)
assert run.returncode==0,(run.returncode,run.stderr,run.stdout)
rows=[json.loads(x) for x in run.stdout.splitlines() if x.startswith('{')]
event=next(x for x in rows if x.get('scope')=='video_buffer_batch_control')
assert event['simultaneously_live']==2 and event['failure_preserves_live_buffers'] and event['exception_cleanup']
assert event['synthetic'] and not event['GPU_execution']
report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'control':event,'exit_code':0,'R3':False,'engine_replacements':0,'limitation':'Authored sources on selected Vulkan session. No descriptors, asynchronous GPU consumer, guest coherence, draw or game frame.'}
Path('reports/reconstruction/infra/video_buffer_batch_control.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(event))
