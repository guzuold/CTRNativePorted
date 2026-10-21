# SPDX-License-Identifier: GPL-2.0-or-later
"""Synthetic lobby task through the actual Server::Impl::hub_task, no WAN/game."""
import argparse,json,subprocess,tempfile,time
from pathlib import Path
import test_public_hub as h
p=argparse.ArgumentParser();p.add_argument('--member',default='build/hub-test/hub_election_test.exe');p.add_argument('--hub');p.add_argument('--assigned',action='store_true');p.add_argument('--late',action='store_true');args=p.parse_args()
# Only synthetic services on dedicated ports; never probe the user's default ports.
def at(host):
    h.e2e.HOST,h.e2e.HTTP=host,29780
h.at=at
original_connect=h.e2e.socket.create_connection
def connect(address,*args,**kwargs):
    host,port=address
    return original_connect((host,29774 if port==3074 else port),*args,**kwargs)
h.e2e.socket.create_connection=connect
def ready(root,proc):
    log=root/'runtime/user/logs/member.jsonl'
    for _ in range(200):
        if proc.poll() is not None:raise RuntimeError('fixture exited: ports unavailable')
        if log.exists():
            events=[json.loads(l) for l in log.read_text().splitlines()]
            if any(e['event']=='listen_failed' for e in events):raise RuntimeError('fixture bind failed')
            if any(e['event']=='listening' for e in events):return
        time.sleep(.02)
    raise RuntimeError('fixture did not start')
processes=[]
with tempfile.TemporaryDirectory(prefix='ctr_hub_wait_') as tmp:
    root=Path(tmp)
    try:
        processes.append(subprocess.Popen([str(Path(args.hub or args.member).resolve()),'--hub',str(root/'hub'),'unused','absent'],
            stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL))
        ready(root/'hub',processes[-1]);
        at(h.HUB)
        code=json.loads(h.e2e.http('GET','/ctr/v1/identity?nonce=00112233445566778899aabbccddeeff')[3])['friend_code']
        for mode in (['assigned'] if args.assigned else ['late'] if args.late else ['appears','absent']):
            member=root/mode
            proc=subprocess.Popen([str(Path(args.member).resolve()),'--member',str(member),code,mode],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
            processes.append(proc)
            for _ in range(100):
                if proc.poll() is not None:raise RuntimeError('fixture exited')
                logpath=member/'runtime/user/logs/member.jsonl'
                if logpath.exists() and '"listening"' in logpath.read_text():break
                time.sleep(.02)
            ready(member,proc)
            player=h.login(h.MEMBER,'SyntheticPlayer')
            start=time.monotonic();h.advertise(player,'192.0.2.1');elapsed=time.monotonic()-start
            if mode in ['appears','assigned']:h.search(player)
            refused=None
            if mode in ['late','absent']:
                # This PC cannot be reached: a local public queue could never pair it with anyone.
                # The search is refused (error 1) instead, also when the hub appears halfway
                # through, since the advertise is on this side; the next attempt starts over.
                if mode=='late':time.sleep(.3)
                kind,payload=h.search(player)
                values=h.Reader(payload).values()
                refused=len(values)>1 and values[1][1]==1 and not any(t==h.STRING and 'mm_id' in str(v) for t,v in values)
                h.check(refused,'%s: unreachable PC refuses a public search it could only queue locally'%mode)
            log=[json.loads(line) for line in (member/'runtime/user/logs/member.jsonl').read_text().splitlines()]
            print(json.dumps({'mode':mode,'elapsed':round(elapsed,3)}),flush=True)
            if mode=='late':
                h.check(any(e['event']=='hub_none' for e in log),'late hub: first public task fell back locally')
                h.check(not any(e['event']=='hub_task_forwarded' for e in log),'late hub: no task of the local sequence forwarded to the hub')
            elif mode in ['appears','assigned']:
                h.check(any(e['event']=='hub_task_forwarded' for e in log),'waiting public task forwarded when election publishes hub')
                h.check(any(e['event']=='hub_task_forwarded' and e.get('task')==18 for e in log),'public search task 145/18 forwarded to hub')
                hub_log=[json.loads(l) for l in (root/'hub/runtime/user/logs/member.jsonl').read_text().splitlines()]
                h.check(any(e['event']=='hub_player_joined' for e in hub_log),'hub logs remote player arrival')
                if mode=='appears':
                    h.check(elapsed>=.13,'task actually waited for delayed election')
                    h.check(any(e['event']=='hub_elected' and e.get('source')=='dht' and e.get('address')=='127.0.0.2:29780' and e.get('candidates')==1 for e in log),'hub_elected fields written to service log')
            else:
                none=[e for e in log if e['event']=='hub_none' and e.get('reason')=='wait_expired']
                h.check(len(none)==1 and all(e.get('reachable') is False and e.get('wait_ms')==1000 for e in none),'local fallback logs hub_none with unreachable/deadline fields')
                h.check(any(e['event']=='hub_none' and e.get('task')==18 and e.get('reason')=='search_refused' for e in log),'refused search logged as hub_none search_refused')
                # Task 2 waits the budget; task 3 continues the local sequence without waiting again.
                h.check(.9<=elapsed<1.9,'only the advertise waits the one-second budget')
            if mode=='late':
                h.check(any(e['event']=='hub_none' and e.get('task')==18 and e.get('reason')=='search_refused' for e in log),'late hub: refused search logged')
            player['lobby'].s.close();proc.terminate();proc.wait();processes.remove(proc)
    finally:
        for proc in processes:
            proc.terminate();proc.wait()
raise SystemExit(1 if h.failures else 0)
