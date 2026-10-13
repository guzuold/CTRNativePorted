# SPDX-License-Identifier: GPL-2.0-only
"""Audit actual selected-session allocation, without claiming image contents."""
import hashlib
import json
from pathlib import Path

root=Path(__file__).resolve().parents[1]
base=root/'reports/reconstruction/infra'
source=base/'shareplay_pending_startup.json'
raw=source.read_bytes()
r=json.loads(raw)
assert hashlib.sha256((root/r['exe_path']).read_bytes()).hexdigest()==r['exe_sha256']
observations=[e for e in r['events'] if e.get('scope')=='original_GnmSubmitFlip_image_allocation_observation']
summaries=[e for e in r['events'] if e.get('scope')=='original_GnmSubmitFlip_image_allocation_lifecycle']
assert len(summaries)==1
summary=summaries[0]
assert summary['peak_live_images']==53 and summary['released_after_layouts']
assert summary['status']==0 and summary['owner_thread_dispatch']
assert len(observations)==summary['attempted']==66
assert len({(e['word'],e['vertex'],e['sharp_index']) for e in observations})==66
created=[e for e in observations if e['create_status']==0]
unsupported=[e for e in observations if e['create_status']==88]
assert len(created)==summary['created']==summary['released']>0
assert len(unsupported)==summary['unsupported'] and len(created)+len(unsupported)==66
assert all(e['release_status']==0 and e['owned_view_contract'] and e['release_deferred_to_batch_end'] for e in created)
assert all(not e['owned_view_contract'] for e in unsupported)
assert all(e['release_status']==88 for e in unsupported)
assert not any(summary[k] for k in ('descriptor_binding','inflight_GPU_work','gpu_execution'))
assert all(not e['gpu_execution'] for e in observations)
descriptor_source=base/'descriptor_capture_census.json'
descriptor_raw=descriptor_source.read_bytes()
descriptors=json.loads(descriptor_raw)
assert descriptors['exe_sha256']==r['exe_sha256']
images={(d['word'],d['vertex'],d['sharp_index']):d for d in descriptors['descriptor_observations'] if d['kind']==1}
assert len(images)==67
for event in observations:
 d=images[(event['word'],event['vertex'],event['sharp_index'])]
 assert d['image_status']==d['image_extent_status']==0 and d['image_payload_owned']
 event['parameters']=d['image_parameters']
 event['owned_source_bytes']=d['image_copy']['copied_bytes']
 if event['upload']:
  assert event['create_status']==event['release_status']==event['upload_status']==0
  assert event['readback_status']==event['pixel_differences']==event['padding_differences']==0
  assert event['readback_pixel_bytes']==event['parameters']['width']*event['parameters']['height']*4
  assert event['compared_pixel_bytes']==(event['readback_pixel_bytes'] if event['parameters']['tile_mode']==8 else 0)
  assert event['source_tile_mode']==event['parameters']['tile_mode'] and event['source_tile_mode'] in (8,14)
  assert event['parameters']['data_format']==10 and event['parameters']['number_format']==0
 else:assert event['upload_status']==88
uploaded=[e for e in observations if e['upload']]
assert sum(e['source_tile_mode']==8 for e in uploaded)==summary['linear_uploads_fence_completed']==5
assert sum(e['source_tile_mode']==14 for e in uploaded)==summary['tiled_uploads_fence_completed']==48
assert len(uploaded)==53 and summary['upload']
# This one descriptor is not passed to allocation; preserve the upstream failure.
upstream_missing=[{k:d[k] for k in ('word','vertex','sharp_index','image_status','image_extent_status')} for d in images.values() if d['image_status'] or d['image_extent_status']]
assert len(upstream_missing)==1 and upstream_missing[0]['image_status']==88
out={'scope':'retail_selected_session_sampled_image_allocation',
 'eboot_sha256':'ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1',
 'exe_sha256':r['exe_sha256'],'source_sha256':hashlib.sha256(raw).hexdigest(),
 'descriptor_source_sha256':hashlib.sha256(descriptor_raw).hexdigest(),
 'summary':summary,'observations':observations,'upstream_missing':upstream_missing,
 'active_stubs':r['active_stubs'],
 'called_stubs':r['called_stubs'],'R3':False,'engine_replacements':0,'payload_persisted':False,
 'limitation':'53 actual VkImage/device-local allocations; five LinearAligned RGBA8 owned snapshots uploaded and read back through synchronous GPU transfers with fence completion. Exact pixel comparison and untouched padding verified; no payload persisted. 48 tile14 images detiled and uploaded by the separate whole-public API, with fenced readback and padding checks; actual tiled pixel values have no independent retail oracle comparison yet. All tokens released after completion on owner thread. Other formats explicit88. Creation now includes an owned 2D image view; no shader binding, source writer coherence, ordered draw/dispatch or flip. SharePlay caller parked without return; not normal boot.',
 'repeat':'python -B ctrport/audit_image_allocation.py'}
(base/'image_allocation_lifecycle.json').write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps(summary))
