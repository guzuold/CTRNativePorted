# SPDX-License-Identifier: GPL-2.0-only
"""Retail ordered compute evidence; compilation, dispatch and frame are distinct."""
import datetime,hashlib,json
from pathlib import Path
from ctrport.retail_pending import ordered_pending
from tools.lane_lock import lane_lock
with lane_lock('shared','infra'):
 source=Path('runtime/user/logs/submit_flip_census.jsonl');rows=[json.loads(x) for x in source.read_text().splitlines()]
 def one(name,word):
  found=[r for r in rows if r.get('scope')=='original_GnmSubmitFlip_'+name and r.get('word')==word];assert len(found)==1,(name,word);return found[0]
 compiled=[one('first_compute_compiled',755)]+[one('compute_compiled',word) for word in (869,983,1087)]
 for r in compiled:assert r['status']==0 and r['owned_after_DLL_unload'] and not r['GPU_execution']
 assert compiled[-1]['spirv_words']==1011 and compiled[-1]['descriptor_count']==3
 resources=compiled[-1]['resources'];assert len(resources)==3
 assert resources[0]['kind']==0 and resources[0]['written']==0 and resources[0]['bytes']==resources[0]['owned_bytes']==32
 assert [(r['sharp'],r['flags'],r['format'],r['number_format']) for r in resources[1:]]==[(0,0,10,0),(16,1,10,0)]
 dispatched=[]
 for word,eos,wait,byte_count in ((755,768,773,8355840),(869,882,887,8355840),(983,996,1001,4423680),(1087,1100,1105,8355840)):
  result=one('compute_GPU_dispatch',word);assert result['status']==result['guest_writeback_status']==0 and result['guest_bytes']==byte_count and result['writeback_completed'] and result['GPU_execution']
  signal=one('EOS_completed',eos);poll=one('post_compute_wait',wait)
  assert signal['status']==poll['status']==0 and signal['after_GPU_and_guest_writeback']
  assert rows.index(signal)<rows.index(poll)<rows.index(result)
  dispatched.append({'dispatch':result,'EOS':signal,'wait':poll})
 pending=ordered_pending(rows);assert len(pending)==1 and pending[0]['status']==88
 assert pending[0]['completed_packets']>=373 and pending[0]['word']>1643
 additional=[r for r in rows if r.get('scope')=='original_GnmSubmitFlip_compute_GPU_dispatch' and r.get('word',0)>=1426]
 assert [r['word'] for r in additional[:4]]==[1426,1499,1571,1643]
 assert all(r['status']==r['guest_writeback_status']==0 and r['writeback_completed'] and r['GPU_execution'] for r in additional)
 refresh=[r for r in rows if r.get('scope')=='original_GnmSubmitFlip_compute_to_sampled'];assert len(refresh)>=1
 refresh=[r for r in refresh if r['writer_word']==1087 and r['consumer_word']==1377];assert len(refresh)==1
 assert refresh[0]['writer_word']==1087 and refresh[0]['consumer_word']==1377 and refresh[0]['status']==0 and refresh[0]['compared_guest_bytes']==8355840 and refresh[0]['real_transfer_fences'] and refresh[0]['original_writer_guest_verified']
 color=[r for r in rows if r.get('scope')=='original_GnmSubmitFlip_compute_to_color'];assert len(color)>=1
 color=[r for r in color if r['writer_word']==983 and r['consumer_word']==1377];assert len(color)==1
 assert color[0]['writer_word']==983 and color[0]['consumer_word']==1377 and color[0]['slot']==1 and color[0]['status']==0 and color[0]['compared_backing_bytes']==4423680 and color[0]['real_transfer_fences']
 report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'passed':True,'compiled':compiled,'completed':dispatched,'additional_completed':additional,'compute_to_sampled':refresh[0],'compute_to_color':color[0],'pending':pending[0],'census_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'exe_sha256':hashlib.sha256(Path('build/CTRPort-infra/ctrport.exe').read_bytes()).hexdigest(),'scope':'At least eight original compute dispatches, real tiled guest writebacks and EOS/waits verified. Fourth dispatch uses real32B readonly buffer and sampled/storage images; exact next sampled owner refreshed and fully compared.','R3':False,'engine_replacements':0}
 Path('reports/reconstruction/infra/compute_chain_retail.json').write_text(json.dumps(report,indent=2)+'\n');print('ordered compute chain retail evidence passed')
