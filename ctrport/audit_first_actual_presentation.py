# SPDX-License-Identifier: GPL-2.0-only
"""Audit real first presentation without promoting a black diagnostic to R3."""
import json,hashlib,struct,datetime
from pathlib import Path
from tools.lane_lock import lane_lock
def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
with lane_lock('shared','infra'):
 source=Path('reports/reconstruction/infra/shareplay_pending_startup.json'); r=json.loads(source.read_text()); e=r['events']
 assert (r['exit']==4 or (r['exit']=='diagnostic_timeout_killed' and r['timeout_seconds']==120)) and r['engine_replacements']==0
 assert digest(r['exe_path'])==r['exe_sha256']
 presents=[x for x in e if x.get('scope')=='original_GnmSubmitFlip_actual_present'];assert len(presents)==1
 p=presents[0];assert p['status']==0 and p['real_present'] and p['checked_GPU_publication'] and p['guest_address']==73110945792 and p['guest_bytes']==8355840 and p['slot']==0 and p['mode']==1 and p['argument']==0 and p['completion_count']==1 and p['completion_token']
 captures=[]
 for path in [Path('runtime/user/captures/infra_first_present.bmp'),*sorted(Path('runtime/user/captures').glob('baseline_*.bmp'))]:
  b=path.read_bytes();offset=struct.unpack_from('<I',b,10)[0];w,h=struct.unpack_from('<ii',b,18);assert b[:2]==b'BM' and struct.unpack_from('<H',b,28)[0]==32
  pixels=b[offset:]; nonblack=sum(any(pixels[i:i+3]) for i in range(0,len(pixels),4))
  captures.append(dict(path=str(path),sha256=digest(path),width=w,height=abs(h),nonblack_pixels=nonblack,GPU_capture='DXGI desktop duplication cropped to process client; visual inspection required for occlusion'))
 assert digest('runtime/shadPS4.exe')=='915d0317bcd45f01ca00a59cc3e0be37c83b61c294bb059253785ec056f45864'
 baseline=dict(exe_sha256=digest('runtime/shadPS4.exe'),stdout_sha256=digest('runtime/user/logs/infra_first_frame_baseline.stdout.log'),stderr_sha256=digest('runtime/user/logs/infra_first_frame_baseline.stderr.log'),observation='Two bounded baseline client captures are also uniformly black; no synchronized first-submission comparison or title content established.')
 assert captures and captures[0]['width']==1280 and captures[0]['height']==720
 result=dict(scope='retail_first_actual_presentation',timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=True,source_sha256=digest(source),exe_sha256=r['exe_sha256'],present=p,captures=captures,baseline=baseline,active_compatibility_stubs=r['active_stubs'],called_compatibility_stubs=r['called_stubs'],R3=False,engine_replacements=0,observation='Native client capture is uniformly black. Genuine presented GPU generation is verified, but visible game content and baseline same-moment equivalence are not established. Do not declare R3.',limitation='SharePlay parked diagnostic; Strict opt-in first submission now completes cursor/service return; sceGnmSubmitDone and original GetFlipStatus now complete with real clock. Attached black screenshots are historical captures, not a new synchronized capture of this replay. Current slot0 lock retained, no fake IRQ. Screenshot pixels remain under ignored runtime/user, not published game content.')
 Path('reports/reconstruction/infra/first_actual_presentation.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(passed=True,present=p,captures=captures,baseline=baseline,R3=False)))
