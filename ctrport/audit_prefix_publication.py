# SPDX-License-Identifier: GPL-2.0-only
from pathlib import Path
import json,hashlib,datetime
cold=json.loads(Path('reports/reconstruction/infra/json2_compat_startup.json').read_text())
rows=[]
for line in Path('runtime/user/logs/submit_flip_census.jsonl').read_text(errors='replace').splitlines():
 if line.startswith('{') and len(line)<3000:
  try:rows.append(json.loads(line))
  except ValueError:pass
publication=[x for x in rows if x.get('scope')=='compute_output_depth_prefix_publication']
dispatch=[x for x in rows if x.get('scope')=='original_GnmSubmitFlip_compute_GPU_dispatch' and x.get('word')==755]
controls=[]
for line in Path('runtime/user/logs/gpu_overlap_prefix_publication_control.log').read_text().splitlines():
 if line.startswith('{'):controls.append(json.loads(line))
assert len(controls)==1 and controls[0]['passed'] and controls[0]['rejections']==6 and controls[0]['suffix_read_write_faults']==2
assert hashlib.sha256(Path('build/CTRPort-infra/ctrport.exe').read_bytes()).hexdigest()==cold['exe_sha256']
passed=bool(publication and all(x['word']==755 and x['status']==0 and x['bytes']==8355840 and x['depth_overlap_bytes']==6291456 and x['old_full_alias_invalidated'] for x in publication) and dispatch and all(x['status']==0 and x['guest_writeback_status']==0 and x['writeback_completed'] and x['completed_packets']>174 for x in dispatch) and not cold['original_access_faults'])
r=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),SUP='SUP-029',scope='fenced_original_compute_result_published_to_owned_partial_depth_backing',passed=passed,publication=publication,dispatch=dispatch,control=controls[0],exe_sha256=cold['exe_sha256'],exit_code=cold['exit_code'],terminal_boundary=cold['terminal_boundary'],terminal_service_diagnostic=cold.get('service_diagnostics',[])[-1:],original_access_faults=cold['original_access_faults'],source_sha256={p:hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in ['ctrport/gpu_resident_memory.hpp','ctrport/entry_boundary.hpp','ctrport/owner_dispatch.hpp']},active_stubs=cold['active_stubs'],distinct_called_stubs={str(x['registry_index']):x['public_name'] for x in cold['called_stubs']},synchronization_domain='GnmSubmit caller waits in OwnerDispatch.invoke result.get on the synchronous video owner. The existing host ownership mutex serializes mapping/revocation. Other guest consumers must obey game GPU/EOS ordering; publication is NOT atomic against unsynchronized guest CPU instructions and no whole-process quiescence or scheduler change is claimed.',provenance='Original infra page ownership transfer and orchestration of existing pinned provider; no new upstream/prof code.',limits='Synthetic-result real-page control verifies exclusions/ownership, not GPU conversion. Retail uses actual fenced compute/readback. No independent pixel oracle, coherent whole old depth alias, full-submit completion or R4/R5 credit. Remaining depth/stencil lease deliberately retained.',R3=True,R4=False,R5=False,engine_replacements=0)
Path('reports/reconstruction/infra/compute755_prefix_publication.json').write_text(json.dumps(r,indent=2)+'\n')
print(json.dumps({'passed':passed,'publication':publication,'dispatch':dispatch,'next':r['terminal_service_diagnostic'],'faults':r['original_access_faults']}))
