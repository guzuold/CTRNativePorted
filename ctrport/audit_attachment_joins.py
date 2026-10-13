# SPDX-License-Identifier: GPL-2.0-only
"""Same selected callback attachment/layout lifetimes, not GPU admission."""
import datetime,hashlib,json
from pathlib import Path
source=Path('runtime/user/logs/submit_flip_census.jsonl');events=[json.loads(x) for x in source.read_text().splitlines()]
rows=[x for x in events if x.get('scope')=='original_GnmSubmitFlip_attachment_join']
pairs=[x for x in events if x.get('scope')=='original_GnmSubmitFlip_graphics_pair' and x['status']==0]
depths={x['word']:x for x in events if x.get('scope')=='original_GnmSubmitFlip_depth_allocation'}
colors={x['word']:x for x in events if x.get('scope')=='original_GnmSubmitFlip_pair_color_inputs'}
assert len(rows)==len(pairs)==39 and [x['word'] for x in rows]==[x['word'] for x in pairs]
for x in rows:
 assert x['layout_token'] and x['live_depths']==8 and not x['GPU_execution']
 assert x['color_status']==colors[x['word']]['status']
 assert x['color_tokens']==colors[x['word']]['owned_colors']
 d=depths.get(x['word']);assert x['depth_token']==bool(d and d['create_status']==0)
 assert x['depth_standalone_initialized']==bool(d and d['clear_status']==0)
 if d:assert d['release_deferred_to_batch_end']
authority=next(x for x in events if x.get('scope')=='original_GnmSubmitFlip_dynamic_rendering_authority')
assert authority['status'] in (0,88) and authority['selected_session'] and not authority['GPU_execution']
assert all(x['dynamic_rendering_authority']==authority['status'] for x in rows)
life=next(x for x in events if x.get('scope')=='original_GnmSubmitFlip_depth_batch')
assert life['status']==0 and life['peak_live_depths']==8 and life['released_after_layouts']
startup=json.loads(Path('reports/reconstruction/infra/shareplay_pending_startup.json').read_text());exe=Path(startup['exe_path'])
sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha==startup['exe_sha256']
report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'exe_sha256':sha,'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'rows':rows,'depth_batch':life,'dynamic_rendering_authority':authority,'R3':False,'engine_replacements':0,'limitation':'Same-callback opaque token joins only. Color CPU snapshot initialization is audited separately, depth clear standalone; no guest LOAD/HTILE/writer coherence, pipeline binding, draw or flip.'}
Path('reports/reconstruction/infra/attachment_joins.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'pairs':len(rows),'paired_depth_tokens':sum(x['depth_token'] for x in rows),'depth_batch':life}))
