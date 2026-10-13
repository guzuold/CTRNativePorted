# SPDX-License-Identifier: GPL-2.0-only
"""Retained original full banks; no pipeline state inference or GPU execution."""
import datetime,hashlib,json,subprocess
from pathlib import Path
exe=Path(json.loads(Path('reports/reconstruction/infra/shareplay_pending_startup.json').read_text())['exe_path'])
assert exe.resolve().parent==Path('build/CTRPort-infra').resolve()
run=subprocess.run([str(exe),'--graphics-pair-register-control'],capture_output=True,text=True,timeout=30)
assert run.returncode==0,(run.stdout,run.stderr)
control=json.loads(run.stdout)
assert control['mutation_isolated'] and control['words']==0xD000 and control['negative_cases']==3
source=Path('runtime/user/logs/submit_flip_census.jsonl')
rows=[json.loads(x) for x in source.read_text().splitlines()]
pairs=[x for x in rows if x.get('scope')=='original_GnmSubmitFlip_graphics_pair' and x['status']==0]
assert len(pairs)==39
identities=set()
for pair in pairs:
 assert pair['register_words']==0xD000 and pair['submission_token']>0
 assert pair['pipeline_metadata_owned'] and pair['vertex_fetch_attributes']<=32
 assert pair['compiler_profile_owned'] and pair['original_opcode'] in (0x2d,0x27)
 assert pair['original_packet_words']==(3 if pair['original_opcode']==0x2d else 6)
 key=(pair['submission_token'],pair['command_buffer_index'],pair['command_buffer_kind'],pair['word'])
 assert key not in identities;identities.add(key)
 assert not pair['gpu_execution'] and not pair['descriptor_binding']
startup=json.loads(Path('reports/reconstruction/infra/shareplay_pending_startup.json').read_text())
sha=hashlib.sha256(exe.read_bytes()).hexdigest();assert sha==startup['exe_sha256']
report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'exe_sha256':sha,'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'scope':'retail original per-pair immutable D000 banks plus authored ownership isolation','control':control,'pipeline_metadata_pairs':len(pairs),'nonempty_fetch_pairs':sum(x['vertex_fetch_attributes']>0 for x in pairs),'fetch_attribute_counts':[{'word':x['word'],'has_fetch':x['vertex_has_fetch'],'attributes':x['vertex_fetch_attributes'],'mrt_mask':x['fragment_mrt_mask'],'persp_sample':x['fragment_persp_sample'],'linear_sample':x['fragment_linear_sample']} for x in pairs],'retained_banks':len(pairs),'retained_bank_bytes':len(pairs)*0xD000*4,'identities':sorted(identities),'R3':False,'engine_replacements':0,'payload_persisted':False,'limitation':'Process-local submission tokens; executable identity pinned by report SHA and startup retail identity. Same-pass compiler MRT/sample/fetch retained via v20 pairedv2; real attachments and pipeline execution still required.'}
Path('reports/reconstruction/infra/pair_register_ownership.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'banks':len(pairs),'bytes':report['retained_bank_bytes'],'control':control}))
