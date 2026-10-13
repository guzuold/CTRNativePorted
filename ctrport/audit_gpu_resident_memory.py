# SPDX-License-Identifier: GPL-2.0-only
import datetime,hashlib,json
from pathlib import Path
from tools.lane_lock import lane_lock
with lane_lock('shared','infra'):
 def passed(path):
  return next(json.loads(line) for line in reversed(Path(path).read_text(encoding='utf-8-sig').splitlines()) if line.startswith('{') and json.loads(line).get('passed'))
 memory=passed('runtime/user/logs/infra_gpu_resident_memory_control.json')
 gpu=passed('runtime/user/logs/infra_gpu_resident_eop_control.log')
 assert memory['actual_read_and_write_faults'] and memory['lease_retains_generation'] and memory['exact_GPU_lookup'] and memory['stale_generation_rejected'] and memory['lease_destructor_reentrancy']
 assert gpu['provider_lease_survives_original_token_release'] and gpu['actual_D32_MS2_depth_owner'] and gpu['actual_GPU_clear'] and gpu['real_queue_fence'] and gpu['EOP_label_verified'] and gpu['CPU_read_write_faults'] and gpu['second_real_GPU_generation'] and gpu['exact_address_lookup'] and not gpu['guest_depth_publication'] and gpu['authored_page_not_retail_span']
 manifest=json.loads(Path('build/CTRPort-infra/ctrport.exe.build.json').read_text(encoding='utf-8-sig'))
 sha=hashlib.sha256(Path('build/CTRPort-infra/ctrport.exe').read_bytes()).hexdigest();assert sha==manifest['executable_sha256']
 report=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=True,exe_sha256=sha,memory=memory,GPU_EOP=gpu,source_sha256={f:hashlib.sha256(Path(f).read_bytes()).hexdigest() for f in ['ctrport/gpu_resident_memory.hpp','ctrport/gnm_write_data.hpp','ctrport/entry_boundary.hpp']},retail_residency_policy_active=False,retail_pending_word=3622,R3=False,reconstructed_replacements=0,limitations='Actual GPU D32/MS2 image, two actual clear generations, fence/EOP/retained sampled alias; guest page and backing identity are authored, not a retail span mapping. CPU guards and diagnostics are wired but no retail page is protected yet. Cross-submission genuine image lease and validated stencil padded span remain data handoff; original EOP retains missing D32/stencil publication until this lifetime contract is installed. Shutdown restores permission only after guest execution has joined, never a publication.')
 Path('reports/reconstruction/infra/gpu_resident_memory_control.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(dict(passed=True,retail_policy_active=False,R3=False)))
