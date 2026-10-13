# SPDX-License-Identifier: GPL-2.0-only
"""Actual original compute generation -> future MRT backing transport proof."""
import datetime, hashlib, json
from pathlib import Path
from tools.lane_lock import lane_lock

with lane_lock('shared', 'infra'):
    census = Path('runtime/user/logs/submit_flip_census.jsonl')
    rows = [json.loads(line) for line in census.read_text().splitlines()]
    colors = [r for r in rows if r.get('scope') == 'original_GnmSubmitFlip_compute_to_color']
    assert len(colors) == 1
    color = colors[0]
    assert (color['writer_word'], color['consumer_word'], color['slot']) == (983, 1377, 1)
    assert color['status'] == 0 and color['compared_backing_bytes'] == 4423680
    assert color['real_transfer_fences'] and not color['pixel_payload_persisted']
    writer = next(r for r in rows if r.get('scope') == 'original_GnmSubmitFlip_compute_GPU_dispatch' and r.get('word') == 983)
    assert writer['status'] == writer['guest_writeback_status'] == 0 and writer['writeback_completed']
    joined = next(r for r in rows if r.get('scope') == 'original_GnmSubmitFlip_attachment_join' and r.get('word') == 1377)
    assert joined['color_status'] == 0 and joined['color_tokens'] == 2 and joined['layout_token']
    report = dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(), passed=True,
                  event=color, original_writer=writer, attachments=joined,
                  exe_sha256=hashlib.sha256(Path('build/CTRPort-infra/ctrport.exe').read_bytes()).hexdigest(),
                  census_sha256=hashlib.sha256(census.read_bytes()).hexdigest(),
                  scope='Real original983 guest generation refreshed into retained MRT1. Full color backing compared against separate actual GPU detile/reference image; same public detiler is a transport proof, not independent proof of tiling mathematics.',
                  R3=False, engine_replacements=0)
    Path('reports/reconstruction/infra/compute_color_refresh_retail.json').write_text(json.dumps(report, indent=2)+'\n')
    print('original983 -> MRT1 full backing transport audit passed')
