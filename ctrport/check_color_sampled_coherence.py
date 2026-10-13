# SPDX-License-Identifier: GPL-2.0-only
import datetime,hashlib,json,subprocess
from pathlib import Path
exe=Path('build/CTRPort-infra/ctrport-host-compiler.exe')
r=subprocess.run([str(exe),'--color-sampled-coherence-control'],capture_output=True,text=True,timeout=20)
assert r.returncode==0,(r.returncode,r.stdout,r.stderr)
e=next(json.loads(x) for x in r.stdout.splitlines() if x.startswith('{'))
assert e['passed'] and e['pending_writer_blocks_stale_read'] and e['failed_transfer_retry'] and e['generation_deduplication']
report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'control':e,'R3':False,'engine_replacements':0,'limitation':'Authored completion callbacks; no original GPU writer is completed by this control.'}
Path('reports/reconstruction/infra/color_sampled_coherence_control.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(e))
