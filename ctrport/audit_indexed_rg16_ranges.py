# SPDX-License-Identifier: GPL-2.0-only
"""Audit retail draw10451 range admission separately from provider execution."""
from pathlib import Path
import datetime, hashlib, json
cold = json.loads(Path('reports/reconstruction/infra/json2_compat_startup.json').read_text())
exe_sha = hashlib.sha256(Path('build/CTRPort-infra/ctrport.exe').read_bytes()).hexdigest()
assert cold['exe_sha256'] == exe_sha
control = json.loads(Path('runtime/user/logs/indexed_vertex_ranges_0148_control.log').read_text())
assert control['passed'] and control['atomic_rejections'] == 14
scopes = {'original_GnmSubmitFlip_vertex_preflight', 'original_GnmSubmitFlip_indexed_transport',
 'original_GnmSubmitFlip_indexed_vertex_generation', 'original_GnmSubmitFlip_indexed_vertex_ranges',
 'original_GnmSubmitFlip_MRT_GPU_draw', 'original_GnmSubmitFlip_first_GPU_draw', 'original_GnmSubmitFlip_ordered_route_handoff'}
events = []
census = Path('runtime/user/logs/submit_flip_census.jsonl')
for line in (census.read_text(errors='replace').splitlines() if census.exists() else []):
 if len(line) > 4000: continue
 try: event = json.loads(line)
 except ValueError: continue
 if event.get('word') == 10451 and event.get('scope') in scopes: events.append(event)
format_observed = any(e['scope'].endswith('vertex_preflight') and e.get('status') == 0 and any(v.get('vk_format') == 83 and v.get('stride') == 40 and v.get('buffer_size') == 160 for v in e.get('inputs', [])) for e in events)
transport_exact = any(e['scope'].endswith('indexed_transport') and e.get('status') == 0 and e.get('count') == 6 and e.get('bytes') == 12 and e.get('minimum') == 0 and e.get('maximum') == 3 and e.get('current_guest_and_writer_generation_checked') for e in events)
generation_checked = any(e['scope'].endswith('indexed_vertex_generation') and e.get('status') == 0 and e.get('compared_bytes') == 320 for e in events)
admitted = format_observed and transport_exact and generation_checked and any(e['scope'].endswith('indexed_vertex_ranges') and e.get('status') == 0 and e.get('checked_bindings') == 2 for e in events)
gpu = any((e['scope'].endswith('MRT_GPU_draw') or e['scope'].endswith('first_GPU_draw')) and e.get('status') == 0 and e.get('invoked') and e.get('GPU_execution') for e in events)
report = dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(), SUP='SUP-029',
 scope='indexed10451_RG16_range_admission', passed=admitted and not cold['original_access_faults'],
 format_observed=format_observed, transport_exact=transport_exact, generation_checked=generation_checked, range_admitted=admitted, GPU_draw_verified=gpu, cold_exit=cold['exit_code'], events=events, control=control,
 original_access_faults=cold['original_access_faults'], exe_sha256=exe_sha,
 source_sha256=hashlib.sha256(Path('ctrport/indexed_draw_transport.hpp').read_bytes()).hexdigest(),
 provenance='Pinned public Vulkan enum VK_FORMAT_R16G16_SFLOAT=83; shadPS4/src/video_core/amdgpu/pixel_format.cpp Format16_16 is32 bits. Project-authored bounded admission; no upstream code copied or prof used.',
 limits='Synthetic exact124-byte/short123-byte tests prove bounds only. Actual draw has two160-byte bindings stride40 and index range0..3. Provider GPU execution is separate and must pass before crediting draw completion. No visual equivalence, R4/R5/audio or reconstruction credit.',
 R4=False, R5=False, engine_replacements=0)
Path('reports/reconstruction/infra/indexed_rg16_ranges.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps({'passed':report['passed'], 'GPU_draw_verified':gpu, 'events':events}))
