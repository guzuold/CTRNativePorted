# SPDX-License-Identifier: GPL-2.0-only
import datetime,hashlib,json
from pathlib import Path
source=Path('runtime/user/logs/submit_flip_census.jsonl')
events=[json.loads(x) for x in source.read_text().splitlines()]
rows=[x for x in events if x.get('scope')=='original_GnmSubmitFlip_color_initialization']
assert rows
alloc=[x for x in events if x.get('scope')=='original_GnmSubmitFlip_color_allocation']
assert len(rows)==len(alloc)
for x,a in zip(rows,alloc):
 assert (x['word'],x['slot'])==(a['word'],a['slot'])
 assert a['undefined_contents']==(x['upload_status']!=0)
 if x['initialization_reused']:assert a['reused_backing'] and x['readback_status']==88
assert rows[0]['word']==255 and rows[0]['upload_status']==0
for x in rows:
 assert x['upload_status'] in (0,88) and not x['GPU_execution']
 assert x['padding_differences']==0
 if x['upload_status']==0 and not x['initialization_reused']:assert x['readback_status']==0 and x['readback_pixel_bytes']>0
startup=json.loads(Path('reports/reconstruction/infra/shareplay_pending_startup.json').read_text())
report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'exe_sha256':startup['exe_sha256'],'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'rows':rows,'initialized_unique':sum(x['upload_status']==0 and not x['initialization_reused'] for x in rows),'initialized_reuses':sum(x['initialization_reused'] for x in rows),'readback_pixel_bytes':sum(x['readback_pixel_bytes'] for x in rows),'R3':False,'engine_replacements':0,'limitation':'Original CPU snapshots uploaded using whole public GPU detiler to color attachment owners. Linear readback is not compared to tiled bytes and is not an independent detile oracle. No guest writer coherence, command execution, pipeline, draw or flip.'}
Path('reports/reconstruction/infra/color_initialization.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:report[k] for k in ('initialized_unique','initialized_reuses','readback_pixel_bytes')}))
