# SPDX-License-Identifier: GPL-2.0-only
"""Audit general GnmSubmit route against authored controls and a fresh retail run."""
from pathlib import Path
import json,hashlib,datetime

def events(path):
 for line in Path(path).read_text(errors='replace').splitlines():
  if line.startswith('{') and len(line)<12000:
   try:yield json.loads(line)
   except ValueError:pass
r=json.loads(Path('reports/reconstruction/infra/json2_compat_startup.json').read_text())
c=list(events('runtime/user/logs/gnm_submit_ordered_control.log'))
assert any(x.get('scope')=='gnm_submit_ordered_control' and x.get('passed') for x in c)
i=list(events('runtime/user/logs/gnm_submit_initial_regression.log'))
assert any(x.get('SysV5') and x.get('actual_EOP64') for x in i)
e=list(events(r['stdout'])); routes=[x for x in e if x.get('scope')=='original_GnmSubmit_ordered_route'];assert routes
census=list(events('runtime/user/logs/submit_flip_census.jsonl'))
pending=[x for x in census if x.get('word')==755]
assert any(x.get('scope')=='original_GnmSubmitFlip_compute_output_transfer_pending' for x in pending)
report=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),scope='real_general_GnmSubmit_route_partial_retail_execution',control_passed=True,retail_submission_completed=False,exe_sha256=r['exe_sha256'],cold_report_sha256=hashlib.sha256(Path('reports/reconstruction/infra/json2_compat_startup.json').read_bytes()).hexdigest(),controls=c,initialization_regression=i,routes=routes,next_dependency=pending,service_diagnostic=r['service_diagnostics'][-1],original_access_faults=r['original_access_faults'],active_stubs=r['active_stubs'],called_stubs={str(x['registry_index']):x['public_name'] for x in r['called_stubs']},R3=True,R4=False,R5=False,engine_replacements=0,scope_limit='Count1 DCB with empty CCB; existing ordered owner reused. No flip receipt from standalone submit. Unknown packet before EOP rejects without sentinel write. First authored control exposed missing no-draw EOP consumption; corrected by ordered transport prefix. Current retail output CPU capture88 at755; no fake output or provider-defect claim. SUP030 general architectural refactor remains deferred.')
Path('reports/reconstruction/infra/gnm_submit_general_route.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(dict(control_passed=True,retail_complete=False,pending_word=755,active_stubs=r['active_stubs'],called=len(report['called_stubs']))))
