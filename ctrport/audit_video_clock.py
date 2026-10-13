# SPDX-License-Identifier: GPL-2.0-only
"""Assert actual original polling consumes a genuinely completed flip clock."""
import json,hashlib,datetime,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent.parent))
from tools.lane_lock import lane_lock
with lane_lock('shared','infra'):
 source=Path('reports/reconstruction/infra/shareplay_pending_startup.json');r=json.loads(source.read_text());events=r['events']
 assert hashlib.sha256(Path(r['exe_path']).read_bytes()).hexdigest()==r['exe_sha256']
 bound=[e for e in events if e.get('scope')=='original_VideoOut_clock_bound'];assert len(bound)==1
 assert bound[0]['status']==0 and bound[0]['before_session_publication'] and bound[0]['global_kernel_clock'] and bound[0]['extra_events']==0
 polls=[e for e in events if e.get('scope')=='original_VideoOut_GetFlipStatus_observed' and e['count']>0];assert polls
 for p in polls:assert p['result']==0 and p['process_time']>0 and p['tsc']>=p['submit_tsc']>0 and p['current_buffer']>=0
 provider=Path('reports/reconstruction/data/noevent_clock_control.json');assert json.loads(provider.read_text())['passed']
 authored=json.loads(Path('runtime/user/logs/infra_video_clock_binding_control.log').read_text());assert authored['passed'] and not authored['queue_created_by_binding'] and authored['provider_failure_propagated']
 result=dict(scope='retail_original_flip_status_real_clock',passed=True,timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),exe_sha256=r['exe_sha256'],source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),binding=bound,polls=polls,provider_sha256=hashlib.sha256(provider.read_bytes()).hexdigest(),authored=authored,next_failure=r['stderr_tail'],R3=False,engine_replacements=0,limitation='SharePlay remains parked. Real status/timestamps are not evidence of visible game content.')
 Path('reports/reconstruction/infra/video_clock_retail.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
