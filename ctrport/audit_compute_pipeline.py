# SPDX-License-Identifier: GPL-2.0-only
"""Original compute pipeline allocation and owned CPU inputs; actual ordered dispatch and guest writeback evidence."""
import datetime,hashlib,json
from pathlib import Path
from tools.lane_lock import lane_lock

def main():
 with lane_lock('shared','infra'):
  census=Path('runtime/user/logs/submit_flip_census.jsonl')
  rows=[json.loads(line) for line in census.read_text().splitlines()]
  def one(scope,word=None):
   found=[r for r in rows if r.get('scope')==scope and (word is None or r.get('word')==word)]
   assert len(found)==1,(scope,len(found))
   return found[0]
  pipeline=one('original_GnmSubmitFlip_compute_pipeline',755)
  release=one('original_GnmSubmitFlip_compute_pipeline_release',755)
  gpu_buffer=one('original_GnmSubmitFlip_compute_buffer',755)
  assert gpu_buffer['status']==0 and gpu_buffer['bytes']==16 and gpu_buffer['descriptor_matches']==1
  assert gpu_buffer['CPU_snapshot_rechecked'] and gpu_buffer['initial_copy_compared'] and not gpu_buffer['GPU_execution']
  pending=one('original_GnmSubmitFlip_ordered_route_handoff')
  storage=one('original_GnmSubmitFlip_compute_storage_allocation',755)
  assert storage['status']==0 and storage['owned_token'] and not storage['contents_initialized'] and not storage['GPU_execution']
  dispatch=one('original_GnmSubmitFlip_compute_GPU_dispatch',755)
  initialized=one('original_GnmSubmitFlip_compute_storage_initialized',755)
  eos=one('original_GnmSubmitFlip_EOS_completed',768);wait=one('original_GnmSubmitFlip_post_compute_wait',773)
  assert dispatch['status']==dispatch['guest_writeback_status']==0 and dispatch['writeback_completed'] and dispatch['GPU_execution'] and dispatch['guest_bytes']==8355840
  assert initialized['status']==0 and initialized['inputs_disjoint'] and initialized['real_transfer_fence']
  assert eos['status']==wait['status']==0 and eos['after_GPU_and_guest_writeback']
  second=one('original_GnmSubmitFlip_compute_GPU_dispatch',869)
  assert second['status']==second['guest_writeback_status']==0 and second['writeback_completed'] and second['GPU_execution'] and second['guest_bytes']==8355840
  second_eos=one('original_GnmSubmitFlip_EOS_completed',882);second_wait=one('original_GnmSubmitFlip_post_compute_wait',887)
  assert second_eos['status']==second_wait['status']==0 and second_eos['after_GPU_and_guest_writeback']
  missing=one('original_GnmSubmitFlip_compute_storage_allocation',983)
  assert missing['status']==0 and missing['owned_token']
  third=one('original_GnmSubmitFlip_compute_GPU_dispatch',983)
  assert third['status']==third['guest_writeback_status']==0 and third['writeback_completed'] and third['GPU_execution'] and third['guest_bytes']==4423680
  assert one('original_GnmSubmitFlip_EOS_completed',996)['status']==one('original_GnmSubmitFlip_post_compute_wait',1001)['status']==0
  assert pipeline['word']==755 and pipeline['status']==0 and pipeline['spirv_words']==871
  assert pipeline['GPU_execution'] is False and pipeline['push_status']==0
  assert pipeline['dedicated_buffer_offset_zero_only'] is True
  buffer,image=pipeline['resources']
  assert buffer['kind']==0 and buffer['written']==0 and buffer['bytes']==16
  assert buffer['owned_bytes']==16
  assert all(buffer[k]==0 for k in ('decode_status','capture_status','lease_status','release_status','lease_end_status'))
  assert (image['kind'],image['flags'],image['width'],image['height'],image['format'],image['number_format'],image['tile'],image['samples'])==(1,1,1920,1080,4,7,14,1)
  assert image['decode_status']==image['extent_status']==0 and image['bytes']==8355840
  assert image['CPU_image_capture_status']==0 and image['CPU_image_owned_bytes']==8355840 and not image['GPU_writer_coherence']
  assert release['status']==0 and release['before_resources'] is True
  assert pending['word']>983 and pending['completed_packets']>=231 and pending['status']==88
  assert rows.index(pipeline)<rows.index(release)<rows.index(pending)
  exe=Path('build/CTRPort-infra/ctrport.exe')
  report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'passed':True,'events':[pipeline,gpu_buffer,storage,initialized,dispatch,eos,wait,release,second,second_eos,second_wait,missing,third,pending],
   'exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'census_sha256':hashlib.sha256(census.read_bytes()).hexdigest(),
   'scope':'Original755 VkComputePipeline allocation/release, original resource decode and owned read-only16B CPU capture; push preparation for dedicated offset-zero buffers. Real16B GPU buffer allocation/descriptor and initial copy comparison. Original755 storage initialized/bound, actual GPU dispatch, public tiled download/retile and8355840B guest write verified; EOS768 and Wait773 pass. Original869 RGBA8 storage dispatch/writeback/EOS complete; original983 R8G8 GPU dispatch/full4423680B guest writeback/EOS/wait completed; no frame/flip.',
   'R3':False,'GPU_compute_execution':True,'engine_replacements':0}
  Path('reports/reconstruction/infra/compute_pipeline_retail.json').write_text(json.dumps(report,indent=2)+'\n')
  print(json.dumps({'passed':True,'compute_pipeline':755,'pending':pending['word'],'R3':False}))
if __name__=='__main__':main()
