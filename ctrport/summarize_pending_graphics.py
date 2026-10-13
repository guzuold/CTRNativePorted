# SPDX-License-Identifier: GPL-2.0-only
"""Extract bounded resource requirements at/after the measured cold draw boundary.

Lookahead census entries are requirements, never credited as executed GPU work.
No shader words, descriptor words, texture bytes, or development symbols are saved.
"""
import hashlib,json,pathlib,sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
def validate_census(cold,raw):
    assert cold.get('gnm_census_sha256'), 'cold report lacks a bound census; regenerate cold evidence'
    assert hashlib.sha256(raw).hexdigest()==cold['gnm_census_sha256'], 'census belongs to another cold run'

def buffered_allocations(rows,stop_index,word):
    following=next((i for i in range(stop_index+1,len(rows)) if rows[i].get('scope')=='original_GnmSubmitFlip_command_cursor'),len(rows))
    return [dict(row,execution='allocation/upload observation only; no GPU draw credit') for row in rows[stop_index+1:following] if row.get('scope')=='original_GnmSubmitFlip_image_allocation_observation' and row.get('word',-1)>=word]

def submission_allocations(rows,start_index,stop_index,word):
    prefix=[dict(row,execution='allocation/upload observation only; no GPU draw credit') for row in rows[start_index:stop_index] if row.get('scope')=='original_GnmSubmitFlip_image_allocation_observation' and row.get('word',-1)>=word]
    return prefix+buffered_allocations(rows,stop_index,word)
def main():
    reports=ROOT/'reports/reconstruction/infra'
    cold=json.loads((reports/'json2_compat_startup.json').read_text())
    assert hashlib.sha256((ROOT/'build/CTRPort-infra/ctrport.exe').read_bytes()).hexdigest()==cold['exe_sha256']
    path=ROOT/'runtime/user/logs/submit_flip_census.jsonl'
    raw=path.read_bytes()
    validate_census(cold,raw)
    if '--control' in sys.argv:
        for altered,data in (({},raw),(cold,raw+b'\n')):
            try:validate_census(altered,data)
            except AssertionError:continue
            raise AssertionError('invalid census was admitted')
        print(json.dumps({'scope':'cold_census_binding_control','matched_real_census':True,'missing_binding_rejected':True,'changed_census_rejected':True,'GPU_execution':False}))
        return
    rows=[json.loads(line) for line in raw.splitlines()]
    stops=[row for row in rows if (row.get('scope')=='original_GnmSubmitFlip_ordered_route_handoff' and row.get('missing_whole_GPU_handler')) or (row.get('scope')=='original_GnmSubmitFlip_color_guest_writeback' and row.get('status'))]
    if not stops:
        stops=[row for row in rows if row.get('scope')=='original_GnmSubmitFlip_ordered_submission_complete' and not row.get('status')]
    assert stops, 'no measured boundary or completed submission in this census'
    stop=stops[-1]; word=stop['word']; requirements=[];colors=[];vertices=[]
    stop_index=max(i for i,row in enumerate(rows) if row is stop)
    starts=[i for i,row in enumerate(rows[:stop_index]) if row.get('scope')=='original_GnmSubmitFlip_command_cursor']
    assert starts, 'boundary has no enclosing command cursor'
    # Allocation observations are buffered by the runtime and flushed after the
    # handoff. Keep that suffix, but never cross into another submission.
    # New runtimes flush preparation before execution, so a publication failure
    # still has its actual allocation/readback diagnostics in the prefix.
    allocations=submission_allocations(rows,starts[-1],stop_index,word)
    if '--allocation-control' in sys.argv:
        terminal=[row for row in allocations if row['word']==word]
        assert terminal, 'current terminal has no image allocation observation'
        # Append a synthetic next submission to the actual census. Its reused
        # word/sharp identity must never be attributed to the preceding stop.
        sentinel={'scope':'original_GnmSubmitFlip_image_allocation_observation','word':word,'sharp_index':0,'create_status':0,'control_sentinel':True}
        altered=rows+[{'scope':'original_GnmSubmitFlip_command_cursor'},sentinel]
        assert submission_allocations(altered,starts[-1],stop_index,word)==allocations
        assert not any('execution' in row for row in rows), 'extractor mutated source observations'
        print(json.dumps({'scope':'buffered_allocation_scope_control','actual_scoped_records':len(allocations),'terminal_statuses':[{k:row[k] for k in ('word','create_status','upload_status','readback_status')} for row in terminal],'synthetic_next_submission_excluded':True,'source_unchanged':True,'exe_sha256':cold['exe_sha256'],'census_sha256':cold['gnm_census_sha256'],'GPU_draw_credit':False}))
        return
    rows=rows[starts[-1]:stop_index+1]
    for row in rows:
        if row.get('word',-1)<word:continue
        if row.get('scope')=='original_GnmSubmitFlip_graphics_census':
            for descriptor in row.get('owned_stage',{}).get('descriptors',[]):
                if descriptor.get('kind')!=1 or descriptor.get('effective_null_reasons'):continue
                requirements.append({'word':row['word'],'vertex':row['vertex'],'sharp':descriptor['sharp_index'],'extent_status':descriptor['image_extent_status'],'copy':descriptor.get('image_copy'),'payload_owned':descriptor.get('image_payload_owned'),'image_status':descriptor.get('image_status'),'effective_null_reasons':descriptor.get('effective_null_reasons'),'parameters':descriptor.get('image_parameters'),'execution':'pending boundary' if row['word']==word else 'lookahead only; not executed'})
        if row.get('scope')=='original_GnmSubmitFlip_vertex_preflight':vertices.append(row)
        if row.get('scope')=='original_GnmSubmitFlip_color_initialization' and row.get('upload_status'):
            colors.append(row)
    value={'scope':'pending_graphics_resource_requirements','exe_sha256':cold['exe_sha256'],'census_sha256':hashlib.sha256(raw).hexdigest(),'census_bytes':len(raw),'terminal_boundary':stop,'image_requirements':requirements,'image_allocation_observations':allocations,'color_upload_failures':colors,'vertex_preflights_lookahead':vertices,'lookahead_note':'Includes unexecuted later requirements in the same captured submission; no additional runtime progress is credited.','R3':False,'engine_replacements':0,'limitations':['Requirements after terminal word are lookahead metadata, not observed executions.','Extent failure does not authorize synthetic pixels, null substitution, or skipping the original draw.','Census path must be the matching fresh cold-run output; the cold checker archives earlier runs.']}
    (reports/'pending_graphics_resources.json').write_text(json.dumps(value,indent=2)+'\n')
    print(json.dumps({'word':word,'pending_images':len(requirements),'color_upload_failures':len(colors)}))
if __name__=='__main__':main()
