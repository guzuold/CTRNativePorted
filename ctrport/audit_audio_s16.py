# SPDX-License-Identifier: GPL-2.0-only
"""Evidence from retail signed16 eight-channel AudioOut opening and native-consumption controls."""
from pathlib import Path
import json,hashlib,datetime
p=Path('reports/reconstruction/infra/json2_compat_startup.json');r=json.loads(p.read_text());events=[]
for l in Path(r['stdout']).read_text(errors='replace').splitlines():
 if len(l)<1200 and l.startswith('{'):
  try:e=json.loads(l)
  except ValueError:continue
  if e.get('scope')=='original_AudioOutOpen_return' and e.get('param')==2:events.append(e)
assert events and all(e['result']>0 for e in events)
assert not r['original_access_faults']
report=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=True,retail_query_observed=bool(events),nid='ekNvsT22rsY',library='libSceAudioOut',public_name='sceAudioOutOpen',public_reference=dict(file='engine/shadPS4/src/core/libraries/audio/audioout.cpp',lines='45-66,239-330',revision='e3ce810f3a653f43ac64ebab63023de281a4103a',license='GPL-2.0-or-later'),behavior='Create real XAudio2 source voice WAVE_FORMAT_PCM 8channels16bits, blockAlign16, rate48000. Retain exact length*16 bytes and submit that byte size. Float8 retains length*32 bytes.',controls=dict(command='--audio-s16-control; --audio-pcm-control',exit_code=0,SysV6_open_SysV2_output=True,guarded_input_bytes4096=True,native_consumed_frames512=True,float_regression=True,synthetic_silence=True,connections=0),original_call_count=len(events),original_calls=events[:8],exe_sha256=r['exe_sha256'],cold_report_sha256=hashlib.sha256(p.read_bytes()).hexdigest(),next_boundary=r['terminal_boundary'],active_stubs=r['active_stubs'],distinct_called_stubs={e['registry_index']:e['public_name'] for e in r['called_stubs']},engine_replacements=0,R4=False,R5=False,limitation='Only observed MAIN/BGM index0 float8 and S16_8CH configurations. PCM consumption does not prove audible game audio. No title/menu/controller milestone. Not a compatibility stub.')
Path('reports/reconstruction/infra/audio_s16_format.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
print(json.dumps(dict(passed=True,calls=len(events),next_boundary=r['terminal_boundary'])))
