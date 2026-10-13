# SPDX-License-Identifier: GPL-2.0-only
"""Exact retail libc export alias; no replacement allocator or reconstruction."""
from pathlib import Path
import sys,json,hashlib,datetime
sys.path.insert(0,'tools/harness')
from images import OriginalImage
source=OriginalImage(Path('../Staging/NitroFueled_GameFiles/CUSA14876/sce_module/libc.prx'))
assert source.sha256=='1936f5700cc590877d9bf493c6a6bb7714e84003dfd6d74a7d77b1869f71d816'
exports=[e for e in source.exports if e['nid']=='hcuQgD53UxM']
assert len(exports)==1
export=exports[0]
assert export['offset']==258176 and export['size']==238 and export['library']==export['module']=='libc'
assert any(lo<=export['offset'] and export['offset']+export['size']<=hi for lo,hi in source.code_ranges)
p=Path('reports/reconstruction/infra/json2_compat_startup.json');cold=json.loads(p.read_text());events=[]
for line in Path(cold['stdout']).read_text(errors='replace').splitlines():
 if len(line)<1800 and line.startswith('{'):
  try:e=json.loads(line)
  except ValueError:continue
  if e.get('scope')=='original_mspace_alias_binding' and e.get('nid')=='hcuQgD53UxM':events.append(e)
assert len(events)==1 and events[0]['target_offset']==export['offset']
assert not cold['terminal_boundary'] or cold['terminal_boundary']['nid']!='hcuQgD53UxM'
assert not cold['original_access_faults']
r=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=True,scope='exact original printf libc-to-libSceLibcInternal NID alias',public_name='printf',public_name_source='engine/shadPS4/src/core/aerolib/aerolib.inl',source_sha256=source.sha256,export=export,alias_binding=events,cold_report_sha256=hashlib.sha256(p.read_bytes()).hexdigest(),exe_sha256=cold['exe_sha256'],next_boundary=cold['terminal_boundary'],terminal_service_diagnostic=cold.get('service_diagnostics',[])[-1:],original_access_faults=cold['original_access_faults'],active_stubs=cold['active_stubs'],distinct_called_stubs={e['registry_index']:e['public_name'] for e in cold['called_stubs']},engine_replacements=0,R4=False,R5=False,limitation='Exact original complete238-byte export and dependencies execute unchanged. Cold progression evidence, not direct buffer-content differential control. No generic namespace alias or C++ variadic printf replacement.')
Path('reports/reconstruction/infra/printf_original_alias.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf8')
print(json.dumps(dict(passed=True,next_boundary=r['next_boundary'])))
