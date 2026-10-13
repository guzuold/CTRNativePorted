# SPDX-License-Identifier: GPL-2.0-only
import json,hashlib,datetime
from pathlib import Path
from tools.lane_lock import lane_lock
with lane_lock('shared','infra'):
 source=Path('reports/reconstruction/infra/shareplay_pending_startup.json');r=json.loads(source.read_text());e=r['events']
 def rows(scope):return [x for x in e if x.get('scope')==scope and x.get('word')==10323]
 assert r['exit']==4 and not r['R3'] and r['engine_replacements']==0
 assert hashlib.sha256(Path(r['exe_path']).read_bytes()).hexdigest()==r['exe_sha256']
 pipeline=rows('original_GnmSubmitFlip_compute_pipeline');assert len(pipeline)==1 and pipeline[0]['status']==0
 image=[x for x in pipeline[0]['resources'] if x['kind']==1];assert len(image)==1 and image[0]['tile']==10 and image[0]['extent_status']==0 and image[0]['bytes']==8355840 and image[0]['CPU_image_capture_status']==0
 initialized=rows('original_GnmSubmitFlip_compute_storage_initialized');assert len(initialized)==1 and initialized[0]['status']==0 and initialized[0]['real_transfer_fence'] and initialized[0]['inputs_disjoint']
 dispatch=rows('original_GnmSubmitFlip_compute_GPU_dispatch');assert len(dispatch)==1 and dispatch[0]['status']==dispatch[0]['guest_writeback_status']==0 and dispatch[0]['writeback_completed'] and dispatch[0]['guest_bytes']==8355840 and dispatch[0]['GPU_execution']
 stdout=Path('runtime/user/logs/shareplay_park.stdout.log');surface_events=[json.loads(x) for x in stdout.read_text().splitlines() if x.startswith('{') and 'original_VideoOut_registered_surface_identity' in x]
 surfaces=[x for x in surface_events if x.get('scope')=='original_VideoOut_registered_surface_identity' and x.get('slot')==0]
 assert len(surfaces)==1 and surfaces[0]['guest_address']==image[0]['address'] and surfaces[0]['width']==1920 and surfaces[0]['height']==1080 and surfaces[0]['pitch']==1920
 arguments=[x for x in e if x.get('scope')=='original_GnmSubmitFlip_arguments'];assert arguments and arguments[0]['video_handle']==1 and arguments[0]['buffer_index']==0 and arguments[0]['flip_mode']==1 and arguments[0]['stack_owned']
 released=rows('original_GnmSubmitFlip_compute_pipeline_release');assert released and released[0]['status']==0
 pending=[x for x in e if x.get('scope') in ('original_GnmSubmitFlip_ordered_route_handoff','original_GnmSubmitFlip_ordered_submission_complete')][-1];assert pending['word']>=10356 and pending['completed_packets']>=2345
 if pending.get('scope')=='original_GnmSubmitFlip_ordered_submission_complete':assert pending['status']==0 and not pending['pending'] and pending['completed_packets']==pending['packet_count'] and pending['real_present'] and pending['label_retirement_verified']
 result=dict(scope='retail_display10_original_compute_dispatch',timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=True,exe_sha256=r['exe_sha256'],source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),pipeline=pipeline,initialized=initialized,dispatch=dispatch,released=released,registered_surface=surfaces,stdout_sha256=hashlib.sha256(stdout.read_bytes()).hexdigest(),original_flip_arguments=arguments,pending=pending,R3=False,engine_replacements=0,limitation='Actual original shader dispatch and real guest writeback; PrepareFlip not completed, no visible frame. Parked SharePlay diagnostic.')
 Path('reports/reconstruction/infra/display10_dispatch_retail.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(passed=True,pending=pending)))
