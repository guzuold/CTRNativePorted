# SPDX-License-Identifier: GPL-2.0-only
"""Summarize the latest cold run without importing evidence from older binaries."""
import hashlib,json,pathlib
ROOT=pathlib.Path(__file__).resolve().parents[1]
def main():
    report=ROOT/'reports/reconstruction/infra'
    cold=json.loads((report/'json2_compat_startup.json').read_text())
    exe=ROOT/'build/CTRPort-infra/ctrport.exe'
    assert hashlib.sha256(exe.read_bytes()).hexdigest()==cold['exe_sha256'], 'cold evidence belongs to another binary'
    scopes={'original_GnmSubmitFlip_arguments','original_GnmSubmitFlip_completed_service_return','original_GnmSubmitDone_frame_end','original_CondDestroy_return','original_MutexDestroy_return','original_VideoOutSetGamma_return','original_VideoOutAdjustColor_return','original_PadReadState_return','original_NpGetNpId_return'}
    events=[]
    for line in (ROOT/cold['stdout']).read_text(errors='replace').splitlines():
        try: event=json.loads(line)
        except json.JSONDecodeError: continue
        if event.get('scope') in scopes: events.append(event)
    calls=cold['called_stubs']
    value={'scope':cold['scope'],'exe_sha256':cold['exe_sha256'],'exit_code':cold['exit_code'],'terminal_boundary':cold['terminal_boundary'],'events':events,'active_R8':cold['active_stubs'],'distinct_called_R8':len({x['registry_index'] for x in calls}),'called_registry_indices':sorted({x['registry_index'] for x in calls}),'SharePlay_calls':sum(x['nid']=='+MCXJlWdi+s' for x in calls),'R3':False,'engine_replacements':0,'limitations':['No verified visible-content screenshot/baseline comparison.','AdjustColor supports identity gamma1 only; other gamma values fail explicitly.'],'cold_report':'json2_compat_startup.json'}
    (report/'offline_startup_progress.json').write_text(json.dumps(value,indent=2)+'\n')
if __name__=='__main__': main()
