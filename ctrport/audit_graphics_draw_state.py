# SPDX-License-Identifier: GPL-2.0-only
import json,datetime,hashlib
from pathlib import Path
from tools.lane_lock import lane_lock
with lane_lock('shared','infra'):
 source=Path('runtime/user/logs/submit_flip_census.jsonl');rows=[json.loads(x) for x in source.read_text().splitlines()]
 names=['first_viewports','first_dynamic_state','first_dynamic_preflight','first_draw_parameters','first_draw_execution_bank','first_push_resources','first_push_data','first_shader_writeback_requirements']
 found={}
 for name in names:
  matches=[x for x in rows if x.get('scope')=='original_GnmSubmitFlip_'+name and x.get('word')==255];assert len(matches)==1
  x=matches[0];assert x['word']==255 and not x['GPU_execution'];found[name]=x
  if 'status' in x:assert x['status']==0
 assert found['first_draw_execution_bank']['count']==3 and found['first_draw_execution_bank']['initiator']==2
 assert found['first_draw_execution_bank']['compute_unchanged'] and found['first_draw_execution_bank']['original_bank_retained']
 assert found['first_viewports']['count']==1
 assert found['first_push_resources']['userdata_count']==0 and found['first_push_resources']['buffer_count']==3
 assert found['first_push_data']['same_pass_userdata_match'] and found['first_push_data']['payload_bytes']==120
 values=[x for x in rows if x.get('scope')=='original_GnmSubmitFlip_first_viewport_value' and x.get('word')==255];assert len(values)==1
 assert values[0]['x']==0 and values[0]['y']==1 and values[0]['width']==64 and values[0]['height']==-1
 assert values[0]['scissor_width']==64 and values[0]['scissor_height']==1
 report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'passed':True,'first_draw':found,'viewports':values,'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'companion_sha256':{v:hashlib.sha256(Path(f'build/CTRPort-data/ctr_graphics_companion_{v}.dll').read_bytes()).hexdigest() for v in ['v25','v26','v27','v28']},'R3':False,'engine_replacements':0}
 Path('reports/reconstruction/infra/graphics_draw_state_retail.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
