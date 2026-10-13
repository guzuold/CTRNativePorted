# SPDX-License-Identifier: GPL-2.0-only
"""Correlate original producer wait frames without manufacturing a wakeup."""
import datetime, hashlib, json, re, struct, sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent.parent))
from tools.lane_lock import lane_lock
with lane_lock('shared','infra'):
    source=Path('reports/reconstruction/infra/shareplay_pending_startup.json')
    run=json.loads(source.read_text())
    assert run['exit']=='diagnostic_timeout_killed' and run['thread_checkpoint_status']==0
    assert hashlib.sha256(Path(run['exe_path']).read_bytes()).hexdigest()==run['exe_sha256']
    calls=run['events']
    producers=sorted({e['thread'] for e in calls if e.get('scope')=='original_Semaphore_call' and e['id']==52 and e['operation']==3})
    assert producers
    stderr=Path('runtime/user/logs/shareplay_park.stderr.log')
    rows=[]
    for line in stderr.read_text().splitlines():
        try: event=json.loads(line)
        except ValueError: continue
        if isinstance(event,dict): rows.append(event)
    chains={}
    for event in rows:
        if event.get('scope')!='condition_wait_original_frame' or event['thread'] not in producers: continue
        thread=event['thread']
        if event['depth']==0: chains.setdefault(thread,[]).append([])
        if chains.get(thread): chains[thread][-1].append(event)
    assert all(chains.get(t) for t in producers)
    parked=[e for e in rows if e.get('scope')=='missing_call_original_frame']
    assert parked
    pending_threads=[int(re.search(r'thread=(\d+)',line).group(1)) for line in run['pending_lines'] if 'libSceSharePlay' in line]
    assert len(pending_threads)==1 and parked[0]['thread']==pending_threads[0]
    assert pending_threads[0] not in producers
    completion=[e for e in rows if e.get('scope')=='parked_worker_completion_identity']
    assert len(completion)==1 and completion[0]['thread']==pending_threads[0]
    completion=completion[0]
    assert completion['read_valid'] and not completion['guest_modified']
    matched=[e for e in rows if e.get('scope')=='condition_wait_identity' and e['thread'] in producers and e['condition_cell']==completion['completion_object']+0x28]
    assert matched and all(not e['guest_modified'] for e in matched)
    base=None
    for index,event in enumerate(calls):
        if event.get('scope')=='original_startup_libc_dependency_boundary' and event.get('library')=='libSceSharePlay':
            before=calls[index-1];assert before['scope']=='original_caller_location'
            base=event['caller_address']-before['offset'];break
    assert base and completion['wait_target']-base==0x140e320 and completion['signal_target']-base==0x140e280
    checkpoints=[json.loads(line) for line in Path('runtime/user/logs/infra_thread_checkpoint.jsonl').read_text().splitlines()]
    for event in matched:
        checkpoint=next(e for e in checkpoints if e.get('thread')==event['thread'])
        assert checkpoint['resumed'] and 0x145c759 in checkpoint['return_candidates']
    assert len([e for e in calls if e.get('scope')=='original_GnmSubmitFlip_arguments'])==1
    # Read-only retail instruction checks. Persist offsets/targets, never bytes.
    retail=Path('../Staging/NitroFueled_GameFiles/eboot_flat.elf').read_bytes()
    retail_sha=hashlib.sha256(retail).hexdigest()
    assert retail_sha=='3e0158933a6bf690cf386f5ab846b0ef7b3d30ff8fc75c420d0dc41736fc4b9a'
    edges=[]
    for address,target in [(0x145c754,0x1438630),(0x9dd5b1,0x13d8710),(0x140e365,0x1adaef0),(0x13b1ff5,0x1adbb50)]:
        file_offset=address+0x4000
        assert retail[file_offset]==0xe8
        assert address+5+struct.unpack_from('<i',retail,file_offset+1)[0]==target
        edges.append(dict(call_offset=address,target_offset=target))
    result=dict(scope='original_producer_condition_frames',passed=True,
        timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),exe_sha256=run['exe_sha256'],
        stderr_sha256=hashlib.sha256(stderr.read_bytes()).hexdigest(),
        original_eboot_sha256='ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1',
        producer_threads=producers,producer_wait_chains=chains,parked_query_frames=parked,
        parked_completion_identity=completion,matched_producer_condition=matched,
        completion_signal_offset=0x140e280,completion_wait_offset=0x140e320,
        dependency='Producer waits on the exact completion object that original parked SharePlay worker signals only after returning from its entry callback at0x1409b1e; the query has not returned.',
        retail_flat_sha256=retail_sha,checked_retail_call_edges=edges,
        submissions_requested=1,R3=False,engine_replacements=0,
        limitation='RBP and native return candidates are not a verified unwind. Exact live completion-object/condition-cell identity and pinned retail wait/signal targets establish the producer dependency on this parked worker completion. This does not establish general scheduler equivalence. First observation per thread/cell only, bounded to256 identities and8 frames. No condition, semaphore, query result or original code was changed.')
    Path('reports/reconstruction/infra/producer_condition_frames.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(dict(passed=True,producers=producers,parked_thread=pending_threads[0],R3=False)))
