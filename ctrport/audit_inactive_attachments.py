# SPDX-License-Identifier: GPL-2.0-only
import datetime,hashlib,json
from pathlib import Path
from tools.lane_lock import lane_lock
with lane_lock('shared','infra'):
 source=Path('runtime/user/logs/submit_flip_census.jsonl');rows=[json.loads(x) for x in source.read_text().splitlines()]
 selected=[x for x in rows if x.get('word')==3428]
 def event(name):
  found=[x for x in selected if x['scope']=='original_GnmSubmitFlip_'+name];assert len(found)==1;return found[0]
 resource=event('draw_resource_inputs');assert resource['status']==0 and resource['vertices']==2 and resource['descriptor_complete']
 coherence=event('ordered_source_coherence');assert coherence['status']==0
 dynamic=event('first_dynamic_state');assert dynamic['status']==0 and not dynamic['depth_test'] and not dynamic['stencil_test']
 pipeline=event('first_pipeline');assert pipeline['status']==88 and not pipeline['owned_token']
 pending=event('no_attachment_pipeline_pending');assert pending['status']==88 and pending['original_key_preserved'] and pending['depth_stencil_disabled']
 handoff=event('ordered_route_handoff');assert handoff['status']==88 and handoff['completed_packets']==756 and handoff['resource_matches']==1
 startup=json.loads(Path('reports/reconstruction/infra/shareplay_pending_startup.json').read_text());assert startup['exit']==4
 controls={name:json.loads(Path('runtime/user/logs/'+file).read_text(encoding='utf-8-sig')) for name,file in [('resources','infra_inactive_draw_resources_control.json'),('depth_publication','infra_depth_guest_writeback_control.json')]};assert all(x['passed'] for x in controls.values())
 report=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=True,resource=resource,coherence=coherence,dynamic=dynamic,pipeline=pipeline,pending=handoff,controls=controls,exe_sha256=startup['exe_sha256'],census_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),eboot_sha256='ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1',R3=False,reconstructed_replacements=0,limitations='Resource/source preparation only for original3428; no GPU execution or attachment substitution. Depth transaction controls use actual owned guest memory and synthetic download/fault callbacks; real depth/stencil GPU download/retile remains missing.')
 Path('reports/reconstruction/infra/inactive_attachment_preparation_retail.json').write_text(json.dumps(report,indent=2)+'\n');print('Original3428 real resource/source preparation and explicit missing no-attachment pipeline passed')
