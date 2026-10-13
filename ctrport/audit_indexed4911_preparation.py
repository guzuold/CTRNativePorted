# SPDX-License-Identifier: GPL-2.0-only
"""Check actual partial descriptor writes and coherent original index transport.

No draw4911 or first-frame claim. Authored negative controls are recorded apart
from original retail execution; no shader/index/pixel payload is persisted.
"""
import datetime
import hashlib
import json
from pathlib import Path
import subprocess
from tools.lane_lock import lane_lock

source = Path('reports/reconstruction/infra/shareplay_pending_startup.json')
retail = json.loads(source.read_text())
exe = Path(retail['exe_path'])
sha = hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha == retail['exe_sha256'] and retail['exit'] == 4
events = retail['events']
def rows(scope):
    return [e for e in events if e['scope'] == scope and e.get('word') == 4911]
buffers = rows('original_GnmSubmitFlip_buffer_descriptor_update')
samplers = rows('original_GnmSubmitFlip_sampler_descriptor_update')
images = rows('original_GnmSubmitFlip_image_descriptor_update')
indices = rows('original_GnmSubmitFlip_indexed_transport')
handoff = rows('original_GnmSubmitFlip_indexed_draw_handoff')
vertices = rows('original_GnmSubmitFlip_indexed_vertex_generation')
vertex_ranges = rows('original_GnmSubmitFlip_indexed_vertex_ranges')
cabi=rows('original_GnmSubmitFlip_indexed_CABI_transport')
assert cabi and all(e['status']==0 and e['index_count']==102 and e['index_bytes']==2 and e['vertex_buffers']==1 and e['signed_vertex_offset']==0 and e['original_tokens_preserved'] and not e['GPU_execution'] for e in cabi)
ms_backing = rows('original_GnmSubmitFlip_indexed_color_MS_initialization')
assert len(ms_backing)==1 and all(e['status']==0 and e['compared_bytes']==8355840 and e['differences']==0 and e['sample0_roundtrip'] and e['original_key_unchanged'] and e['GPU_execution'] and not e['original_draw'] and not e['guest_writeback'] for e in ms_backing)
assert vertex_ranges and all(e['status']==0 and e['checked_bindings']==1 for e in vertex_ranges)
assert vertices and all(e['status']==0 and e['compared_bytes']==50700 for e in vertices)
rg16 = [e for e in rows('original_GnmSubmitFlip_image_allocation_observation') if e['sharp_index']==96]
rg16_before=json.loads(Path('reports/reconstruction/infra/rg16_tile13_failing_case.json').read_text())
assert all(e['create_status']==0 and e['upload_status']==88 for e in rg16_before['events'])
assert rg16 and all(e['upload_status']==e['readback_status']==e['tiled_roundtrip_status']==0 and e['tiled_compared_bytes']==16384 and e['tiled_differences']==0 for e in rg16)
assert handoff and all(e['offsets_status'] == e['dynamic_status'] == 0 and e['instance_count'] > 0 and e['original_execution_bank_retained'] for e in handoff)
assert buffers and all(e['prepare_status'] == e['write_status'] == 0 and e['buffer_binding_count'] == 3 for e in buffers)
assert samplers and all(e['prepare_status'] == e['write_status'] == 0 and e['sampler_binding_count'] == 10 for e in samplers)
assert images and all(e['prepare_status']==0 and e['write_status']==0 and e['image_binding_count']==13 and sum(x['semantic_null'] for x in e['writes'])==1 and all(x['sharp_index']==112 and not x['live_token'] for x in e['writes'] if x['semantic_null']) for e in images)
assert indices and all(e['status'] == 0 and e['count'] == 102 and e['type'] == 0 and e['bytes'] == 204 and e['live_index_token'] and e['current_guest_and_writer_generation_checked'] for e in indices)
pending = [e for e in events if e['scope'] in ('original_GnmSubmitFlip_ordered_route_handoff','original_GnmSubmitFlip_ordered_submission_complete')][-1]
if pending['scope']=='original_GnmSubmitFlip_ordered_submission_complete':
 assert pending['status']==0 and not pending['pending'] and pending['completed_packets']==pending['packet_count'] and pending['real_present'] and pending['label_retirement_verified']
assert pending['word'] >= 6309 and pending['completed_packets'] >= 1409
draws = [e for e in events if e.get('GPU_execution') and e.get('status') == 0 and e['scope'] in ['original_GnmSubmitFlip_MRT_GPU_draw', 'original_GnmSubmitFlip_first_GPU_draw', 'original_GnmSubmitFlip_vertex_only_GPU_draw', 'original_GnmSubmitFlip_no_attachment_GPU_draw']]
assert len(draws)>=22 and len({e['word'] for e in draws})==len(draws) and {4911,5026,5379,5639,5851,6052} <= {e['word'] for e in draws}
palette=[e for e in events if e['scope']=='original_GnmSubmitFlip_published_color_to_1D_sampled']
assert len(palette)==2 and all(e['status']==0 and e['guest_bytes']==32768 and e['exact_published_GPU_generation'] and e['actual_tiled_upload'] and e['guest_rechecked_after_fence'] for e in palette)
before = json.loads(Path('reports/reconstruction/infra/indexed4911_failing_case.json').read_text())
assert any(e['scope'] == 'original_GnmSubmitFlip_buffer_descriptor_update' and e['prepare_status'] == 88 for e in before['events'])
controls = []
for option in ['--graphics-buffer-writes-control', '--graphics-pair-input-control', '--indexed-draw-transport-control', '--indexed-vertex-generation-control', '--shader-capture-trace-control', '--indexed-command-parameters-control', '--indexed-vertex-ranges-control', '--indexed-call-control', '--indexed-ordered-transaction-control', '--shader-provenance-trace-control', '--effective-null-requirement-control']:
    result = subprocess.run([str(exe), option], capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, (option, result.stdout, result.stderr)
    controls.append(dict(option=option, events=[json.loads(line) for line in result.stdout.splitlines() if line.startswith('{')]))
invalid = [d for e in rows('original_GnmSubmitFlip_graphics_census') if not e['vertex'] for d in e['owned_stage']['descriptors'] if d['kind'] == 1 and d['sharp_index'] == 112]
assert invalid and all(d['image_status'] == 88 and d['descriptor_type_raw'] == 0 and not d['image_payload_owned'] and d['effective_class_status']==0 and d['effective_type_valid']==1 and d['effective_fallback_reason']==1 and d['effective_null_reasons']==1 and d['source_span_verified'] for d in invalid)
private_source = Path('private/infra_descriptor_evidence.json')
private = json.loads(private_source.read_text())
assert private['process_id']==retail['process_id'], 'Private evidence belongs to another runtime process'
assert private['capture_trace_status']==0 and private['provenance_status']==0
selected=[x for x in private['selected_sources'] if x['flat_index']==112]
assert private['span_status']==0 and len(private['selected_span'])==8
assert all(x['flat_index']==112+i and x['root_sgpr']==8 and x['source_dword']==128+i and x['source_kind']==1 for i,x in enumerate(private['selected_span']))
assert selected and all(x['pc']==1936 and x['root_sgpr']==8 and x['source_dword']==128 and x['source_kind']==1 for x in selected)
ranges=private['capture_ranges']
assert len(ranges)==3 and all(x['capture_status']==x['release_status']==0 and x['retained_bytes']==x['bytes'] for x in ranges)
root8=[(i,x) for i,x in enumerate(ranges) if x['userdata_root_candidates']==[8]]
root10=[(i,x) for i,x in enumerate(ranges) if x['userdata_root_candidates']==[10]]
assert len(root8)==len(root10)==1 and root8[0][1]['bytes']==544 and root10[0][1]['bytes']==144
assert root8[0][1]['address']+512==root10[0][1]['address']
for index,offset in [(root8[0][0],512),(root10[0][0],0)]:
 assert any(x['sharp']==112 and x['capture_index']==index and x['byte_offset']==offset and x['source_address']==ranges[index]['address']+offset and x['matched_bytes']==32 for x in private['invalid_source_matches'])
commands=rows('original_GnmSubmitFlip_indexed_command_parameters')
assert commands and all(x['status']==0 and x['index_count']==102 and x['index_type']==0 and x['first_index']==x['index_buffer_offset']==x['vertex_offset']==x['first_instance']==0 and x['instance_count']==1 for x in commands)

invalid_raw = [d for d in private['descriptors'] if d['kind']==1 and d['sharp']==112]
assert len(invalid_raw)==1 and invalid_raw[0]['status']==0 and len(invalid_raw[0]['words'])==8
words = invalid_raw[0]['words']
matching_samplers = [d['sharp'] for d in private['descriptors'] if d['kind']==2 and d['status']==0 and d['words']==words[:4]]
assert words[:4]==words[4:] and matching_samplers
private_diagnosis = dict(source_sha256=hashlib.sha256(private_source.read_bytes()).hexdigest(),
                        shader_address=private['shader_address'], process_id=private['process_id'], invalid_sharp=112,
                        capture_ranges=ranges, source_matches=private['invalid_source_matches'], trace_status=0,
                        repeated_sampler_halves=True, matching_sampler_sharps=matching_samplers, selected_sources=selected, selected_span=private['selected_span'],
                        raw_words_published=False, image_substitution_authorized=False,
                        limitation='Compiler selects PC1936/root8 contiguous DWORD128..135. Exact public GetSharp classifies invalid raw type as effective valid null(address0); genuine capability-gated null sampled descriptor is written for sharp112, no dummy resource.')
paths = ['ctrport/graphics_companion_client.hpp', 'ctrport/graphics_pair_client.hpp', 'ctrport/graphics_buffer_bindings.hpp', 'ctrport/indexed_draw_transport.hpp', 'ctrport/shader_capture_trace.hpp', 'ctrport/check_pending_startup.py', 'ctrport/entry_boundary.hpp']
report = dict(scope='retail_indexed4911_original_execution', timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(), passed=True,
              exe_sha256=sha, source_report_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
              source_hashes={p: hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in paths},
              buffer_writes=buffers, sampler_writes=samplers, image_writes=images, original_indices=indices, indexed_draw_handoff=handoff, original_vertex_generation=vertices, original_vertex_ranges=vertex_ranges, original_indexed_MS_backing=ms_backing, original_RG16_tile13=rg16, indexed_command_parameters=commands, indexed_CABI_transport=cabi,
              invalid_image=invalid, private_descriptor_diagnosis=private_diagnosis, controls=controls, pending=pending, original_GPU_draws=len(draws), published_color_to_1D=palette, R3=False, engine_replacements=0,
              limitation='Partial preparation is not draw admission. Sharp112 raw type is invalid; exact public GetSharp effective semantic null and eight-DWORD source continuity proven. Actual null sampled binding and exact published 1D palette generation verified; original4911/5026 indexed draws execute with retained stencil,5379/5639 also execute. Current pending command recorded separately; no frame or flip.')
with lane_lock('shared', 'infra'):
    Path('reports/reconstruction/infra/indexed4911_preparation_retail.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(dict(passed=True, buffer_writes=3, sampler_writes=10, original_U16_indices=102, pending_word=pending['word'], R3=False)))
