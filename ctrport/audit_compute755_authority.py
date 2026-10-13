# SPDX-License-Identifier: GPL-2.0-only
"""Repeat the current partial image-lease authority audit after a diagnostic cold run."""
from pathlib import Path
import json,datetime,hashlib
now=datetime.datetime.now(datetime.timezone.utc).isoformat();r=json.loads(Path('reports/reconstruction/infra/json2_compat_startup.json').read_text())
e=[json.loads(l) for l in Path('runtime/user/logs/submit_flip_census.jsonl').read_text().splitlines() if '"scope":"compute_output_capture_authority"' in l]
assert len(e)==1 and e[0]['lease_end_status']==0 and e[0]['suffix_guarded'] and e[0]['suffix_begin']==73170878464 and e[0]['owned_bytes']==2064384
p=Path('reports/reconstruction/infra/compute755_capture_authority.json');d=json.loads(p.read_text());d['current_suffix_owner_cold']={'exe_sha256':r['exe_sha256'],'events':e,'cold_report_sha256':hashlib.sha256(Path('reports/reconstruction/infra/json2_compat_startup.json').read_bytes()).hexdigest()};d['interpretation']='Exact readable lease stops at a current retained GPU depth guard. Capture callback not invoked because lease is shorter than full R32 tile14 extent. Need genuine overlapping backing materialization or validated write-only overwrite/owner transition; do not unguard stale CPU bytes. No provider format defect.';d['active_stubs']=r['active_stubs'];d['called_stubs']={str(x['registry_index']):x['public_name'] for x in r['called_stubs']};p.write_text(json.dumps(d,indent=2)+'\n')
