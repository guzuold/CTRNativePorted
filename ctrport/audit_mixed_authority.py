# SPDX-License-Identifier: GPL-2.0-only
import datetime, hashlib, json
from pathlib import Path
from tools.lane_lock import lane_lock
with lane_lock('shared', 'infra'):
 source=Path('runtime/user/logs/submit_flip_census.jsonl')
 rows=[json.loads(line) for line in source.read_text().splitlines()]
 snapshots=[r for r in rows if r.get('scope')=='original_GnmSubmitFlip_mixed_depth_authority']
 assert len(snapshots)==1
 snapshot=snapshots[0]
 assert snapshot['status']==0 and snapshot['mask']==1 and snapshot['queried']
 assert not snapshot['AMD_supported'] and not snapshot['enabled'] and snapshot['before_shader_capture']
 keys=[r for r in rows if r.get('scope')=='original_GnmSubmitFlip_graphics_key']
 assert keys and all(rows.index(snapshot)<rows.index(key) for key in keys)
 assert all(not key['mixed_depth_enabled'] for key in keys)
 target=[key for key in keys if key['word']==1377]
 assert len(target)==1 and target[0]['samples']==target[0]['depth_samples']==2
 physical=Path('reports/reconstruction/data/mixed_depth_authority_control.json')
 control=json.loads(physical.read_text())
 assert control['status']=='passed' and control['actual_mask']==1
 pending=[r for r in rows if r.get('scope')=='original_GnmSubmitFlip_compute_coherence_pending']
 assert len(pending)==1 and pending[0]['word']==1426 and pending[0]['status']==88 and pending[0]['completed_packets']==317
 report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'passed':True,
 'snapshot':snapshot,'target_key':target[0],'pending':pending[0],
 'exe_sha256':hashlib.sha256(Path('build/CTRPort-infra/ctrport.exe').read_bytes()).hexdigest(),
 'census_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
 'physical_control_sha256':hashlib.sha256(physical.read_bytes()).hexdigest(),
 'scope':'Actual selected session queried before fresh shader/key capture. AMD absent. Conservative public two-sample fallback retained; no posthoc key edit.',
 'R3':False,'engine_replacements':0}
 Path('reports/reconstruction/infra/mixed_authority_retail.json').write_text(json.dumps(report,indent=2)+'\n')
 print('selected-device authority before retail capture passed')
