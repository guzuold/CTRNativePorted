# SPDX-License-Identifier: GPL-2.0-only
"""Reached depth attachment metadata; never authorizes image allocation."""
import json,hashlib,collections,datetime
from pathlib import Path
source=Path('runtime/user/logs/submit_flip_ordered.json')
x=json.loads(source.read_text());routes=x['execution_routes'];assert len(routes)==58
rows=[]
for route in routes:
 d=route['depth_attachment'];assert not d['allocation_extent_validated'] and not d['gpu_admission']
 assert d['samples'] in (1,2,4,8) and 0<=d['tile_mode']<8
 for kind in ('z','stencil'):
  for direction in ('read','write'):
   address=d[kind+'_'+direction+'_address'];assert address%256==0
   assert d[kind+'_'+direction+'_valid']==bool(address and d['z_format' if kind=='z' else 'stencil_format'])
 assert 0<=d['depth_clear_float_bits']<=0xffffffff and 0<=d['stencil_clear_raw']<=0xffffffff
 assert d['depth_clear_enable'] in (0,1) and d['stencil_clear_enable'] in (0,1)
 assert all(0<=d[key]<=0xffffffff for key in ('stencil_control_raw','stencil_front_ref_mask_raw','stencil_back_ref_mask_raw'))
 for face,shift in (('front',0),('back',12)):
  decoded=d['stencil_'+face];raw=d['stencil_control_raw']>>shift;reference=d['stencil_'+face+'_ref_mask_raw']
  assert decoded['fail']==raw&15 and decoded['zpass']==(raw>>4)&15 and decoded['zfail']==(raw>>8)&15
  assert decoded['reference']==reference&255 and decoded['compare_mask']==(reference>>8)&255 and decoded['write_mask']==(reference>>16)&255 and decoded['operation_value']==reference>>24
  assert decoded['compare']==d['stencil_'+face+'_compare']
 assert d['pitch']%8==0 and d['height']%8==0
 rows.append({'word':route['word'],'opcode':route['opcode'],**d})
draws=[r for r in rows if r['opcode']!=21]
report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'scope':'actual retail retained register depth metadata; diagnostic SharePlay parked, no GPU execution',
 'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'routes':len(rows),'draw_routes':len(draws),
 'enabled_depth_draws':sum(r['depth_enable']!=0 for r in draws),'enabled_stencil_draws':sum(r['stencil_enable']!=0 for r in draws),
 'invalid_enabled_depth_draw_words':[r['word'] for r in draws if r['depth_enable'] and not r['z_read_valid']],
 'format_pairs':dict(collections.Counter(str((r['z_format'],r['stencil_format'])) for r in draws)),
 'rows':rows,'payload_persisted':False,'allocation_extent_validated':False,'R3':False,'engine_replacements':0,
 'public_sources':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in map(Path,('engine/shadPS4/src/video_core/amdgpu/regs_depth.h','engine/shadPS4/src/video_core/amdgpu/regs.cpp','engine/shadPS4/src/video_core/amdgpu/liverpool.cpp','engine/shadPS4/src/video_core/amdgpu/cb_db_extent.h'))}}
Path('reports/reconstruction/infra/depth_attachment_census.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k in ('routes','draw_routes','enabled_depth_draws','enabled_stencil_draws','invalid_enabled_depth_draw_words','format_pairs')}))
