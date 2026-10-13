# SPDX-License-Identifier: GPL-2.0-only
from pathlib import Path
import json,re,hashlib
try:
 from .transfer_timing_report import aggregate_transfer_phases
except ImportError:
 from transfer_timing_report import aggregate_transfer_phases

def graphics_phases(stderr,census):
 groups={};current=None;rows=[];bad=[];complete=0;unattributed=0
 marker=re.compile(r'^CTR_GRAPHICS_CONTEXT (begin|end) submission=(\d+) word=(\d+)')
 with Path(stderr).open(errors='replace') as stream:
  for line in stream:
   m=marker.match(line)
   if m:
    kind,sub,word=m.groups();identity=(int(sub),int(word))
    if kind=='begin':
     if current is not None:bad.append({'reason':'nested_or_missing_end','prior':current,'next':identity})
     current=identity;rows=[]
    elif identity!=current:bad.append({'reason':'end_identity_mismatch','expected':current,'actual':identity});current=None;rows=[]
    else:
     parsed=aggregate_transfer_phases(rows);g=groups.setdefault(str(identity[1]),{'completed_contexts':0,'phases':{}});g['completed_contexts']+=1;complete+=1
     for key,a in parsed['aggregates'].items():
      size_key='operand_sum' if 'operand_sum' in a else 'bytes_sum'
      v=g['phases'].setdefault(key,{'count':0,'ms':0.0,'max_ms':0.0,size_key:0,'unit':a.get('operand_unit','bytes'),'statuses':{}})
      for field in ('count','ms',size_key):v[field]+=a[field]
      v['max_ms']=max(v['max_ms'],a['max_ms'])
      for status,count in a['statuses'].items():v['statuses'][status]=v['statuses'].get(status,0)+count
     current=None;rows=[]
   elif 'CTR_TRANSFER_PHASE ' in line:
    if current is not None:rows.append(line)
    elif 'operation=graphics_' in line:unattributed+=1
 metadata={};malformed=0
 with Path(census).open(errors='replace') as stream:
  for line in stream:
   if 'original_GnmSubmitFlip_pair_compile_timing' not in line and 'original_GnmSubmitFlip_source_coherence_timing' not in line:continue
   try:e=json.loads(line)
   except ValueError:malformed+=1;continue
   key=e['scope']+'/'+str(e['word']);g=metadata.setdefault(key,{'count':0,'elapsed_ms':0,'max_ms':0,'statuses':{},'shader_identities':[]})
   g['count']+=1;g['elapsed_ms']+=e['elapsed_ms'];g['max_ms']=max(g['max_ms'],e['elapsed_ms']);status=str(e['status']);g['statuses'][status]=g['statuses'].get(status,0)+1
   if 'fragment_sha256' in e:
    identity={k:e[k] for k in ('fragment_sha256','vertex_sha256','fragment_hash_status','vertex_hash_status')}
    if identity not in g['shader_identities']:g['shader_identities'].append(identity)
 return {'completed_contexts':complete,'per_original_word':groups,'infra_phases':metadata,'context_errors':bad,'pending_context':current,'unattributed_graphics_rows':unattributed,'malformed_census_rows':malformed,'limits':'Host wall time, not GPU timestamps. Completed stderr contexts only; final interrupted context excluded. Provider phases nested in draw total, do not add totals. Source coherence covers explicit validation/refresh scope only. Pair time includes host thread/load/compiler/sink; hash excluded. GetTickCount64 coarse timer.'}
if __name__=='__main__':
 r=graphics_phases('runtime/user/logs/json2_compat.stderr.log','runtime/user/logs/json2_compat.stdout.log')
 Path('runtime/user/logs/infra_current_graphics_phases.json').write_text(json.dumps(r,indent=2)+'\n')
 print(json.dumps({k:r[k] for k in ('completed_contexts','context_errors','pending_context','unattributed_graphics_rows','malformed_census_rows')}))
 print('pair compile top',sorted(((k,v['elapsed_ms'],v['count']) for k,v in r['infra_phases'].items() if 'pair_compile' in k),key=lambda x:-x[1])[:8])
 print('provider top',sorted(((word,sum(p['ms'] for p in g['phases'].values()),g['completed_contexts']) for word,g in r['per_original_word'].items()),key=lambda x:-x[1])[:8])
