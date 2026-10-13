# SPDX-License-Identifier: GPL-2.0-only
"""Paired MRT-selected CPU color input ownership, no attachment support claim."""
import datetime,hashlib,json,subprocess
from pathlib import Path
exe=Path(json.loads(Path('reports/reconstruction/infra/shareplay_pending_startup.json').read_text())['exe_path'])
assert exe.resolve().parent==Path('build/CTRPort-infra').resolve()
run=subprocess.run([str(exe),'--graphics-pair-color-control'],capture_output=True,text=True,timeout=30)
assert run.returncode==0,(run.returncode,run.stdout,run.stderr)
control=json.loads(run.stdout);assert control['negative_cases']==3 and control['zero_mrt_supported']
source=Path('runtime/user/logs/submit_flip_census.jsonl');events=[json.loads(x) for x in source.read_text().splitlines()]
rows=[x for x in events if x.get('scope')=='original_GnmSubmitFlip_pair_color_inputs']
pairs=[x for x in events if x.get('scope')=='original_GnmSubmitFlip_graphics_pair' and x['status']==0]
assert len(rows)==len(pairs)==39 and [x['word'] for x in rows]==[x['word'] for x in pairs]
for x in rows:
 assert x['status'] in (0,88) and not x['GPU_execution']
 if not x['status']:assert x['owned_colors']==x['mrt_mask'].bit_count()
 else:assert x['owned_colors']==0
startup=json.loads(Path('reports/reconstruction/infra/shareplay_pending_startup.json').read_text())
sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha==startup['exe_sha256']
report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'exe_sha256':sha,'eboot_sha256':'ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1','source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'control':control,'rows':rows,'successful_joins':sum(x['status']==0 for x in rows),'explicit_missing':sum(x['status']==88 for x in rows),'R3':False,'engine_replacements':0,'limitation':'CPU captures selected by same-pass PS MRT only; no color VkImage owner, LOAD/HTILE coherence, pipeline, draw or flip.'}
Path('reports/reconstruction/infra/pair_color_inputs.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:report[k] for k in ('successful_joins','explicit_missing')}))
