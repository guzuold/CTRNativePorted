#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Check GPU output against its own input (not a different animation frame).

Requires NumPy/Pillow. Raw game-derived data and previews stay with the isolated
probe under runtime/user. Pixel differences demonstrate processing, not quality.
"""
import argparse
import json
from pathlib import Path
import re
import numpy as np
from PIL import Image

def load(path):
    w,h = map(int,re.search(r'_(\d+)x(\d+)\.raw$',path.name).groups())
    channels = 2 if 'rg16f' in path.name else 1 if 'r32f' in path.name else 4
    dtype = np.float16 if '16f' in path.name else np.float32 if '32f' in path.name else np.uint8
    data = np.fromfile(path,dtype=dtype).reshape(h,w,channels).astype(np.float32)
    if dtype == np.uint8: data /= 255
    return data

def check(directory):
    source_path = next(directory.glob('*input_rgba8*.raw'))
    output_path = next(directory.glob('*output*.raw'))
    source, output = load(source_path)[:,:,:3], load(output_path)[:,:,:3]
    # FSR samples through the game's B/R swizzle and writes RGB float. NGX uses
    # the underlying memory format, so both DLSS raw images have matching order.
    fsr = output_path.name.startswith('fsr1')
    if fsr: source = source[:,:,::-1]
    size = (output.shape[1],output.shape[0])
    bilinear = np.stack([np.asarray(Image.fromarray(source[:,:,c]).resize(size,Image.Resampling.BILINEAR)) for c in range(3)],axis=2)
    difference = np.abs(output-bilinear)
    result = {'directory':str(directory),'input':[source.shape[1],source.shape[0]],
              'output':list(size), 'finite':bool(np.isfinite(output).all()),
              'input_rgb_std':float(source.std()),'output_rgb_std':float(output.std()),
              'mae_vs_same_input_bilinear':float(difference.mean()),
              'fraction_changed_over_1_255':float((difference>1/255).mean()),
              'correlation_vs_bilinear':float(np.corrcoef(output[::4,::4].ravel(),bilinear[::4,::4].ravel())[0,1])}
    for kind in ('depth_r32f','motion_rg16f'):
        files=list(directory.glob('*'+kind+'*.raw'))
        if files:
            a=load(files[0]);result[kind]={'finite':bool(np.isfinite(a).all()),'min':float(a.min()),'max':float(a.max()),'std':float(a.std()),'nonzero_fraction':float((np.abs(a)>1e-6).mean())}
    result['passed'] = result['finite'] and result['input_rgb_std']>.01 and result['output_rgb_std']>.01 and result['correlation_vs_bilinear']>.8 and result['mae_vs_same_input_bilinear']>1e-5
    for name,data in [('input',source),('output',output)]:
        if not fsr: data=data[:,:,::-1]
        Image.fromarray((np.clip(data,0,1)*255).astype(np.uint8)).resize((1280,720)).save(directory/(name+'_preview.png'))
    (directory/'pixel_check.json').write_text(json.dumps(result,indent=2))
    return result

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('directory',type=Path);a=p.parse_args()
    result=check(a.directory.resolve());print(json.dumps(result,indent=2));raise SystemExit(0 if result['passed'] else 1)
