# SPDX-License-Identifier: GPL-2.0-only
"""Per-route captured input evidence; never an execution admission decision."""
import collections
import hashlib
import json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
base=root/'reports/reconstruction/infra'
names=['shareplay_pending_startup','descriptor_capture_census','image_allocation_lifecycle','index_capture_census','buffer_initialization_lifecycle','depth_allocation_lifecycle','paired_graphics_census','paired_layout_lifecycle','buffer_write_inputs','image_classification','buffer_descriptor_updates','sampler_descriptor_updates','image_descriptor_updates']
raw={n:(base/(n+'.json')).read_bytes() for n in names}
r,d,a,i,b,z,p,l,w,c,u,t,h=[json.loads(raw[n]) for n in names]
assert all(x['exe_sha256']==r['exe_sha256'] for x in (d,a,i,b,z,p,l,w,c,u,t,h))
assert hashlib.sha256((root/r['exe_path']).read_bytes()).hexdigest()==r['exe_sha256']
ordered=next(e for e in r['events'] if e.get('scope')=='original_GnmSubmitFlip_ordered')
stages={(e['word'],e['vertex']):e.get('owned_stage',e.get('owned_compiled_stage')) for e in r['events'] if e.get('scope') in ('original_GnmSubmitFlip_first_graphics','original_GnmSubmitFlip_graphics_census')}
alloc={(e['word'],e['vertex'],e['sharp_index']):e for e in a['observations']}
indices={e['word']:e for e in i['events']}
paired={e['word']:e for e in p['rows']}
attached={e['word']:e for e in p['cpu_input_attachments']}
depth={e['word']:e for e in z['rows']}
layouts={e['word']:e for e in l['rows']}
writes={e['word']:e for e in w['rows']}
updates={e['word']:e for e in u['rows']}
sampler_updates={e['word']:e for e in t['rows']}
image_updates={e['word']:e for e in h['rows']}
classified={(e['word'],e['vertex'],e['sharp_index']):e for e in c['rows']}
registers=json.loads((base/'pair_register_ownership.json').read_text());colors=json.loads((base/'pair_color_inputs.json').read_text())
assert registers['exe_sha256']==colors['exe_sha256']==r['exe_sha256']
color_rows={x['word']:x for x in colors['rows']}
attachments=json.loads((base/'attachment_joins.json').read_text());assert attachments['exe_sha256']==r['exe_sha256']
attachment_rows={x['word']:x for x in attachments['rows']}
register_words={x[3] for x in registers['identities']}
vertex_only=json.loads((base/'vertex_only_retail.json').read_text());assert vertex_only['exe_sha256']==r['exe_sha256']
vertex_rows={x['word']:x for x in vertex_only['rows']};vertex_layouts={x['word']:x for x in vertex_only['layouts']}
rows=[]
for route in ordered['execution_routes']:
 word=route['word'];op=route['opcode']
 row={'word':word,'opcode':op,'gpu_admission':False,'blockers':['ordered GPU submission and completion','source writer coherence']}
 if op in (0x15,0x16):
  row['blockers']+=['compute compilation and resource capture','GPU dispatch binding']
 else:
  pair=[stages[(word,v)] for v in (True,False)]
  vertex=vertex_rows.get(word,{})
  row['genuine_vertex_only_compiled']=vertex.get('status')==0
  row['genuine_vertex_only_descriptor_set_written']=vertex_layouts.get(word,{}).get('descriptor_set_written',False)
  row['compiled_stage_count']=sum(s is not None for s in pair)
  row['cpu_complete_stage_count']=sum(s is not None and s['cpu_input_capture_status']==0 for s in pair)
  row['captured_shader_buffers']=sum(len(s['buffers']) for s in pair if s)
  images=[e for e in d['descriptor_observations'] if e['word']==word and e['kind']==1]
  row['image_descriptors']=len(images)
  row['image_gpu_roundtrips']=sum(bool(alloc.get((word,e['vertex'],e['sharp_index']),{}).get('upload')) for e in images)
  row['image_missing_roundtrips']=len(images)-row['image_gpu_roundtrips']
  initialized=[e for e in b['rows'] if e['word']==word]
  row['standalone_initialized_buffers']=len(initialized)
  row['standalone_initialized_buffer_bytes']=sum(e['bytes'] for e in initialized)
  row['paired_compiler_status']=paired[word]['status']
  row['paired_original_bank_owned']=word in register_words
  row['paired_color_input_status']=color_rows.get(word,{}).get('status')
  row['paired_color_inputs_owned']=color_rows.get(word,{}).get('owned_colors',0)
  attachment=attachment_rows.get(word,{})
  row['live_color_attachment_tokens']=attachment.get('color_tokens',0)
  row['live_depth_attachment_token']=attachment.get('depth_token',False)
  row['selected_dynamic_rendering_authority']=attachment.get('dynamic_rendering_authority',88)
  row['attachment_content_coherent']=False
  row['paired_CPU_inputs_attached']=attached.get(word,{}).get('attached',False)
  row['standalone_paired_layout_status']=layouts.get(word,{}).get('create_status')
  prepared=writes.get(word,{})
  row['buffer_write_input_status']=prepared.get('status')
  row['prepared_live_buffer_references']=sum(not q['null_clip_planes'] for q in prepared.get('writes',[]))
  row['prepared_null_clip_planes']=sum(q['null_clip_planes'] for q in prepared.get('writes',[]))
  row['nonnull_invalid_image_types']=sum(not classified[(word,e['vertex'],e['sharp_index'])]['type_valid'] and not classified[(word,e['vertex'],e['sharp_index'])]['null_reasons'] for e in images)
  if row['nonnull_invalid_image_types']:row['blockers'].append('non-null invalid image type: explicit88, no null fallback')
  row['buffer_descriptors_written']=updates.get(word,{}).get('write_status')==0
  row['sampler_descriptors_written']=sampler_updates.get(word,{}).get('write_status')==0
  row['image_descriptors_written']=image_updates.get(word,{}).get('write_status')==0
  row['descriptor_set_written']=layouts.get(word,{}).get('descriptor_set_written',False)
  row['standalone_depth_allocation_status']=depth.get(word,{}).get('create_status')
  row['standalone_depth_clear_status']=depth.get(word,{}).get('clear_status')
  row['standalone_depth_content_initialized']=depth.get(word,{}).get('clear_status')==0
  row['depth_content_initialized']=False
  row['captured_index_bytes']=indices[word]['bytes'] if op==0x27 else 0
  row['blockers']+=['GPU buffer binding','attachment creation and binding','graphics pipeline and draw']
  required=1 if row['genuine_vertex_only_compiled'] else 2
  if row['cpu_complete_stage_count']!=required:row['blockers'].append('incomplete required shader stage inputs')
  if row['image_missing_roundtrips']:row['blockers'].append('image format/tiling upload')
  if images:row['blockers'].append('sampled image descriptor binding')
  if op==0x27:row['blockers'].append('original index GPU buffer binding')
 rows.append(row)
assert len(rows)==58 and len({x['word'] for x in rows})==58
assert sum(x['opcode'] in (0x15,0x16) for x in rows)==15
assert sum(x.get('captured_index_bytes',0) for x in rows)==408
out={'scope':'retail_route_input_dependency_matrix','eboot_sha256':d['eboot_sha256'],'exe_sha256':r['exe_sha256'],'sources':{n:hashlib.sha256(b).hexdigest() for n,b in raw.items()},'routes':rows,'blocker_route_counts':dict(collections.Counter(b for x in rows for b in x['blockers'])),'active_stubs':r['active_stubs'],'called_stubs':r['called_stubs'],'R3':False,'engine_replacements':0,'payload_persisted':False,'limitation':'Metadata of diagnostic parked-SharePlay path only. GPU roundtrips refer to synchronous standalone texture transfers retained with descriptor layouts until owner callback completion; no draw has consumed them. No binding, in-flight game GPU use, absent-stage no-op, coherent guest writeback, flip or visible frame is admitted.','repeat':'python -B ctrport/audit_gpu_route_inputs.py'}
(base/'gpu_route_input_dependencies.json').write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps(out['blocker_route_counts']))
