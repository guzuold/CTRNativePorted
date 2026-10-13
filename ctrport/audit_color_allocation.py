# SPDX-License-Identifier: GPL-2.0-only
"""Actual selected-session undefined color attachment backing, never a frame."""
import datetime,hashlib,json
from pathlib import Path
source=Path('runtime/user/logs/submit_flip_census.jsonl');events=[json.loads(x) for x in source.read_text().splitlines()]
rows=[x for x in events if x.get('scope')=='original_GnmSubmitFlip_color_allocation']
life=next(x for x in events if x.get('scope')=='original_GnmSubmitFlip_color_lifecycle')
assert rows and life['status']==0 and life['released_after_layouts']
for x in rows:
 assert x['status'] in (0,88) and not x['GPU_execution']
 assert x['live_token']==(x['status']==0) and x['requires_blend_capability']
assert life['created']==life['released']==sum(x['status']==0 and not x.get('reused_backing',False) for x in rows)
startup=json.loads(Path('reports/reconstruction/infra/shareplay_pending_startup.json').read_text())
exe=Path(startup['exe_path']);sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha==startup['exe_sha256']
report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'exe_sha256':sha,'build_input_manifest':json.loads(exe.with_name(exe.name+'.build.json').read_text(encoding='utf-8-sig')),'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'rows':rows,'lifecycle':life,'R3':False,'engine_replacements':0,'limitation':'Actual image/view backing extent only, always requires blend-capable format conservatively. CPU snapshot initialization is recorded separately; no guest writer coherence, LOAD, pipeline, draw or flip.'}
Path('reports/reconstruction/infra/color_allocation_lifecycle.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(life))
