# SPDX-License-Identifier: GPL-2.0-only
"""Check whole submission CPU retention and selected-device sampler lifecycle."""
import hashlib
import json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
base=root/'reports/reconstruction/infra'
source=base/'shareplay_pending_startup.json'
r=json.loads(source.read_text())
assert hashlib.sha256((root/r['exe_path']).read_bytes()).hexdigest()==r['exe_sha256']
def single(scope):
    values=[e for e in r['events'] if e.get('scope')==scope]
    assert len(values)==1,(scope,len(values))
    return values[0]
batch=single('original_GnmSubmitFlip_owned_batch')
sampler=single('original_GnmSubmitFlip_sampler_lifecycle')
ordered=single('original_GnmSubmitFlip_ordered')
assert batch['retention_status']==0 and batch['retained_graphics_stages']==82
assert batch['retained_routes']==len(ordered['execution_routes'])==58
assert 0<batch['retained_host_bytes']<=512*1024*1024
assert batch['compute_binaries_owned']==15 and not batch['compute_resources_owned'] and not batch['source_writer_coherence'] and not batch['gpu_execution']
assert sampler['attempted']==sampler['created']==sampler['released']==64
assert sampler['peak_live_samplers']==64 and sampler['released_after_layouts']
assert sampler['status']==0 and sampler['owner_thread_dispatch']
assert not any(sampler[k] for k in ('descriptor_binding','inflight_GPU_work','gpu_execution'))
out={'scope':'retail_submission_owner_and_sampler_lifecycle','eboot_sha256':'ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1','exe_sha256':r['exe_sha256'],'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'batch':batch,'samplers':sampler,'active_stubs':r['active_stubs'],'called_stubs':r['called_stubs'],'payload_persisted':False,'R3':False,'engine_replacements':0,'limitation':'Ephemeral CPU owners survive through the diagnostic boundary, then release. Samplers are created and destroyed without binding or in-flight GPU work. Compute compiler/resources, unsupported image formats, tiled upload and shader resource binding, source writer coherence, ordered GPU draw/dispatch and flip remain missing. Image allocations and fenced linear uploads have their separate lifecycle report. SharePlay caller is parked without return; this is not normal boot.','repeat':'python -B ctrport/audit_submission_owner.py'}
(base/'submission_owner_sampler_lifecycle.json').write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps({'retained_stages':82,'routes':58,'bytes':batch['retained_host_bytes'],'samplers_created_released':64,'R3':False}))
