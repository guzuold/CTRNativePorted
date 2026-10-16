# SPDX-License-Identifier: GPL-2.0-only
"""Play the original game offline with ctrport and the shadPS4 GPU library.

Usage (from the project root): python -B ctrport/play_gpu.py
Runs build/CTRPort-claude/ctrport.exe until the game window is closed or the runtime
stops, logs to runtime/user/logs/play_<time>.{stdout,stderr}.log, and on exit prints the
reason the runtime stopped (missing function, unresolved access or assertion).
"""
import datetime, json, os, re, shutil, subprocess, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.lane_lock import lane_lock

root = Path(__file__).resolve().parents[1]
os.chdir(root)
workdir = root
instance_name = None
for i, arg in enumerate(sys.argv):
    if arg == '--instance':
        if i + 1 >= len(sys.argv):
            sys.exit('CTRPort: --instance requires a name.')
        instance_name = sys.argv[i + 1]
if instance_name:
    from tools.online.instance import prepare, lock as instance_lock
    workdir = prepare(root, instance_name)
user_root = workdir / 'runtime/user'
if '--seed-runtime-saves' in sys.argv:
    if not instance_name:
        sys.exit('CTRPort: --seed-runtime-saves requires --instance.')
    source_saves = root / 'runtime/user/savedata'
    target_saves = user_root / 'savedata'
    if target_saves.exists():
        sys.exit('CTRPort: instance saves already exist; refusing to overwrite them.')
    shutil.copytree(source_saves, target_saves)
    print('CTRPort: instance seeded from this runtime saves:', target_saves, flush=True)
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
scene_height = os.environ.get('CTR_SCENE_HEIGHT')
upscaler = os.environ.get('CTR_UPSCALER', 'off')
if '--upscaler' in sys.argv:
    index = sys.argv.index('--upscaler')
    if index + 1 >= len(sys.argv):
        sys.exit('--upscaler requires off, fsr1 or dlss')
    upscaler = sys.argv[index + 1]
if upscaler not in ('off', 'fsr1', 'dlss'):
    sys.exit('Invalid upscaler: ' + upscaler)
env['CTR_UPSCALER'] = upscaler
env['CTR_GPU_FSR'] = '0'
if '--internal-res' in sys.argv:
    index = sys.argv.index('--internal-res')
    if index + 1 >= len(sys.argv):
        sys.exit('--internal-res requires 720, 900, 1080, 1440, 1800 or 2160')
    scene_height = sys.argv[index + 1]
if scene_height is not None:
    if scene_height not in ('720', '900', '1080', '1440', '1800', '2160'):
        sys.exit('Invalid internal resolution: ' + scene_height)
    env['CTR_SCENE_HEIGHT'] = scene_height
use_pro = bool(scene_height) or '--pro' in sys.argv or '--neo' in sys.argv or use_native_4k or os.environ.get('CTR_NEO_MODE') == '1'
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
# Online (R8): network and NP services in ctr_hle; every *.demonware.net name resolves to the
# project server (tools/online/ctr_online_server.py), Sony and Activision hosts are never
# contacted (ctrport/hle/online_config.h).
use_online = '--online' in sys.argv or os.environ.get('CTR_ONLINE') == '1'
env.pop('CTR_ONLINE', None)
if use_online:
    env['CTR_ONLINE'] = '1'
    for i, arg in enumerate(sys.argv):
        if arg == '--online-server' and i + 1 < len(sys.argv):
            env['CTR_ONLINE_SERVER'] = sys.argv[i + 1]
        elif arg == '--online-id' and i + 1 < len(sys.argv):
            env['CTR_ONLINE_ID'] = sys.argv[i + 1]
        elif arg == '--online-port-offset' and i + 1 < len(sys.argv):
            offset = int(sys.argv[i + 1])
            if not 0 <= offset <= 65535 - 3074:
                sys.exit('CTRPort: online port offset outside 0..62461.')
            env['CTR_ONLINE_PORT_OFFSET'] = str(offset)
    sys.path.insert(0, str(root / 'tools/online'))
    import room
    password = env.pop('CTR_ONLINE_PASSWORD', '')
    if '--online-password' in sys.argv:
        index = sys.argv.index('--online-password')
        if index + 1 >= len(sys.argv):
            sys.exit('CTRPort: --online-password requires a value.')
        password = sys.argv[index + 1]
    if password:
        try:
            env['CTR_ONLINE_ROOM_TOKEN'] = room.token(password)
        except ValueError as error:
            sys.exit('CTRPort: ' + str(error))
    if not room.valid_token(env.get('CTR_ONLINE_ROOM_TOKEN', '')):
        sys.exit('CTRPort: online play requires a room password (--online-password or CTR_ONLINE_PASSWORD).')
    # The project server's public key replaces the auth3 traffic key in the eboot
    # (ctrport/online_traffic_key.hpp); fetch it from the server this run will use.
    import urllib.request
    server = env.get('CTR_ONLINE_SERVER', '127.0.0.1')
    port = env.get('CTR_ONLINE_HTTP_PORT', '8780')
    key_path = user_root / 'online' / ('traffic_key_%s_%s.der' % (server, port))
    try:
        with urllib.request.urlopen('http://%s:%s/ctr/v1/traffic-key' % (server, port), timeout=5) as reply:
            key = reply.read()
    except OSError as error:
        sys.exit('CTRPort: project server %s:%s unreachable (%s); start tools/online/ctr_online_server.py' % (server, port, error))
    key_path.parent.mkdir(parents=True, exist_ok=True)
    key_path.write_bytes(key)
    env['CTR_ONLINE_TRAFFIC_KEY'] = str(key_path)
if '--mute' in sys.argv:
    env['CTR_AUDIO_MUTE'] = '1'
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
game = Path(os.environ['CTR_BASE_DIR']) if 'CTR_BASE_DIR' in os.environ else (root / 'dump/CUSA14876')
patch = Path(os.environ['CTR_PATCH_DIR']) if 'CTR_PATCH_DIR' in os.environ else (root / 'dump/CUSA14876-patch')
for i, arg in enumerate(sys.argv):
    if arg in ('--base-dir', '--game-dir') and i + 1 < len(sys.argv):
        game = Path(sys.argv[i + 1])
    elif arg in ('--patch-dir', '--update-dir') and i + 1 < len(sys.argv):
        patch = Path(sys.argv[i + 1])
if use_121 and not (patch / 'eboot.bin').exists():
    sys.exit(f'CTRPort: v01.21 requested but {patch / "eboot.bin"} is missing (use --patch-dir).')
eboot = (patch / 'eboot.bin') if use_121 else (game / 'eboot.bin')
game, patch, eboot = game.resolve(), patch.resolve(), eboot.resolve()
env.pop('CTR_PATCH_DIR', None)
if use_121:
    env['CTR_BASE_DIR'] = str(game)
    env['CTR_PATCH_DIR'] = str(patch)
log = user_root / 'logs' / ('play_' + datetime.datetime.now().strftime('%Y%m%d_%H%M%S'))
command = [str(root / 'build/CTRPort-claude/ctrport.exe'), '--entry-after-libc-init-images', str(eboot)] + \
          [str(p) for p in sorted((game / 'sce_module').glob('*.prx'))]
res_info = 'fullscreen' if fullscreen else f'{width or 1280}x{height or 720}'
# The game keeps saving during an unlock-all run; keep a copy of the saves made before it.
saves = user_root / 'savedata'
if use_unlock_all and saves.exists():
    backup = user_root / 'savedata_backups' / ('before_unlock_all_' + log.name.removeprefix('play_'))
    shutil.copytree(saves, backup)
    print('CTRPort: saves copied to', backup, flush=True)
print(f'CTRPort (target={"v01.21" if use_121 else "v01.00"}, pro={use_pro}, native_4k={use_native_4k}, scene_height={scene_height or "legacy"}, 60fps={use_60fps}, unlock_all={use_unlock_all}, online={use_online}, display={res_info}): close window to quit. Logs:', log, flush=True)
with (instance_lock(workdir) if instance_name else lane_lock('shared', 'claude', timeout=600)), log.with_suffix('.stdout.log').open('wb') as out, log.with_suffix('.stderr.log').open('wb') as err:
    child = subprocess.Popen(command, cwd=workdir, env=env, stdout=out, stderr=err)
    log.with_suffix('.run.json').write_text(json.dumps({'pid': child.pid, 'cwd': str(workdir),
        'instance': instance_name, 'online_id': env.get('CTR_ONLINE_ID'),
        'room_selector': env.get('CTR_ONLINE_ROOM_TOKEN'),
        'online_port_offset': env.get('CTR_ONLINE_PORT_OFFSET', '0')}, indent=2))
    print('CTRPort PID:', child.pid, flush=True)
    try:
        code = child.wait()
    finally:
        if child.poll() is None:
            child.terminate()
            child.wait(timeout=10)
print('exit code', code)
reasons = []
for name in ('.stdout.log', '.stderr.log'):
    text = log.with_suffix(name).read_text(encoding='utf-8', errors='replace').splitlines()
    for line in text[-4000:]:
        if re.search(r'startup_libc_dependency_boundary|unresolved_access|unsupported:|STOP:|window closed', line) \
                and not re.search(r'obuxdTiwkF8|PlayGo|fZo48un7LK4', line):
            reasons.append(line[:300])
print('\n'.join(reasons[-5:]) if reasons else 'no stop reason found in the logs')
