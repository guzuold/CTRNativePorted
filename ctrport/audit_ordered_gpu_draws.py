# SPDX-License-Identifier: GPL-2.0-only
import datetime,hashlib,json
from pathlib import Path
from tools.lane_lock import lane_lock
with lane_lock('shared','infra'):
 source=Path('runtime/user/logs/submit_flip_census.jsonl');rows=list(map(json.loads,source.read_text().splitlines()))
 def events(name):return [r for r in rows if r.get('scope')=='original_GnmSubmitFlip_'+name]
 draws=events('first_GPU_draw');reads=events('first_GPU_readback');created=[r for r in events('first_pipeline') if r['word'] in (255,468)]
 assert [r['word'] for r in draws]==[255,468] and [r['word'] for r in reads]==[255,468]
 assert all(r['status']==0 and r['invoked'] and r['real_fence'] and r['GPU_execution'] for r in draws)
 assert [r['completed_packets'] for r in draws]==[114,160]
 assert [r['vertex_buffers'] for r in draws]==[2,1]
 assert [r['bytes'] for r in reads]==[32768,262144]
 assert all(r['status']==0 and r['changed_bytes']>0 and r['writer_generation_completed'] and not r['pixel_payload_persisted'] for r in reads)
 releases=[r for r in events('ordered_pipeline_release') if r.get('word') in (255,468)]
 assert len(created)==len(releases)==2 and all(r['status']==0 for r in releases)
 assert rows.index(releases[0])>rows.index(reads[0]) and rows.index(releases[0])<rows.index(created[1])
 assert rows.index(releases[1])>rows.index(reads[1])
 coherence=[r for r in events('ordered_source_coherence') if r['word'] in (255,468)];assert [r['word'] for r in coherence]==[255,468]
 assert [r['prior_color_writers'] for r in coherence]==[0,1] and all(r['status']==0 for r in coherence)
 pending=events('ordered_route_handoff');assert len(pending)==1 and pending[0]['word']>983 and pending[0]['completed_packets']>=231 and pending[0]['status']==88
 vertex=events('vertex_only_pending_inputs');assert len(vertex)==1 and vertex[0]['word']==680
 assert vertex[0]['viewport_status']==vertex[0]['dynamic_status']==vertex[0]['draw_parameters_status']==0
 assert vertex[0]['depth_token'] and vertex[0]['depth_initialized'] and not vertex[0]['GPU_execution']
 push=events('vertex_only_pending_push_data');assert len(push)==1 and push[0]['status']==0 and push[0]['owned_payload'] and push[0]['buffer_count']==1 and push[0]['fragment_absent']
 refresh=events('completed_writer_to_sampled');assert len(refresh)==1 and refresh[0]['word']==468 and refresh[0]['status']==0 and refresh[0]['real_transfer_fence'] and refresh[0]['real_game_writer_completed'] and not refresh[0]['guest_writeback']
 sampled=events('completed_writer_sampled_readback');assert len(sampled)==1 and sampled[0]['status']==0 and sampled[0]['compared_pixel_bytes']==131072 and sampled[0]['pixel_differences']==sampled[0]['padding_differences']==0 and not sampled[0]['pixel_payload_persisted']
 startup=json.loads(Path('reports/reconstruction/infra/shareplay_pending_startup.json').read_text())
 depth=events('vertex_only_GPU_draw');assert len(depth)==1 and depth[0]['word']==680 and depth[0]['status']==0 and depth[0]['real_fence']
 report={'depth_draw':depth[0],'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'passed':True,'draws':draws,'readbacks':reads,'source_coherence':coherence,'completed_writer_refresh':refresh,'sampled_readback':sampled,'vertex_push_data':push[0],'pending':pending[0],'vertex_only_pending':vertex[0],'exe_sha256':startup['exe_sha256'],'census_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'R3':False,'engine_replacements':0,'scope':'Three original retail draws on real GPU/fences; no complete frame or flip. Snapshot overlap guard rejects stale CPU input after preceding color writer; sampled-resource routes remain pending.'}
 Path('reports/reconstruction/infra/ordered_gpu_draws_retail.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
