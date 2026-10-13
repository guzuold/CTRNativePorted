# SPDX-License-Identifier: GPL-2.0-only
"""Audit metadata from the optional whole retail graphics compiler census."""
import collections, hashlib, json, re
from pathlib import Path
root=Path(__file__).resolve().parents[1]
r=json.loads((root/'reports/reconstruction/infra/shareplay_pending_startup.json').read_text())
exe=Path(r['exe_path'])
assert hashlib.sha256(exe.read_bytes()).hexdigest()==r['exe_sha256']
e=[x for x in r['events'] if x.get('scope')=='original_GnmSubmitFlip_graphics_census']
canonical=[]
for line in (root/'runtime/user/logs/submit_flip_census.jsonl').read_text(errors='replace').splitlines():
 if line.startswith('{'):
  try: item=json.loads(line)
  except ValueError: continue
  if item.get('scope')=='original_GnmSubmitFlip_graphics_census':canonical.append(item)
assert canonical==e
ordered=next(x for x in r['events'] if isinstance(x.get('packets'),list))
draws=[w for w,n,op in ordered['packets'] if op in (0x27,0x2d)]
assert [(x['word'],x['vertex']) for x in e]==[(w,v) for w in draws[1:] for v in (True,False)]
assert len(e)==84
# Scope: this observed submission has no ClearState reset. Replay only the
# public target register metadata from its retained initial bank and writes.
assert not any(op==0x12 for w,n,op in ordered['packets'])
first=next(x for x in r['events'] if x.get('scope')=='original_GnmSubmitFlip_first_graphics')
def bank(t):
 b={0xa08e:t['target_mask_raw'],0xa08f:t['shader_mask_raw']}
 for c in t['color_buffers']:
  for offset,key in enumerate(('base_raw','pitch_raw','slice_raw','view_raw','info_raw','attrib_raw')):b[0xa318+15*c['slot']+offset]=c[key]
 return b
writes=ordered['render_target_writes_full_submission']
assert writes==sorted(writes,key=lambda x:x[0])
state=bank(first['render_target_initial']);cursor=0
for x in e:
 if not x['vertex']:continue
 while cursor<len(writes) and writes[cursor][0]<x['word']:
  word,index,value=writes[cursor];assert index in state
  assert any(w==word and op==0x69 for w,n,op in ordered['packets'])
  state[index]=value;cursor+=1
 assert state==bank(x['render_target_metadata'])

for x in e:
 assert x['range_begins']==x['range_ends'] and x['active_leases']==x['active_snapshots']==0
 assert not x['source_writer_coherence'] and not x['gpu_execution']
 if x['compiler_status']==0:
  assert x['state_status']==0 and x['compiler_words']>=5
  assert x['owned_stage']['owned_spirv_words']==x['compiler_words']
  assert 16<=x['owned_stage']['owned_flattened_userdata_words']<=65536
  assert x['owned_stage']['owned_flattened_userdata_words']==x['resources']['flattened_userdata_words']
  assert not x['owned_stage']['gpu_admission']
  if x['vertex']:
   targets=x['render_target_metadata']
   assert not targets['surface_admission']
   assert [c['slot'] for c in x['owned_stage']['colors']]==[c['slot'] for c in targets['color_buffers'] if c['base_raw']]
   for c in x['owned_stage']['colors']:
    if c['requirement_status']==0:assert c['address']==targets['color_buffers'][c['slot']]['base_raw']<<8
   permissions=x['color_output_permissions']
   assert len(permissions)==len(x['owned_stage']['colors'])
   for permission,color in zip(permissions,x['owned_stage']['colors']):
    assert permission['slot']==color['slot'] and not permission['output_admission']
    assert permission['queried']==(color['requirement_status']==0)
    assert permission['writable_extent']==(color['bytes'] if permission['queried'] else 0)
 else:
  assert x['compiler_status']==88 and x['compiler_words']==0
  assert x['state_status']==88 or x['diagnostic'].startswith('SRT descriptor capture traversal missing: shader has extended userdata roots') and len(x['diagnostic'])<2047
counts=collections.Counter((x['state_status'],x['compiler_status']) for x in e)
assert counts=={(0,0):80,(88,88):4}
srt_shapes=[]
for x in e:
 if x['state_status'] or not x['compiler_status']:continue
 text=x['diagnostic']
 prefix='SRT descriptor capture traversal missing: shader has extended userdata roots; '
 assert text.startswith(prefix) and len(text)<2047
 match=re.fullmatch(r'roots=(\d+) pointer_nodes=(\d+)((?: sgpr=\d+ offsets_dw=(?:\d+,)*)+)',text[len(prefix):])
 assert match, 'Missing or truncated typed SRT root diagnostic'
 roots=[]
 for sgpr,offsets in re.findall(r' sgpr=(\d+) offsets_dw=((?:\d+,)*)',match[3]):
  values=[int(v) for v in offsets.split(',') if v]
  assert values==sorted(set(values)) and all(0<=v<=0xffffffff for v in values)
  roots.append({'sgpr':int(sgpr),'offsets_dw':values})
 assert len(roots)==int(match[1]) and len({v['sgpr'] for v in roots})==len(roots)
 srt_shapes.append({'word':x['word'],'vertex':x['vertex'],'code_address':x['code_address'],'root_count':int(match[1]),'pointer_nodes':int(match[2]),'roots':roots,'diagnostic_bytes':len(text),'truncated':False,'traversal_implemented':False})
assert len(srt_shapes)==0
cpu=collections.Counter(x.get('owned_stage',{}).get('cpu_input_capture_status','no_stage') for x in e)
colors=collections.Counter((c['requirement_status'],c['initial_copy_status']) for x in e for c in x.get('owned_stage',{}).get('colors',[]))
unique={}
for x in e:
 if x['code_address']:
  key=(x['vertex'],x['code_address'])
  unique.setdefault(key,{'vertex':x['vertex'],'code_address':x['code_address'],'compiler_status':x['compiler_status'],'words':[]})['words'].append(x['word'])
out={'srt_root_shapes':srt_shapes,'render_target_writes_verified':len(writes),'cpu_input_status_counts':dict(cpu),'color_requirement_copy_counts':[{ 'requirement_status':a,'copy_status':b,'count':n} for (a,b),n in colors.items()],'unique_stage_programs':list(unique.values()),'scope':'retail_graphics_submission_compiler_census','eboot_sha256':json.loads((root/'reports/reconstruction/infra/first_draw_shader_capture.json').read_text())['eboot_sha256'],'exe_sha256':r['exe_sha256'],'additional_stage_contexts':84,'compiled_additional_contexts':80,'compiled_including_first_pair':82,'missing_extended_userdata_contexts':0,'absent_pixel_program_contexts':4,'events':e,'active_stubs':r['active_stubs'],'called_stubs':r['called_stubs'],'R3':False,'engine_replacements':0,'payload_persisted':False,'limitation':'CPU compilation and owned input diagnostics only. Images, samplers and special buffer requirements remain incomplete; reached leaf SRT contexts compile with owned flattened descriptor copies. Nested graphs remain unsupported. No ordered GPU execution or flip. Pending SharePlay caller never returns.','repeat':'CTR_GRAPHICS_CENSUS_STAGES=84 CTR_PENDING_TIMEOUT_SECONDS=120 python -B ctrport/check_pending_startup.py; python -B ctrport/audit_graphics_census.py'}
(root/'reports/reconstruction/infra/graphics_submission_compiler_census.json').write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps({k:v for k,v in out.items() if k not in ('events','called_stubs','unique_stage_programs','srt_root_shapes')}))

