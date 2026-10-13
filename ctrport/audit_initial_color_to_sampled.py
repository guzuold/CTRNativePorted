# SPDX-License-Identifier: GPL-2.0-only
import datetime,hashlib,json,collections
from pathlib import Path
source=Path('runtime/user/logs/submit_flip_census.jsonl')
events=[json.loads(x) for x in source.read_text().splitlines()]
rows=[x for x in events if x.get('scope')=='original_GnmSubmitFlip_initial_color_to_sampled']
identities=[x for x in events if x.get("scope")=="original_GnmSubmitFlip_color_sampled_identity"]
assert len(identities)==len(rows)
assert rows
readbacks=[x for x in events if x.get("scope")=="original_GnmSubmitFlip_color_sampled_readback"]
assert len(readbacks)==len(rows)==45
assert all(x["status"]==0 and x["compared_pixel_bytes"]>0 and x["pixel_differences"]==x["padding_differences"]==0 for x in readbacks)
assert all(x['status'] in (0,88,91) and x['standalone_initial_contents'] and not x['game_writer_completed'] and not x['GPU_execution'] for x in rows)
startup=json.loads(Path('reports/reconstruction/infra/shareplay_pending_startup.json').read_text())
report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'exe_sha256':startup['exe_sha256'],'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'rows':rows,'identities':identities,'readbacks':readbacks,'compared_pixel_bytes':sum(x['compared_pixel_bytes'] for x in readbacks),'statuses':dict(collections.Counter(x['status'] for x in rows)),'R3':False,'engine_replacements':0,'limitation':'Initial immutable CPU snapshot projection only; no completed game writer or command replay. Data-owned exact format/extent/tiling validator may reject aliases. Successful generation cache hits need not repeat a transfer.'}
Path('reports/reconstruction/infra/initial_color_to_sampled.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report['statuses']))
