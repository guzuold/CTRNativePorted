# SPDX-License-Identifier: GPL-2.0-only
import json,hashlib,datetime
from pathlib import Path
from ctrport.retail_pending import ordered_pending
from tools.lane_lock import lane_lock
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
with lane_lock('shared','infra'):
 source=Path('runtime/user/logs/submit_flip_census.jsonl');rows=[json.loads(x) for x in source.read_text().splitlines()]
 writes=[x for x in rows if x.get('scope')=='original_GnmSubmitFlip_color_guest_writeback']
 assert [(x['word'],x['guest_bytes']) for x in writes[:8]]==[(255,32768),(468,262144),(1377,8355840),(1377,4423680),(2075,8355840),(2075,4423680),(2336,8355840),(2336,8355840)]
 assert all(x['status']==0 and x['actual_GPU_retiled_generation'] and not x['poisoned'] and not x['pixel_payload_persisted'] for x in writes)
 pending=ordered_pending(rows)
 assert len(pending)==1 and pending[0]['completed_packets']>=373 and pending[0]['status']==88
 report=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=True,exe_sha256=sha(Path('build/CTRPort-infra/ctrport.exe')),census_sha256=sha(source),writes=writes,guest_bytes=sum(x['guest_bytes'] for x in writes),pending=pending[0],R3=False,engine_replacements=0,scope='Actual retail GPU color generations transferred to sampled owner, public GPU retile, owned guest write and full guest reread. RGBA8, RG8 and R32float colors published; depth guest publication remains pending; no frame/flip and no original shader-output equivalence claim.')
 Path('reports/reconstruction/infra/color_guest_writeback_retail.json').write_text(json.dumps(report,indent=2)+'\n');print(f'{len(writes)} actual retail RGBA8/RG8/R32float guest writebacks passed')
