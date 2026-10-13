# SPDX-License-Identifier: GPL-2.0-only
import datetime,hashlib,json
from pathlib import Path
source=Path('runtime/user/logs/submit_flip_census.jsonl');events=[json.loads(x) for x in source.read_text().splitlines()]
row=next(x for x in events if x.get('scope')=='original_GnmSubmitFlip_command_cursor')
assert row['capture_status']==0 and row['status']==88 and row['completed_packets']>50 and row['pending_word']>190 and not row['GPU_execution']
ordered=json.loads(Path('runtime/user/logs/submit_flip_ordered.json').read_text())
assert row['packets']==len(ordered['packets']) and row['words']==sum(x[1] for x in ordered['packets'])
assert row['pending_opcode']==ordered['packets'][row['completed_packets']][2]
assert row['completed_packets']==64 and row['pending_word']==255 and row['pending_opcode']==0x2d
draw=next(x for x in events if x.get('scope')=='original_GnmSubmitFlip_ordered_draw_inputs')
assert draw['word']==255 and draw['register_words']==0xD000 and draw['register_differences']==0
assert draw['command_identity_status']==draw['key_status']==draw['fetch_status']==draw['format_status']==0
assert all(x[2] in (0x68,0x69,0x76,0x79,0x2a,0x2f,0x10,0x50) for x in ordered['packets'][:row['completed_packets']])
startup=json.loads(Path('reports/reconstruction/infra/shareplay_pending_startup.json').read_text())
dma=[x for x in startup['events'] if x.get('scope')=='original_GnmSubmitFlip_ordered_dma_ignored']
assert len(dma)==2 and all(x['destination_low']==0x3022c and x['public_handler_rule'] for x in dma)
report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'exe_sha256':startup['exe_sha256'],'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'row':row,'public_source':{'revision':'e3ce810f3a653f43ac64ebab63023de281a4103a','license':'GPL-2.0-or-later','files':['src/video_core/amdgpu/liverpool.cpp','src/video_core/amdgpu/pm4_cmds.h','src/video_core/amdgpu/regs.cpp']},'R3':False,'engine_replacements':0,'dma_ignored':dma,'pending_draw_inputs':draw,'limitation':'Lossless cursor retains every original command including barriers/DMA/EOP. The explicit public DMA destination-low 0x3022c rule also returns without a transfer; other DMA remains pending. Original register packets apply in order to cursor-owned software register state; ordinary NOPs consumed; proprietary marker hints remain pending. First unsupported packet remains pending. No GPU packet or frame completed.'}
Path('reports/reconstruction/infra/gnm_command_cursor_retail.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(row))
