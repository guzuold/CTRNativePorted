# SPDX-License-Identifier: GPL-2.0-only
import datetime,hashlib,json
from pathlib import Path
from tools.lane_lock import lane_lock

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
with lane_lock('shared','infra'):
 source=Path('runtime/user/logs/submit_flip_census.jsonl')
 rows=[json.loads(x) for x in source.read_text().splitlines()]
 words=[2574,2812,3055]
 draws=[x for x in rows if x.get('scope')=='original_GnmSubmitFlip_first_GPU_draw' and x.get('word') in words]
 assert [x['word'] for x in draws]==words
 assert all(x['status']==0 and x['invoked'] and x['real_fence'] and x['GPU_execution'] and x['vertices']==3 and x['vertex_buffers']==2 for x in draws)
 assert [x['completed_packets'] for x in draws]==[615,671,713]
 writes=[x for x in rows if x.get('scope')=='original_GnmSubmitFlip_color_guest_writeback' and x.get('word') in words]
 assert [(x['word'],x['guest_bytes']) for x in writes]==[(2574,1114112),(2812,131072),(3055,131072)]
 assert all(x['status']==0 and x['actual_GPU_retiled_generation'] and not x['poisoned'] for x in writes)
 checks=[x for x in rows if x.get('scope')=='original_GnmSubmitFlip_completed_writer_sampled_readback' and x.get('word') in words]
 assert [x['word'] for x in checks]==words
 assert [x['compared_pixel_bytes'] for x in checks]==[829440,82944,82944]
 assert all(x['status']==0 and x['pixel_differences']==x['padding_differences']==0 and x['real_game_writer_completed'] for x in checks)
 admissions=[x for x in rows if x.get('scope')=='original_GnmSubmitFlip_sampled_source_admission' and x.get('word') in words]
 assert len(admissions)==3 and all(x['status']==0 and x['fresh_guest_and_writer_generation_checked'] for x in admissions)
 manifest=json.loads(Path('build/CTRPort-infra/ctrport.exe.build.json').read_text(encoding='utf-8-sig'))
 assert sha(Path(manifest['executable']))==manifest['executable_sha256']
 pins=manifest['graphics_companions'];assert len(pins)==7
 assert all(sha(Path(x['path']))==x['sha256'] for x in pins)
 report=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=True,draws=draws,writes=writes,full_sampled_comparisons=checks,source_admissions=admissions,executable_sha256=manifest['executable_sha256'],pinned_companions=pins,census_sha256=sha(source),R3=False,reconstructed_replacements=0,limitations='Original retail GPU draws and full output publication; not shader golden equivalence. Next D16 single-sample vertex-only draw3242 remains pending. No frame or flip.')
 Path('reports/reconstruction/infra/sampled_color_chain_retail.json').write_text(json.dumps(report,indent=2)+'\n')
 print('Three real sampled color draws, full guest generations, comparisons and seven pinned compiler DLLs passed')
