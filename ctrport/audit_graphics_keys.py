# SPDX-License-Identifier: GPL-2.0-only
import datetime,hashlib,json
from pathlib import Path
source=Path('runtime/user/logs/submit_flip_census.jsonl')
events=[json.loads(x) for x in source.read_text().splitlines()]
rows=[x for x in events if x.get('scope')=='original_GnmSubmitFlip_graphics_key']
assert len(rows)==43 and len({x['word'] for x in rows})==43
assert sum(x['fragment_present'] for x in rows)==39
assert all(x['status']==0 and not x['GPU_execution'] and not x['mixed_any_enabled'] and not x['mixed_depth_enabled'] for x in rows)
assert all(x['mrt_mask']==x['color_count']==0 for x in rows if not x['fragment_present'])
startup=json.loads(Path('reports/reconstruction/infra/shareplay_pending_startup.json').read_text())
report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'exe_sha256':startup['exe_sha256'],'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'companion_sha256':hashlib.sha256(Path('build/CTRPort-data/ctr_graphics_companion_v22.dll').read_bytes()).hexdigest(),'rows':rows,'R3':False,'engine_replacements':0,'limitation':'Exact per-draw original bank and same-pass fragment MRT decoded into retained AMD POD; no Vulkan enum casts, pipeline or GPU draw. Both mixed-sample extensions disabled by current device policy.'}
Path('reports/reconstruction/infra/graphics_keys_retail.json').write_text(json.dumps(report,indent=2)+'\n')
print('43 original graphics keys retained; GPU_execution=false')
