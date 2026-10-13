# SPDX-License-Identifier: GPL-2.0-only
import json,hashlib,datetime
from pathlib import Path
from tools.lane_lock import lane_lock
with lane_lock('shared','infra'):
 source=Path('runtime/user/logs/submit_flip_census.jsonl');rows=[json.loads(l) for l in source.read_text().splitlines()]
 aliases=[r for r in rows if r['scope']=='original_GnmSubmitFlip_graphics_depth_alias_prepared']
 assert [(r['word'],r['sharp']) for r in aliases]==[(2336,0),(5379,0),(5639,0),(9263,32)]
 assert all(r['status']==0 and r['same_retained_GPU_owner'] and not r['CPU_snapshot_uploaded'] and not r['GPU_execution'] for r in aliases)
 binding=next(r for r in rows if r['scope']=='original_GnmSubmitFlip_image_descriptor_update' and r['word']==2336)
 assert binding['prepare_status']==binding['layout_status']==binding['write_status']==0 and binding['image_binding_count']==3
 transfers=[r for r in rows if r['scope']=='original_GnmSubmitFlip_MRT_writer_to_sampled']
 assert len(transfers)>=8 and [r['word'] for r in transfers[:8]]==[1377]*3+[2075]*3+[2336]*2
 assert all(r['status']==0 and r['compared_pixel_bytes']==8294400 and r['pixel_differences']==r['padding_differences']==0 and r['real_transfer_fence'] for r in transfers)
 report=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=True,aliases=aliases,binding=binding,transfers=transfers,census_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),exe_sha256=hashlib.sha256(Path('build/CTRPort-infra/ctrport.exe').read_bytes()).hexdigest(),R3=False,engine_replacements=0,scope='Retained real depth aliases and descriptor writes only; original2336 executed. Eight actual MRT writer-to-sampled full logical-view comparisons after real fences, no tiled pixel dumps or shader golden equivalence claim. Depth/stencil guest publication remains missing.')
 Path('reports/reconstruction/infra/graphics_depth_alias_retail.json').write_text(json.dumps(report,indent=2)+'\n');print('retained graphics depth aliases and eight actual MRT sampled transfers passed')
