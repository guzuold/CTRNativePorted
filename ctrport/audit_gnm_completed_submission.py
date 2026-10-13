# SPDX-License-Identifier: GPL-2.0-only
import json,hashlib,datetime
from pathlib import Path
from tools.lane_lock import lane_lock
with lane_lock('shared','infra'):
 source=Path('reports/reconstruction/infra/shareplay_pending_startup.json');r=json.loads(source.read_text());e=r['events']
 def rows(scope):return [x for x in e if x.get('scope')==scope]
 assert (r['exit']==4 or (r['exit']=='diagnostic_timeout_killed' and r['timeout_seconds']==120)) and r['engine_replacements']==0
 assert hashlib.sha256(Path(r['exe_path']).read_bytes()).hexdigest()==r['exe_sha256']
 complete=rows('original_GnmSubmitFlip_ordered_submission_complete');assert len(complete)==1
 c=complete[0];assert c['status']==0 and c['completed_packets']==c['packet_count']==2346 and not c['pending'] and c['word']==10356 and c['label_retirement_verified'] and c['real_present']
 returned=rows('original_GnmSubmitFlip_completed_service_return');assert len(returned)==1 and returned[0]['result']==0 and returned[0]['all_owned_commands_completed'] and returned[0]['all_resources_cleaned']
 lock=rows('original_GnmSubmitFlip_surface_lock');assert len(lock)==1 and lock[0]['status']==0 and lock[0]['current_lock_retained_after_real_completion'] and lock[0]['provider_completion_retirement']
 pixels=rows('original_GnmSubmitFlip_publication_pixel_values');assert len(pixels)==1 and pixels[0]['nonzero_RGB_DWORDs']==0 and pixels[0]['guest_bytes']==8355840 and pixels[0]['full_checked_GPU_snapshot']
 for scope in ['original_GnmSubmitFlip_color_lifecycle','original_GnmSubmitFlip_image_allocation_lifecycle','original_GnmSubmitFlip_buffer_lifecycle','original_GnmSubmitFlip_sampler_lifecycle']:
  items=rows(scope);assert items and all(x['status']==0 for x in items)
 boundaries=rows('original_startup_libc_dependency_boundary')
 result=dict(scope='retail_first_fully_completed_Gnm_submission',timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=True,exe_sha256=r['exe_sha256'],source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),complete=complete,service_return=returned,label=lock,black_GPU_publication=pixels,next_boundaries=boundaries[-2:],R3=False,engine_replacements=0,active_compatibility_stubs=r['active_stubs'],called_compatibility_stubs=r['called_stubs'],limitation='Opt-in completed-service continuation, strict one DCB and empty CCB; SharePlay main remains parked. Black source GPU generation, no visible game-content claim. First completion retains current slot0 lock; provider retires previous only. No synthetic IRQ.')
 Path('reports/reconstruction/infra/gnm_first_completed_submission.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(passed=True,complete=complete,next_boundaries=boundaries[-2:])) )
