# SPDX-License-Identifier: GPL-2.0-only
"""Audit metadata from a fresh pending-call replay; never persist shader payloads."""
import hashlib,json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
source=root/'reports/reconstruction/infra/shareplay_pending_startup.json'
r=json.loads(source.read_text())
exe=root/r.get('exe_path','build/CTRPort-infra/ctrport.exe')
assert r['exe_sha256']==hashlib.sha256(exe.read_bytes()).hexdigest(), 'Replay belongs to another executable'
assert r['exit']==4 and r['pending_lines'], 'Expected explicit missing call and pending SharePlay'
ordered=next(e for e in r['events'] if e.get('scope')=='original_GnmSubmitFlip_ordered')
assert ordered['decode_status']==0
first=min(word for word,size,opcode in ordered['packets'] if opcode in (0x27,0x2d))
e=[x for x in r['events'] if x.get('scope')=='original_GnmSubmitFlip_first_graphics']
canonical=root/'runtime/user/logs/submit_flip_graphics.jsonl'
assert canonical.exists(), 'Dedicated graphics capture missing'
assert e==[json.loads(line) for line in canonical.read_text().splitlines()], 'Canonical graphics evidence differs'
assert ordered==json.loads((root/'runtime/user/logs/submit_flip_ordered.json').read_text()), 'Ordered evidence differs'
assert len(e)==2 and {x['vertex'] for x in e}=={True,False}
vertex=next(x for x in e if x['vertex'])
pixel=next(x for x in e if not x['vertex'])
assert vertex['compiler_status']==0 and vertex['compiler_spirv_words']==1564
assert vertex['compiler_resources']['has_fetch_shader']==1
vs_owned=vertex['owned_compiled_stage']
assert vs_owned['owned_spirv_words']==1564 and vs_owned['copied_userdata_words']==8
assert vs_owned['cpu_input_capture_status']==0
assert len(vs_owned['buffers'])==1
vs_buffer=next(b for b in vs_owned['buffers'] if b['sharp_index']==4)
assert vs_buffer['bytes']==vs_buffer['copied_bytes']==vs_buffer['owned_bytes']==64
assert all(vs_buffer[k]==0 for k in ['requirement_status','lease_status','capture_status','release_status','lease_end_status'])
color=vs_owned['colors'][0]
assert len(vs_owned['colors'])==1 and color['slot']==0
assert (color['pitch'],color['height'],color['layers'],color['samples'])==(128,64,1,1)
assert color['bytes']==color['copied_bytes']==32768 and not color['GPU_output_admission']
assert all(color[k]==0 for k in ['requirement_status','initial_copy_status','lease_status','release_status','lease_end_status'])
assert vs_owned['clip_planes']==[{'sharp_index':0,'status':0,'enabled_mask':0,'bytes':0}]
assert pixel['compiler_status']==0 and pixel['compiler_resources']['has_fetch_shader']==0
owned=pixel['owned_compiled_stage']
assert owned['owned_spirv_words']==pixel['compiler_spirv_words']
assert owned['copied_userdata_words']==pixel['user_register_count']==4
assert owned['cpu_input_capture_status']==0
assert not owned['source_writer_coherence'] and not owned['gpu_admission']
assert len(owned['buffers'])==1
buffer=owned['buffers'][0]
assert buffer['sharp_index']==0 and buffer['bytes']==buffer['owned_bytes']==buffer['copied_bytes']==960
assert buffer['stride']==16 and buffer['records']==60
assert all(buffer[k]==0 for k in ['requirement_status','lease_status','capture_status','release_status','lease_end_status'])
for x in e:
 assert x['word']==first and x['state_status']==0 and x['required_registers_observed']
 assert x['binary_capture_status']==0 and x['validated_code_bytes']>0 and x['validated_code_bytes']%4==0
 assert x['footer_offset']+28<=x['owned_extent_remaining']
 assert x['validated_code_bytes']<=x['footer_offset']
 assert not x['source_writer_coherence'] and not x['gpu_execution'] and not x['descriptor_binding']
 assert x['register_apply_failures']==0
 lifetime=x['owned_input_lifetime']
 assert lifetime['range_begins']==lifetime['range_ends'] and lifetime['range_begins']>=3
 assert lifetime['active_leases']==lifetime['active_snapshots']==0
 assert 4<=x['allocated_vgprs']<=256 and x['allocated_vgprs']%4==0 and 0<=x['input_vgprs']<=3
 assert x['pipeline_metadata']['device_profile_observed'] and x['profile_status']==0
 assert x['profile_max_ubo']>0 and x['profile_subgroup']>0 and x['profile_spirv']>=0x10000
 assert x['compiler_loader_error']==0
 assert x['compiler_status'] in (0,88,112)
 if x['compiler_status']==0:
  assert x['compiler_spirv_words']>=5
  resources=x['compiler_resources'];assert resources['version']==1
  assert resources['binding_count']==len(resources['bindings'])<=128
  for kind,key in enumerate(['buffers','images','samplers','fmasks']): assert resources[key]==sum(b['kind']==kind for b in resources['bindings'])
  assert all(0<=b['kind']<=3 and not b['flags']&~127 for b in resources['bindings'])
 else: assert x['compiler_diagnostic'] and x['compiler_spirv_words']==0
 assert x['pipeline_metadata']==e[0]['pipeline_metadata']
 targets=x['render_target_metadata']
 assert targets==e[0]['render_target_metadata'] and not targets['surface_admission']
 assert len(targets['color_buffers'])==8
 assert [b['slot'] for b in targets['color_buffers']]==list(range(8))
 assert all(0<=b[k]<=0xffffffff for b in targets['color_buffers'] for k in ['base_raw','pitch_raw','slice_raw','view_raw','info_raw','attrib_raw'])
 def target_bank(metadata):
  bank={0xa08e:metadata['target_mask_raw'],0xa08f:metadata['shader_mask_raw']}
  for b in metadata['color_buffers']:
   for offset,key in enumerate(['base_raw','pitch_raw','slice_raw','view_raw','info_raw','attrib_raw']):bank[0xa318+15*b['slot']+offset]=b[key]
  return bank
 reconstructed=target_bank(x['render_target_initial'])
 for word,index,value in x['render_target_writes']:
  assert word<first and index in reconstructed and 0<=value<=0xffffffff
  assert any(w==word and op==0x69 for w,n,op in ordered['packets'])
  reconstructed[index]=value
 assert reconstructed==target_bank(targets), 'Target metadata differs from retained state plus ordered context writes'
 assert x['retained_init_state_included'] and x['retained_init_status']==0 and x['completed_init_submissions']>0
 assert x['draw_layout_valid'] and x['draw_opcode'] in (0x27,0x2d)
missing=[x for x in r['events'] if x.get('scope')=='original_startup_libc_dependency_boundary']
assert any(x['nid']=='xbxNatawohc' and x['library']=='libSceGnmDriver' for x in missing)
report={'scope':'retail_first_draw_shader_capture','eboot_sha256':'ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1',
'exe_path':str(exe.relative_to(root)),'exe_sha256':r['exe_sha256'],'canonical_metadata_sha256':hashlib.sha256(canonical.read_bytes()).hexdigest(),'events':e,'first_draw_word':first,'active_compatibility_stubs':r['active_stubs'],
'called_stubs':r['called_stubs'],'R3':False,'engine_replacements':0,'payload_persisted':False,
'limitation':'Diagnostic pending SharePlay caller never returns. Versioned companion uses explicit owned fetch+descriptor leases; VS1564/PS10870 words, copied userdata and directUD buffers64/960B are ephemeral host copies. VS inline ClipPlanes has no enabled planes, zero-byte unread requirement; both CPU input capture statuses0. Initial color surface32768B copied under exact owned lease; no GPU output admission. GPU null-descriptor policy is separate. Source-writer coherence, GPU descriptors, pipeline and execution remain unadmitted. Historical unsafe v3 evidence preserved separately.',
'public_source':{'revision':'e3ce810f3a653f43ac64ebab63023de281a4103a','files':['engine/shadPS4/src/video_core/amdgpu/regs_shader.h','engine/shadPS4/src/video_core/amdgpu/pm4_cmds.h','engine/shadPS4/src/video_core/amdgpu/regs.cpp','engine/shadPS4/src/video_core/amdgpu/regs_vertex.h','engine/shadPS4/src/video_core/amdgpu/regs_color.h'],'license':'GPL-2.0-or-later'},
'repeat':'python -B ctrport/check_pending_startup.py, then python -B ctrport/audit_first_draw_capture.py (under shared lock)'}
(root/'reports/reconstruction/infra/first_draw_shader_capture.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'first_draw_word':first,'stages':[(x['vertex'],x['validated_code_bytes']) for x in e],'active_stubs':r['active_stubs'],'R3':False}))
