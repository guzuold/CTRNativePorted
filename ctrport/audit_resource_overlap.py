# SPDX-License-Identifier: GPL-2.0-only
"""Conservative interval overlap evidence; never a GPU barrier/admission plan."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def overlap(a, b):
    """Half-open guest byte intervals, rejecting invalid uint64 extents."""
    for address, size in (a, b):
        if not isinstance(address, int) or not isinstance(size, int):
            raise ValueError('Non-integer interval')
        if address <= 0 or size <= 0 or address + size > 1 << 64:
            raise ValueError('Invalid interval')
    start, end = max(a[0], b[0]), min(a[0]+a[1], b[0]+b[1])
    return (start, end-start) if start < end else None


def control():
    assert overlap((100, 20), (110, 20)) == (110, 10)
    assert overlap((100, 20), (100, 20)) == (100, 20)
    assert overlap((100, 20), (105, 5)) == (105, 5)
    assert overlap((100, 20), (120, 4)) is None
    assert overlap((100, 20), (80, 20)) is None
    for bad in ((0, 4), (100, 0), (-1, 4), ((1 << 64)-1, 2)):
        try:
            overlap(bad, (100, 4))
        except ValueError:
            continue
        raise AssertionError(bad)


def main():
    control()
    source = ROOT/'reports/reconstruction/infra/shareplay_pending_startup.json'
    raw = source.read_bytes()
    report = json.loads(raw)
    assert hashlib.sha256((ROOT/report['exe_path']).read_bytes()).hexdigest() == report['exe_sha256']
    canonical = []
    for line in ((ROOT/'runtime/user/logs/submit_flip_graphics.jsonl').read_text()+(ROOT/'runtime/user/logs/submit_flip_census.jsonl').read_text()).splitlines():
        if line.startswith('{'):
            try:
                canonical.append(json.loads(line))
            except ValueError:
                pass
    scopes = ('original_GnmSubmitFlip_first_graphics', 'original_GnmSubmitFlip_graphics_census')
    order = lambda e: (e['word'], not e['vertex'])
    events = sorted((e for e in report['events'] if e.get('scope') in scopes), key=order)
    assert events == sorted((e for e in canonical if e.get('scope') in scopes), key=order)
    assert len(events) == 86
    ordered = next(e for e in report['events'] if isinstance(e.get('packets'), list))
    draws = [w for w, n, op in ordered['packets'] if op in (0x27, 0x2d)]
    assert [(e['word'], e['vertex']) for e in events] == [(w, v) for w in draws for v in (True, False)]
    reads, colors, incomplete = [], [], []
    for e in events:
        stage = e.get('owned_stage')
        # The first pair uses a different metadata key, preserved by its emitter.
        if stage is None:
            stage = e.get('owned_compiled_stage')
        if stage is None:
            incomplete.append({'word': e['word'], 'vertex': e['vertex'], 'reason': 'no owned compiled stage'})
            continue
        if stage['cpu_input_capture_status']:
            incomplete.append({'word': e['word'], 'vertex': e['vertex'], 'reason': 'incomplete CPU inputs'})
        for b in stage['buffers']:
            if all(b[k] == 0 for k in ('requirement_status', 'lease_status', 'capture_status', 'release_status', 'lease_end_status')):
                assert b['bytes'] == b['copied_bytes']
                reads.append({'word': e['word'], 'vertex': e['vertex'], 'sharp_index': b['sharp_index'], 'address': b['guest_address'], 'bytes': b['bytes']})
        if e['vertex']:
            for c in stage['colors']:
                if c['requirement_status'] == 0 and c['initial_copy_status'] == 0:
                    colors.append({'word': e['word'], 'slot': c['slot'], 'address': c['address'], 'bytes': c['bytes']})
    index_reads = []
    index_events = [e for e in report['events'] if e.get('scope') == 'original_GnmSubmitFlip_index_capture']
    assert [e['word'] for e in index_events] == [w for w, n, op in ordered['packets'] if op == 0x27]
    for e in index_events:
        assert all(e[k] == 0 for k in ('requirement_status', 'lease_status', 'capture_status', 'release_status', 'lease_end_status', 'retained_status'))
        assert e['bytes'] == e['copied_bytes'] == e['count'] * (4 if e['type'] else 2)
        index_reads.append({'word': e['word'], 'address': e['address'], 'bytes': e['bytes'], 'kind': 'index'})
    hazards = []
    # These are candidate surfaces, not confirmed writes. Include same-draw
    # overlap and both temporal directions; no implicit synchronization claim.
    for color in colors:
        for read in reads + index_reads:
            hit = overlap((color['address'], color['bytes']), (read['address'], read['bytes']))
            if hit:
                hazards.append({'color_candidate': color, 'buffer_read': read, 'intersection_address': hit[0], 'intersection_bytes': hit[1], 'order': 'same_draw' if color['word']==read['word'] else 'color_before_read' if color['word']<read['word'] else 'read_before_color'})
    out = {'scope': 'retail_observed_buffer_color_overlap', 'eboot_sha256': json.loads((ROOT/'reports/reconstruction/infra/first_draw_shader_capture.json').read_text())['eboot_sha256'], 'source_sha256': hashlib.sha256(raw).hexdigest(), 'exe_sha256': report['exe_sha256'], 'draws': len(draws), 'compute_routes_not_analyzed': 15, 'captured_buffer_reads': len(reads), 'captured_index_reads': len(index_reads), 'index_reads': index_reads, 'captured_color_candidates': len(colors), 'incomplete_stage_contexts': incomplete, 'overlaps': hazards, 'control': {'positive': 3, 'adjacent_disjoint': 2, 'invalid': 4}, 'active_stubs': report['active_stubs'], 'called_stubs': report['called_stubs'], 'R3': False, 'engine_replacements': 0, 'gpu_admission': False, 'source_writer_coherence': False, 'payload_persisted': False, 'limitation': 'Conservative half-open overlap of captured shader/index read buffers and candidate color allocations only. Nonzero color base is not proof of a write or enabled attachment. Missing SRT/images/writable resources and all compute resources leave this incomplete. No absence-of-hazards, barriers, guest-writer coherence, execution or flip claim.'}
    (ROOT/'reports/reconstruction/infra/resource_overlap_census.json').write_text(json.dumps(out, indent=2)+'\n')
    print(json.dumps({'buffer_reads': len(reads), 'color_candidates': len(colors), 'overlaps': len(hazards), 'incomplete_contexts': len(incomplete)}))


if __name__ == '__main__':
    main()
