#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Observe real depth-tested color/depth target sizes at a fixed host window size.

Runs original retail code offline, without input, in a new isolated save directory.
No screenshot or requested setting alone is accepted as scene-size evidence.
"""
import argparse
import ctypes
from ctypes import wintypes
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.lane_lock import lane_lock

def window_sizes(pid):
    user = ctypes.WinDLL('user32', use_last_error=True)
    # Measure physical pixels even when Windows desktop scaling is not 100%.
    user.SetThreadDpiAwarenessContext.argtypes = [ctypes.c_void_p]
    user.SetThreadDpiAwarenessContext(ctypes.c_void_p(-4))
    user.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    user.GetClientRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
    user.IsWindowVisible.argtypes = [wintypes.HWND]
    sizes = []
    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def visit(hwnd, _):
        owner = wintypes.DWORD()
        user.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        if owner.value == pid and user.IsWindowVisible(hwnd):
            rect = wintypes.RECT()
            if user.GetClientRect(hwnd, ctypes.byref(rect)):
                sizes.append([rect.right, rect.bottom])
        return True
    user.EnumWindows(visit, 0)
    return sizes

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--base-dir', type=Path, required=True)
    p.add_argument('--patch-dir', type=Path)
    p.add_argument('--heights', type=int, nargs='+', default=[720, 900, 1080, 1440, 1800, 2160])
    p.add_argument('--seconds', type=float, default=100)
    p.add_argument('--window', type=int, nargs=2, default=[1280, 720])
    p.add_argument('--upscaler', choices=['off', 'fsr1', 'dlss'], default='off')
    a = p.parse_args()
    if any(h not in (720, 900, 1080, 1440, 1800, 2160) for h in a.heights):
        p.error('Unsupported scene height')
    base = a.base_dir.resolve()
    patch = a.patch_dir.resolve() if a.patch_dir else None
    eboot = (patch or base) / 'eboot.bin'
    exe = ROOT / 'build/CTRPort-dev/ctrport.exe'
    capture = ROOT / 'build/CTRPort-infra/capture_window.exe'
    stamp = datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
    report = {'eboot_sha256': hashlib.sha256(eboot.read_bytes()).hexdigest(),
              'exe_sha256': hashlib.sha256(exe.read_bytes()).hexdigest(),
              'gpu_sha256': hashlib.sha256((ROOT / 'build/CTRPort-gpu/ctr_gpu.dll').read_bytes()).hexdigest(),
              'window_requested': a.window, 'upscaler': a.upscaler, 'runs': [], 'scope': 'real retail depth-tested attachments; no reconstruction credit'}
    report_path = ROOT / f'runtime/user/probes/scene_{stamp}.json'
    report['dlss_libraries'] = {name: hashlib.sha256((exe.parent/name).read_bytes()).hexdigest()
                              for name in ('ctr_dlss.dll','nvngx_dlss.dll') if (exe.parent/name).exists()}
    report_path.parent.mkdir(parents=True, exist_ok=True)
    with lane_lock('shared', 'claude', timeout=60):
        for height in a.heights:
            work = ROOT / f'runtime/user/instances/scene_{stamp}_{height}'
            (work / 'runtime/user/logs').mkdir(parents=True)
            # Copies of our two small runtime DLLs avoid junction privileges and
            # keep all writable state inside this probe directory.
            for name in ('ctr_gpu.dll', 'ctr_hle.dll'):
                shutil.copy2(ROOT / 'build/CTRPort-gpu' / name, work / name)
            saves = ROOT / 'runtime/user/savedata'
            if saves.exists():
                shutil.copytree(saves, work / 'runtime/user/savedata')
            env = {k: v for k, v in os.environ.items() if not k.startswith('CTR_')}
            env.update(CTR_GPU_LIBRARY='1', CTR_TLS_PATCH='1', CTR_GPU_LOG='error',
                       CTR_NO_FRAME_OBSERVATION='1', CTR_BASE_DIR=str(base), CTR_SCENE_TRACE='1',
                       CTR_SCENE_HEIGHT=str(height), CTR_GPU_WIDTH=str(a.window[0]),
                       CTR_GPU_HEIGHT=str(a.window[1]), CTR_AUDIO_MUTE='1')
            env.update(CTR_UPSCALER=a.upscaler, CTR_GPU_FSR='0', CTR_UPSCALE_CAPTURE_DIR=str(work))
            if patch:
                env['CTR_PATCH_DIR'] = str(patch)
            command = [str(exe), '--entry-after-libc-init-images', str(eboot)]
            command += [str(f) for f in sorted((base / 'sce_module').glob('*.prx'))]
            with (work / 'stdout.log').open('wb') as out, (work / 'stderr.log').open('wb') as err:
                child = subprocess.Popen(command, cwd=work, env=env, stdout=out, stderr=err)
                started = time.monotonic()
                observed_windows = []
                capture_result = None
                try:
                    while child.poll() is None and time.monotonic() - started < a.seconds:
                        time.sleep(.2)
                    if child.poll() is None:
                        observed_windows = window_sizes(child.pid)
                        shot = subprocess.run([str(capture), str(child.pid), str(work / 'scene.bmp')],
                                              capture_output=True, timeout=20)
                        (work / 'capture.log').write_bytes(shot.stdout + shot.stderr)
                        capture_result = shot.returncode
                finally:
                    code = child.poll()
                    if code is None:
                        child.terminate()
                        child.wait(15)
            log = (work / 'stderr.log').read_text(errors='replace')
            output_log = (work / 'stdout.log').read_text(errors='replace')
            dispatches = re.findall(r'CTR_UPSCALE_DISPATCH [^\r\n]+', output_log)
            targets = sorted(set(re.findall(r'CTR_SCENE_TARGET color=(\d+)x(\d+) depth=(\d+)x(\d+) render=(\d+)x(\d+)', log)))
            expected = (height * 16 // 9, height)
            matched = [list(map(int, t)) for t in targets if tuple(map(int, t[:2])) == expected and tuple(map(int, t[2:4])) == expected]
            run = {'height': height, 'expected_scene': expected, 'directory': str(work),
                   'exit': code, 'seconds': round(time.monotonic() - started, 2),
                   'targets': [list(map(int, t)) for t in targets], 'matching_color_and_depth': matched,
                   'window_observed': observed_windows,
                   'capture_exit': capture_result,
                   'upscale_dispatches': dispatches,
                   'readbacks': [str(f) for f in work.glob('*.raw')],
                   'passed': bool(matched) and code is None and a.window in observed_windows}
            if a.upscaler != 'off':
                run['passed'] = run['passed'] and bool(dispatches) and bool(run['readbacks'])
                # A black image or a successful API call alone is not scene evidence.
                import numpy as np
                run['pixel_checks'] = []
                for f in work.glob('*output*.raw'):
                    dtype = np.float16 if 'rgba16f' in f.name else np.uint8
                    rgb = np.fromfile(f, dtype=dtype).reshape(-1, 4)[:, :3].astype(np.float32)
                    normalized = rgb if dtype == np.float16 else rgb / 255.0
                    check = {'file': str(f), 'finite': bool(np.isfinite(normalized).all()),
                             'rgb_std': float(normalized.std()), 'rgb_mean': float(normalized.mean())}
                    check['nonblank'] = check['finite'] and check['rgb_std'] > .01
                    run['pixel_checks'].append(check)
                run['passed'] = run['passed'] and bool(run['pixel_checks']) and all(c['nonblank'] for c in run['pixel_checks'])
                if a.upscaler == 'dlss':
                    run['camera_validated'] = any('camera=1' in d for d in dispatches)
                    run['passed'] = run['passed'] and run['camera_validated']
            report['runs'].append(run)
            report_path.write_text(json.dumps(report, indent=2))
            print(json.dumps(run), flush=True)
            if code is not None:
                break  # Diagnose a real runtime failure before launching more cases.
    print(report_path, flush=True)
    return 0 if all(r['passed'] for r in report['runs']) else 1

if __name__ == '__main__':
    raise SystemExit(main())
