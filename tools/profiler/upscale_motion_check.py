#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Check consecutive retail GPU captures, never a synthetic movement formula.

This is diagnostic evidence, not a temporal-quality certification: lighting,
animation, disocclusion and the game's motion blur also change the pixels.
Requires NumPy and OpenCV. Captures/output belong in ignored runtime/user.
"""
import argparse
import json
import re
from pathlib import Path
import cv2
import numpy as np


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--frame', type=int, default=1)
    args = parser.parse_args()
    p, frame = args.directory, args.frame
    if frame < 1:
        parser.error('frame needs a preceding capture')
    file = next(p.glob(f'fsr3_f{frame:03}_color_*.raw'))
    w, h = map(int, re.search(r'_(\d+)x(\d+)_', file.name).groups())

    def read(n, kind, dtype, channels):
        return np.fromfile(next(p.glob(f'fsr3_f{n:03}_{kind}_*.raw')), dtype).reshape(h, w, channels)

    current = read(frame, 'color', np.uint8, 4)[:, :, :3].astype(np.float32)
    previous = read(frame-1, 'color', np.uint8, 4)[:, :, :3].astype(np.float32)
    motion = read(frame, 'motion', np.float16, 2).astype(np.float32)
    raw = read(frame, 'velocity', np.uint8, 4)
    jitter = {}
    with (p/'stdout.log').open(errors='replace') as stream:
        for line in stream:
            match = re.match(r'CTR DLSS dump (\d+) .*jitter ([\d.\-]+) ([\d.\-]+),', line)
            if match:
                jitter[int(match[1])] = np.array([float(match[2]), float(match[3])])
                if frame in jitter and frame-1 in jitter:
                    break
    delta = jitter[frame-1] - jitter[frame]
    y, x = np.mgrid[:h, :w].astype(np.float32)
    grid = np.stack([x, y], axis=-1)
    maps = {'zero_unjittered': grid+delta, 'supplied_motion': grid+motion+delta}
    masks = {'target3_flag_set': raw[:, :, 2] > 127, 'target3_flag_clear': raw[:, :, 2] <= 127}
    result = {'frame': frame, 'size': [w, h], 'finite_motion': bool(np.isfinite(motion).all()),
              'jitter_previous_minus_current': delta.tolist(), 'regions': {},
              'limitation': 'Target3 flag is not a verified moving-object mask; photometric residuals are not ground-truth motion.'}
    for name, mask in masks.items():
        mask = cv2.erode(mask.astype(np.uint8), np.ones((5, 5), np.uint8)) > 0
        region = {'pixels': int(mask.sum()), 'zero_motion_fraction': float(np.mean(np.all(motion[mask] == 0, axis=-1)))}
        for label, coordinates in maps.items():
            warped = cv2.remap(previous, coordinates.astype(np.float32), None, cv2.INTER_LINEAR)
            valid = mask & (coordinates[:, :, 0] >= 1) & (coordinates[:, :, 0] < w-2) & (coordinates[:, :, 1] >= 1) & (coordinates[:, :, 1] < h-2)
            region[label+'_mae_0_255'] = float(np.mean(abs(current[valid]-warped[valid])))
        result['regions'][name] = region
    (p/f'motion_check_{frame}.json').write_text(json.dumps(result, indent=2))
    print(json.dumps(result))


if __name__ == '__main__':
    main()
