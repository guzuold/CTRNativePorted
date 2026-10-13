# SPDX-License-Identifier: GPL-2.0-only
"""Play the original game offline with ctrport and the shadPS4 GPU library.

Usage (from the project root): python -B ctrport/play_gpu.py
Runs build/CTRPort-dev/ctrport.exe until the game window is closed or the runtime
stops, logs to runtime/user/logs/play_<time>.{stdout,stderr}.log, and on exit prints the
reason the runtime stopped (missing function, unresolved access or assertion).
"""
import datetime, os, re, shutil, subprocess, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.lane_lock import lane_lock

root = Path(__file__).resolve().parents[1]
os.chdir(root)
env = os.environ.copy()
for key in ('CTR_STARTUP_WRITE_DLL_DIR', 'CTR_STARTUP_MUTEX_DLL_DIR', 'CTR_PARK_MISSING_NID', 'CTR_PARK_MISSING_SERVICE', 'CTR_TILING_TIMING'):
    env.pop(key, None)
env.update(CTR_GPU_LIBRARY='1', CTR_TLS_PATCH='1', CTR_GPU_LOG='error', CTR_NO_FRAME_OBSERVATION='1')
use_121 = '--1.21' in sys.argv or os.environ.get('CTR_VERSION') == '1.21'
# The 60 FPS guest patch exists only for the v01.21 eboot (ctrport/GUEST_PATCHES.md).
use_60fps = ('--60fps' in sys.argv or os.environ.get('CTR_60FPS_PATCH') == '1') and use_121
env.pop('CTR_60FPS_PATCH', None)
if use_60fps:
    env['CTR_60FPS_PATCH'] = '1'
elif '--60fps' in sys.argv:
    print('CTRPort: the 60 FPS patch exists only for v01.21; v01.00 runs at its original 30 FPS.', flush=True)
# Native 4K 3D scene scale (2.0f + 1920 MiB Garlic pool) in PS4 Pro mode (ctrport/GUEST_PATCHES.md).
use_native_4k = '--native-4k' in sys.argv or os.environ.get('CTR_NATIVE_4K') == '1'
# PS4 Pro (Neo) hardware profile, experimental (ctrport/direct_memory.hpp).
use_pro = '--pro' in sys.argv or '--neo' in sys.argv or use_native_4k or os.environ.get('CTR_NEO_MODE') == '1'
env.pop('CTR_NEO_MODE', None)
env.pop('CTR_NATIVE_4K', None)
if use_pro:
    env['CTR_NEO_MODE'] = '1'
if use_native_4k:
    env['CTR_NATIVE_4K'] = '1'
# Test-only: every unlockable reports unlocked while this run lasts (ctrport/GUEST_PATCHES.md).
use_unlock_all = '--unlock-all' in sys.argv or os.environ.get('CTR_UNLOCK_ALL') == '1'
env.pop('CTR_UNLOCK_ALL', None)
if use_unlock_all:
    env['CTR_UNLOCK_ALL'] = '1'
fullscreen = '--fullscreen' in sys.argv or os.environ.get('CTR_GPU_FULLSCREEN') == '1'
if fullscreen:
    env['CTR_GPU_FULLSCREEN'] = '1'
width, height = None, None
if '--4k' in sys.argv:
    width, height = 3840, 2160
elif '--1440p' in sys.argv:
    width, height = 2560, 1440
elif '--1080p' in sys.argv:
    width, height = 1920, 1080
elif '--720p' in sys.argv:
    width, height = 1280, 720
for i, arg in enumerate(sys.argv):
    if arg in ('--res', '--resolution') and i + 2 < len(sys.argv):
        try:
            width, height = int(sys.argv[i+1]), int(sys.argv[i+2])
        except ValueError:
            pass
if width and height:
    env['CTR_GPU_WIDTH'] = str(width)
    env['CTR_GPU_HEIGHT'] = str(height)
game = Path(os.environ['CTR_BASE_DIR']) if 'CTR_BASE_DIR' in os.environ else (root.parent / 'dump/CUSA14876')
patch = Path(os.environ['CTR_PATCH_DIR']) if 'CTR_PATCH_DIR' in os.environ else Path(r'dump/CUSA14876-patch')
for i, arg in enumerate(sys.argv):
    if arg in ('--base-dir', '--game-dir') and i + 1 < len(sys.argv):
        game = Path(sys.argv[i + 1])
    elif arg in ('--patch-dir', '--update-dir') and i + 1 < len(sys.argv):
        patch = Path(sys.argv[i + 1])
if use_121 and not (patch / 'eboot.bin').exists():
    sys.exit(f'CTRPort: v01.21 requested but {patch / "eboot.bin"} is missing (use --patch-dir).')
eboot = (patch / 'eboot.bin') if use_121 else (game / 'eboot.bin')
env.pop('CTR_PATCH_DIR', None)
if use_121:
    env['CTR_BASE_DIR'] = str(game)
    env['CTR_PATCH_DIR'] = str(patch)
log = root / 'runtime/user/logs' / ('play_' + datetime.datetime.now().strftime('%Y%m%d_%H%M%S'))
command = [str(root / 'build/CTRPort-dev/ctrport.exe'), '--entry-after-libc-init-images', str(eboot)] + \
          [str(p) for p in sorted((game / 'sce_module').glob('*.prx'))]
res_info = 'fullscreen' if fullscreen else f'{width or 1280}x{height or 720}'
# The game keeps saving during an unlock-all run; keep a copy of the saves made before it.
saves = root / 'runtime/user/savedata'
if use_unlock_all and saves.exists():
    backup = root / 'runtime/user/savedata_backups' / ('before_unlock_all_' + log.name.removeprefix('play_'))
    shutil.copytree(saves, backup)
    print('CTRPort: saves copied to', backup, flush=True)
print(f'CTRPort (target={"v01.21" if use_121 else "v01.00"}, pro={use_pro}, native_4k={use_native_4k}, 60fps={use_60fps}, unlock_all={use_unlock_all}, display={res_info}): close window to quit. Logs:', log, flush=True)
with lane_lock('shared', 'claude', timeout=600), log.with_suffix('.stdout.log').open('wb') as out, log.with_suffix('.stderr.log').open('wb') as err:
    code = subprocess.run(command, env=env, stdout=out, stderr=err).returncode
print('exit code', code)
reasons = []
for name in ('.stdout.log', '.stderr.log'):
    text = log.with_suffix(name).read_text(encoding='utf-8', errors='replace').splitlines()
    for line in text[-4000:]:
        if re.search(r'startup_libc_dependency_boundary|unresolved_access|unsupported:|STOP:|window closed', line) \
                and not re.search(r'obuxdTiwkF8|PlayGo|fZo48un7LK4', line):
            reasons.append(line[:300])
print('\n'.join(reasons[-5:]) if reasons else 'no stop reason found in the logs')
