# SPDX-License-Identifier: GPL-2.0-only
"""Interactive offline original retail execution without diagnostic frame holds."""
from pathlib import Path
import os, subprocess, datetime
from tools.lane_lock import lane_lock
root=Path(__file__).resolve().parents[1]
os.chdir(root)
env=os.environ.copy()
for key in ('CTR_STARTUP_WRITE_DLL_DIR','CTR_STARTUP_MUTEX_DLL_DIR','CTR_PARK_MISSING_NID','CTR_PARK_MISSING_SERVICE','CTR_TILING_TIMING'):
 env.pop(key,None)
env.update(CTR_NO_FRAME_OBSERVATION='1',CTR_RETURN_COMPLETED_GNM='1',CTR_PRIVATE_DESCRIPTOR_EVIDENCE='1',CTR_GRAPHICS_CENSUS_STAGES='128',CTR_VIDEO_TILING_SHADER_DIR='build/CTRPort-data/tiling-glslang-ninja')
game=root.parent/'Staging/NitroFueled_GameFiles/CUSA14876'
exe=root/'build/CTRPort-infra/ctrport.exe'
log=root/'runtime/user/logs'/('interactive_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S'))
print('Offline runtime; no artificial frame pause. Close the game window to finish. Logs:',log,flush=True)
with lane_lock('shared','infra'), log.with_suffix('.stdout.log').open('wb') as out, log.with_suffix('.stderr.log').open('wb') as err:
 raise SystemExit(subprocess.run([str(exe),'--entry-after-libc-init-images',str(game/'eboot.bin')]+[str(p) for p in sorted((game/'sce_module').glob('*.prx'))],env=env,stdout=out,stderr=err).returncode)
