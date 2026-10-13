# SPDX-License-Identifier: GPL-2.0-only
"""Typed public null predicate is independent from image type validity."""
import datetime,hashlib,json
from pathlib import Path
startup=json.loads(Path('reports/reconstruction/infra/shareplay_pending_startup.json').read_text())
rows=[]
for e in startup['events']:
 owned=e.get('owned_stage',e.get('owned_compiled_stage',{}))
 for d in owned.get('descriptors',[]):
  if d['kind']==1:
   assert d['status']==0 and d['copied_words']==8 and d['image_class_status']==0
   rows.append({'word':e['word'],'vertex':e['vertex'],'sharp_index':d['sharp_index'],'null_reasons':d['null_image_reasons'],'type_valid':d['image_type_valid'],'image_status':d['image_status']})
assert len(rows)==67 and all(r['null_reasons']==0 for r in rows)
invalid=[r for r in rows if not r['type_valid']]
assert len(invalid)==1 and (invalid[0]['word'],invalid[0]['vertex'],invalid[0]['sharp_index'],invalid[0]['image_status'])==(4911,False,112,88)
exe=Path(startup['exe_path']);assert hashlib.sha256(exe.read_bytes()).hexdigest()==startup['exe_sha256']
dll=Path('build/CTRPort-data/ctr_graphics_companion_v19.dll')
report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'scope':'actual retail eight-word descriptor classification; no null fallback or GPU admission','exe_sha256':startup['exe_sha256'],'classifier_dll':str(dll),'classifier_sha256':hashlib.sha256(dll.read_bytes()).hexdigest(),'rows':rows,'nonnull':67,'valid_type':66,'invalid_type_nonnull':invalid,'R3':False,'engine_replacements':0,'GPU_execution':False,'active_stubs':startup['active_stubs'],'called_stubs':startup['called_stubs']}
Path('reports/reconstruction/infra/image_classification.json').write_text(json.dumps(report,indent=2)+'\n')
print('67 nonnull images;66 valid types;4911 PS sharp112 invalid type remains88')
