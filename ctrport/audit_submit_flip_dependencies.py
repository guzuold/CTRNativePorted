# SPDX-License-Identifier: GPL-2.0-only
"""Metadata census of one captured original submission, no execution credit."""
import collections,datetime,hashlib,json,re
from pathlib import Path
root=Path(__file__).resolve().parents[1]
source=root/'runtime/user/logs/submit_flip_ordered.json';raw=source.read_bytes();record=json.loads(raw)
packets=record['packets'];assert record['decode_status']==0 and not record['effects']
assert len(packets)==2346 and sum(p[1] for p in packets)==10420
assert all(a[0]+a[1]==b[0] for a,b in zip(packets,packets[1:]))
names={int(v,16):n for n,v in re.findall(r'(\w+)\s*=\s*(0x[0-9a-fA-F]+)',(root/'engine/shadPS4/src/video_core/amdgpu/pm4_opcodes.h').read_text())}
groups=collections.defaultdict(list)
for word,length,opcode in packets:groups[opcode].append((word,length))
graphics=[json.loads(line) for line in (root/'runtime/user/logs/submit_flip_graphics.jsonl').read_text().splitlines()]
routes=record['execution_routes']
assert [(x['word'],x['opcode']) for x in routes]==[(w,op) for w,n,op in packets if op in (0x15,0x27,0x2d)]
assert len(routes)==58 and record['register_apply_failures_full_submission']==0
assert all(x.get('vertex_status',0)==0 and x.get('compute_status',0)==0 and x.get('pixel_status',0) in (0,88) for x in routes)
assert routes[0]['vertex_code']==next(x['code_address'] for x in graphics if x['vertex'])
assert routes[0]['pixel_code']==next(x['code_address'] for x in graphics if not x['vertex'])
dma=record['dma_before_first_draw']
for event in dma:
 assert any(w==event['word'] and n==7 and op==0x50 for w,n,op in packets)
 assert event['word']<min(x['word'] for x in graphics)
 event['destination_matches_shader']=[x['vertex'] for x in graphics if event.get('destination_address')==x['code_address']]
 event['source_matches_shader']=[x['vertex'] for x in graphics if event.get('source_address')==x['code_address']]
 event['public_liverpool_skips_destination_0x3022c']=event.get('destination_address')==0x3022c
report={'scope':'retail_first_submit_flip_dependency_census','timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'source':str(source.relative_to(root)),'source_sha256':hashlib.sha256(raw).hexdigest(),'packets':len(packets),'dwords':10420,'contiguous':True,
 'opcodes':[{'opcode':op,'public_name':names.get(op,'type2_or_unclassified'),'count':len(xs),'first_word':xs[0][0],'last_word':xs[-1][0],'packet_lengths':sorted(set(x[1] for x in xs))} for op,xs in sorted(groups.items(),key=lambda kv:kv[1][0][0])],
 'execution_commands':[{'word':w,'length':n,'opcode':op,'public_name':names[op]} for w,n,op in packets if op in (0x15,0x27,0x2d)],'dma_before_first_draw':dma,'execution_routes':routes,
 'unique_stage_code_counts':{key:len({x[key] for x in routes if key in x and x[key]}) for key in ['vertex_code','pixel_code','compute_code']},'register_apply_failures_full_submission':record['register_apply_failures_full_submission'],
 'public_source':{'path':'engine/shadPS4/src/video_core/amdgpu/pm4_cmds.h and pm4_opcodes.h','revision':'e3ce810f3a653f43ac64ebab63023de281a4103a','license':'GPL-2.0-or-later'},
 'effects':False,'payload_persisted':False,'R3':False,'limitation':'Observation only: no DMA execution, command semantics, resources, ordering, writer coherence or GPU admission inferred. First draw precedes first compute.',
 'repeat':'python -B ctrport/audit_submit_flip_dependencies.py under shared lock after fresh pending replay'}
(root/'reports/reconstruction/infra/submit_flip_dependency_census.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'opcodes':len(groups),'dma_before_first_draw':dma,'R3':False}))
