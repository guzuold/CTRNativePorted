from pathlib import Path
import json,hashlib,datetime
p=Path('reports/reconstruction/infra/json2_compat_startup.json');r=json.loads(p.read_text());text=Path(r['stdout']).read_text(errors='replace');events=[];reads=[]
for l in text.splitlines():
 if len(l)<1200 and l.startswith('{'):
  try:e=json.loads(l)
  except ValueError:continue
  if e.get('scope')=='missing_virtual_query_arguments':events.append(e)
  if e.get('scope')=='owned_file_read':reads.append(e)
assert len(events)==1 and r['terminal_boundary']['nid']=='rVjRvHJ0X6c' and not r['original_access_faults']
assert reads and len(reads)==text.count('"scope":"owned_file_read"')
common=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=True,exe_sha256=r['exe_sha256'],cold_report_sha256=hashlib.sha256(p.read_bytes()).hexdigest(),engine_replacements=0,R4=False,R5=False)
Path('reports/reconstruction/infra/virtual_query_boundary_arguments.json').write_text(json.dumps(dict(**common,arguments=events,missing_import_preserved=True,guest_modified=False,limitation='Register observation only, no query implementation, no memory sample or verified unwind.'),indent=2)+'\n',encoding='utf8')
Path('reports/reconstruction/infra/read_log_emission.json').write_text(json.dumps(dict(**common,intact_events=reads,observed_count=len(reads),raw_scope_occurrences=text.count('"scope":"owned_file_read"'),single_printf=True,limitation='Only this service emission checked; other existing concurrent logs may interleave.'),indent=2)+'\n',encoding='utf8')
print(json.dumps(dict(query=events,read_count=len(reads))))
