# SPDX-License-Identifier: GPL-2.0-only
"""Exact guest base identities only; never infer image intervals or GPU hazards."""
import collections
import hashlib
import json
from pathlib import Path
from audit_resource_overlap import overlap as interval_overlap
ROOT=Path(__file__).resolve().parents[1]
base=ROOT/'reports/reconstruction/infra'
paths=[base/'descriptor_capture_census.json',base/'resource_overlap_census.json',base/'shareplay_pending_startup.json']
raw=[p.read_bytes() for p in paths]
desc,overlap,startup=map(json.loads,raw)
assert desc['exe_sha256']==overlap['exe_sha256']==startup['exe_sha256']
assert hashlib.sha256((ROOT/startup['exe_path']).read_bytes()).hexdigest()==startup['exe_sha256']
assert desc['eboot_sha256']==overlap['eboot_sha256']
images=[d for d in desc['descriptor_observations'] if d['kind']==1 and not d['image_status']]
assert len(images)==66
scopes=('original_GnmSubmitFlip_first_graphics','original_GnmSubmitFlip_graphics_census')
colors=[]
for e in startup['events']:
 if e.get('scope') not in scopes or not e['vertex']:continue
 stage=e.get('owned_stage',e.get('owned_compiled_stage'))
 if not stage:continue
 for c in stage['colors']:
  if not c['requirement_status'] and not c['initial_copy_status']:
   colors.append({'word':e['word'],'slot':c['slot'],'address':c['address'],'bytes':c['bytes']})
assert len(colors)==36
matches=[]
for image in images:
 address=image['image_parameters']['guest_address']
 assert 0<address<1<<64
 for color in colors:
  if address!=color['address']:continue
  order='same_draw' if color['word']==image['word'] else 'color_before_image' if color['word']<image['word'] else 'image_before_color'
  matches.append({'image':{'word':image['word'],'vertex':image['vertex'],'sharp_index':image['sharp_index'],'address':address},'color_candidate':color,'order':order})
image_groups=collections.defaultdict(list)
for d in images:image_groups[d['image_parameters']['guest_address']].append({'word':d['word'],'vertex':d['vertex'],'sharp_index':d['sharp_index']})
intervals=[]
for d in images:
 if d['image_extent_status'] or not d['image_payload_owned']:continue
 x=d['image_extent'];assert d['image_copy']['copied_bytes']==x['bytes']
 for c in colors:
  hit=interval_overlap((x['address'],x['bytes']),(c['address'],c['bytes']))
  if hit:intervals.append({'image':{'word':d['word'],'vertex':d['vertex'],'sharp_index':d['sharp_index'],'address':x['address'],'bytes':x['bytes']},'color_candidate':c,'intersection_address':hit[0],'intersection_bytes':hit[1],'same_interval':x['address']==c['address'] and x['bytes']==c['bytes'],'order':'same_draw' if c['word']==d['word'] else 'color_before_image' if c['word']<d['word'] else 'image_before_color'})
out={'scope':'retail_image_color_equal_base_identity','eboot_sha256':desc['eboot_sha256'],'exe_sha256':desc['exe_sha256'],'source_sha256':{p.name:hashlib.sha256(b).hexdigest() for p,b in zip(paths,raw)},'image_observations':66,'color_candidates':36,'copied_image_intervals':sum(d['image_payload_owned'] for d in images),'image_color_interval_overlaps':intervals,'interval_order_counts':dict(collections.Counter(i['order'] for i in intervals)),'equal_base_pairs':matches,'order_counts':dict(collections.Counter(m['order'] for m in matches)),'image_address_groups':[{'address':a,'observations':v} for a,v in sorted(image_groups.items())],'uninterpreted_images':[{'word':d['word'],'vertex':d['vertex'],'sharp_index':d['sharp_index'],'status':d['image_status']} for d in desc['descriptor_observations'] if d['kind']==1 and d['image_status']],'active_stubs':startup['active_stubs'],'called_stubs':startup['called_stubs'],'R3':False,'engine_replacements':0,'payload_persisted':False,'gpu_admission':False,'limitation':'Equal guest base proves only identity of the starting byte. Only successful companion-admitted owned captures admit image byte intervals here; unsupported descriptors and GPU tiling conversion remain missing. Color entries are candidates, not confirmed writes. Equal intervals still do not prove image/color format compatibility or a barrier; no source-writer coherence or absence-of-hazards claim. Dispatch resources remain unknown.','repeat':'python -B ctrport/audit_image_color_identity.py'}
(base/'image_color_identity_census.json').write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps({'images':66,'distinct_bases':len(image_groups),'equal_base_pairs':len(matches),'orders':out['order_counts'],'interval_overlaps':len(intervals),'interval_orders':out['interval_order_counts']}))
