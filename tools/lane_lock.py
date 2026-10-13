"""Exclusive cross-process lane locks. Never silently removes stale locks."""
import argparse,contextlib,json,os,subprocess,time,uuid
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
LANES=('infra','core','gameplay','data','auto','claude')
@contextlib.contextmanager
def lane_lock(name,lane='infra',timeout=60):
 if lane not in (*LANES,'supervisore','dirty') or name not in ('shared','build-main',*[f'build-{x}' for x in (*LANES,'supervisore','dirty')]):raise ValueError('Unknown lane/lock name')
 if lane=='dirty' and name!='build-dirty':raise ValueError('Dirty may only acquire its isolated build lock')
 if lane=='supervisore' and name!='build-supervisore':raise ValueError('Supervisor may only acquire its isolated build lock')
 folder=ROOT/'coordination/locks';folder.mkdir(parents=True,exist_ok=True);path=folder/(name+'.lock');token=uuid.uuid4().hex
 owner={'lane':lane,'pid':os.getpid(),'timestamp':time.time(),'token':token};start=time.monotonic();warned=False
 while True:
  try:
   fd=os.open(path,os.O_WRONLY|os.O_CREAT|os.O_EXCL)
   try:
    with os.fdopen(fd,'w',encoding='utf8') as stream:json.dump(owner,stream)
   except BaseException:path.unlink();raise
   break
  except FileExistsError:
   try:age=time.time()-path.stat().st_mtime
   except FileNotFoundError:continue
   if age>7200 and not warned:print(f'STALE LOCK {path}: older than two hours; manual investigation required, not removed',flush=True);warned=True
   if time.monotonic()-start>=timeout:
    try:description=path.read_text()
    except FileNotFoundError:continue
    raise TimeoutError(f'Timed out waiting for {path}; owner: {description}')
   time.sleep(.05)
 try:yield owner
 finally:
  if path.exists():
   try:current=json.loads(path.read_text())
   except (OSError,ValueError):current={}
   if current.get('token')==token:path.unlink()
   else:print(f'Lock owner changed; refusing to remove {path}',flush=True)
def main():
 p=argparse.ArgumentParser();p.add_argument('--name',required=True);p.add_argument('--lane',choices=(*LANES,'dirty'),default='infra');p.add_argument('--timeout',type=float,default=60);p.add_argument('--shared',action='store_true');p.add_argument('--common-suite',action='store_true');p.add_argument('command',nargs=argparse.REMAINDER);a=p.parse_args();command=a.command
 if command and command[0]=='--':command=command[1:]
 if not command:p.error('A command is required after --')
 with contextlib.ExitStack() as stack:
  if a.shared:stack.enter_context(lane_lock('shared',a.lane,a.timeout))
  stack.enter_context(lane_lock(a.name,a.lane,a.timeout));env=os.environ.copy();env['CTR_BUILD_LOCK_HELD']='1'
  if a.common_suite:
   if a.lane!='infra' or a.name!='build-main' or not a.shared:raise ValueError('Common suite requires infra, shared and build-main')
   counter=ROOT/'coordination/common_suite_runs.json';date=time.strftime('%Y-%m-%d');record=json.loads(counter.read_text()) if counter.exists() else {}
   count=record.get('count',0) if record.get('date')==date else 0
   if count>=2:raise RuntimeError('Common suite already started twice today; run only affected module validators')
   counter.write_text(json.dumps({'date':date,'count':count+1,'last_pid':os.getpid()},indent=2)+'\n')
  return subprocess.run(command,env=env).returncode
if __name__=='__main__':raise SystemExit(main())
