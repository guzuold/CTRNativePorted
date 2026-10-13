# SPDX-License-Identifier: GPL-2.0-only
"""Verify descriptor and exact CPU image transport; no shader binding or draw admission."""
import collections
import hashlib
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
base = ROOT/'reports/reconstruction/infra'
raw = (base/'shareplay_pending_startup.json').read_bytes()
report = json.loads(raw)
assert hashlib.sha256((ROOT/report['exe_path']).read_bytes()).hexdigest() == report['exe_sha256']
scopes = ('original_GnmSubmitFlip_first_graphics', 'original_GnmSubmitFlip_graphics_census')
key = lambda e: (e['word'], not e['vertex'])
events = sorted((e for e in report['events'] if e.get('scope') in scopes), key=key)
canonical = []
for line in ((ROOT/'runtime/user/logs/submit_flip_graphics.jsonl').read_text()+(ROOT/'runtime/user/logs/submit_flip_census.jsonl').read_text()).splitlines():
    if line.startswith('{'):
        try:
            e = json.loads(line)
        except ValueError:
            continue
        if e.get('scope') in scopes:
            canonical.append(e)
assert events == sorted(canonical, key=key) and len(events) == 86
observations, flatbufs = [], []
for e in events:
    stage = e.get('owned_stage', e.get('owned_compiled_stage'))
    if stage is None:
        continue
    resources = e.get('resources', e.get('compiler_resources'))
    expected = [b for b in resources['bindings'] if b['kind'] in (1, 2)]
    descriptors = stage['descriptors']
    assert [(d['kind'], d['sharp_index']) for d in descriptors] == [(b['kind'], b['sharp_index']) for b in expected]
    for d in descriptors:
        assert d['status'] == 0 and d['copied_words'] == (8 if d['kind'] == 1 else 4)
        assert not d['gpu_admission']
        if d['kind']==1:
            assert d['image_extent_status'] in (0,88)
            b=d['image_copy']
            if d['image_payload_owned']:
                assert d['image_extent_status']==0 and b['copied_bytes']==d['image_extent']['bytes']>0
                assert all(b[k]==0 for k in ('requirement_status','lease_status','capture_status','release_status','lease_end_status'))
            else:
                assert b['copied_bytes']==0
        assert stage['cpu_input_capture_status'] in (0,88)
        if d['kind'] == 1:
            assert d['image_status'] in (0, 88)
            if not d['image_status']:
                p = d['image_parameters']
                assert all(p[v] > 0 for v in ('width','height','depth','pitch','layers','levels','samples'))
                assert len(p['swizzle']) == 4 and all(0 <= v <= 7 for v in p['swizzle'])
                assert p['last_array'] >= p['base_array'] and p['guest_address'] >= 0
            else:
                assert 'image_parameters' not in d
        if d['kind'] == 2:
            assert d['sampler_status'] in (0, 88)
            if not d['sampler_status']:
                p = d['sampler_parameters']
                assert len(p['clamp']) == 3 and all(0 <= v <= 7 for v in p['clamp'])
                assert p['mag_filter'] in range(4) and p['min_filter'] in range(4)
                assert p['mip_filter'] in range(3) and p['compare_func'] in range(8)
                assert p['border_color'] in range(3)
                assert p['unnormalized_in_shader'] in (0, 1) and p['anisotropic'] in (0, 1)
                assert all(math.isfinite(p[v]) for v in ('lod_bias', 'min_lod', 'max_lod', 'max_anisotropy'))
                assert p['max_anisotropy'] in (1, 2, 4, 8, 16)
            else:
                assert 'sampler_parameters' not in d
        observations.append({'word': e['word'], 'vertex': e['vertex'], **d})
    expected_flat = [b['sharp_index'] for b in resources['bindings'] if b['kind'] == 0 and b['buffer_type'] == 1]
    assert [b['sharp_index'] for b in stage['flatbufs']] == expected_flat
    for b in stage['flatbufs']:
        assert b['status'] == 0 and b['bytes'] == b['owned_words'] * 4
        assert b['owned_words'] == stage['owned_flattened_userdata_words']
        assert not b['gpu_admission']
        flatbufs.append({'word': e['word'], 'vertex': e['vertex'], **b})
    assert not stage['gpu_admission'] and not stage['source_writer_coherence']
counts = collections.Counter(d['kind'] for d in observations)
assert counts == {1: 67, 2: 64} and not flatbufs
out = {'scope': 'retail_compiler_descriptor_host_ownership',
       'eboot_sha256': json.loads((base/'first_draw_shader_capture.json').read_text())['eboot_sha256'],
       'exe_sha256': report['exe_sha256'], 'source_sha256': hashlib.sha256(raw).hexdigest(),
       'contexts': len(events), 'image_descriptors': counts[1], 'sampler_descriptors': counts[2],
       'descriptor_observations': observations, 'flatbuf_observations': flatbufs,
       'image_extent_status_counts': dict(collections.Counter(d['image_extent_status'] for d in observations if d['kind']==1)),
       'owned_image_payload_count': sum(d['image_payload_owned'] for d in observations if d['kind']==1),
       'image_status_counts': dict(collections.Counter(d['image_status'] for d in observations if d['kind'] == 1)),
       'sampler_status_counts': dict(collections.Counter(d['sampler_status'] for d in observations if d['kind'] == 2)),
       'active_stubs': report['active_stubs'], 'called_stubs': report['called_stubs'],
       'R3': False, 'engine_replacements': 0, 'payload_persisted': False,
       'limitation': 'Exact compiler-selected descriptor words copied ephemerally. 66 companion-admitted image spans are copied through owned leases; one invalid image descriptor remains explicit88. These CPU observations do not admit shader binding, source-writer coherence, ordered GPU draw/dispatch or flip. Allocation and fenced linear transfer observations are reported separately in image_allocation_lifecycle.json. Reached Flatbuf count zero; its owner completeness control is synthetic only.'}
(base/'descriptor_capture_census.json').write_text(json.dumps(out, indent=2)+'\n')
print(json.dumps({k: out[k] for k in ('contexts', 'image_descriptors', 'sampler_descriptors', 'R3')}))
