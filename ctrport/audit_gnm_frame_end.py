# SPDX-License-Identifier: GPL-2.0-only
import json,hashlib,datetime
from pathlib import Path
from tools.lane_lock import lane_lock
with lane_lock('shared','infra'):
 source=Path('reports/reconstruction/infra/shareplay_pending_startup.json');r=json.loads(source.read_text());events=r['events']
 assert (r['exit']==4 or (r['exit']=='diagnostic_timeout_killed' and r['timeout_seconds']==120)) and r['engine_replacements']==0
 assert hashlib.sha256(Path(r['exe_path']).read_bytes()).hexdigest()==r['exe_sha256']
 ends=[e for e in events if e.get('scope')=='original_GnmSubmitDone_frame_end'];assert ends
 first=ends[0];assert first['status']==0 and first['frame']==1 and first['completion_count']==1 and first['completion_token']==1 and first['real_GPU_idle'] and first['commands_and_resources_completed'] and first['retained_DCB_CCB_offsets']==0 and first['next_submission_fresh_local_state'] and not first['synthetic_IRQ']
 assert any(e.get('scope')=='original_GnmSubmitFlip_completed_service_return' and e['result']==0 and e['all_resources_cleaned'] for e in events)
 rows=[e for e in events if e.get('scope')=='original_GnmSubmitFlip_RGBA8_occupancy'];assert rows and all(not e['pixel_payload_persisted'] and e['mapped_padding_included'] for e in rows)
 display=[e for e in rows if e['word']==10323 and e['origin']=='completed_compute_writer'];assert len(display)==1 and display[0]['RGB_nonzero_DWORDs']==0 and display[0]['bytes']==8355840
 boundaries=[e for e in events if e.get('scope')=='original_startup_libc_dependency_boundary']
 provider=Path('reports/reconstruction/data/tiled_frame_boundary_control.json');assert json.loads(provider.read_text())['passed']
 control=Path('runtime/user/logs/infra_gnm_frame_end_control.log');authored=json.loads(control.read_text());assert authored['passed'] and authored['frames']==2 and not authored['guest_success_bound']
 result=dict(scope='retail_bounded_sceGnmSubmitDone',timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=True,exe_sha256=r['exe_sha256'],source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),source_hashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [Path("ctrport/gnm_frame_end.hpp"),Path("ctrport/rgba8_occupancy.hpp"),Path("ctrport/entry_boundary.hpp"),Path("ctrport/video_session_owner.hpp")]},provider_report_sha256=hashlib.sha256(provider.read_bytes()).hexdigest(),authored_transaction_control=authored,actual_end=ends,RGBA8_occupancy=rows,next_boundaries=boundaries[-2:],next_runtime_failure=(None if r["exit"]=="diagnostic_timeout_killed" else (r.get("runtime_failures") or r["stderr_tail"])[-1]),R3=False,engine_replacements=0,active_compatibility_stubs=r['active_stubs'],called_compatibility_stubs=r['called_stubs'],limitation='Strict synchronous opt-in oneDCB/emptyCCB fully completed path, actual fence/device idle. Command captures/local register state scoped to submission; no retained DCB/CCB offsets. No cache emulation or queued/asynchronous GPU support claimed; SharePlay state query parked. Mapped padding included in occupancy, no image content persisted.')
 Path('reports/reconstruction/infra/gnm_frame_end_retail.json').write_text(json.dumps(result,indent=2)+'\n')
 print(json.dumps(dict(passed=True,ends=ends,next_boundaries=boundaries[-2:],next_runtime_failure=(None if r["exit"]=="diagnostic_timeout_killed" else (r.get("runtime_failures") or r["stderr_tail"])[-1]),RGBA8_occupancy_rows=len(rows))))
