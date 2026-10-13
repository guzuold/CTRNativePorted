# SPDX-License-Identifier: GPL-2.0-only
"""Validate only screenshots explicitly bound to the latest cold run.

Nonblack pixels identify review candidates, never establish a game milestone.
"""
import hashlib,json,pathlib,struct,sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
def bitmap_metadata(raw):
    if len(raw)<54 or raw[:2]!=b'BM':raise ValueError('not BMP')
    length,offset=struct.unpack_from('<IxxxxI',raw,2)
    size,width,height,planes,bits,compression=struct.unpack_from('<IiiHHI',raw,14)
    if size!=40 or planes!=1 or bits!=32 or compression or width<=0 or not height or offset!=54:raise ValueError('not capture_client32B format')
    count=width*abs(height)
    if count>4096*4096 or len(raw)!=offset+count*4 or length!=len(raw):raise ValueError('invalid bounded BMP span')
    pixels=memoryview(raw)[offset:]
    nonblack=sum(bool(pixels[i] or pixels[i+1] or pixels[i+2]) for i in range(0,len(pixels),4))
    return {'width':width,'height':abs(height),'RGB_nonblack_pixels':nonblack,'candidate_for_visual_review':bool(nonblack)}
def main():
    if '--control' in sys.argv:
        # Existing actual desktop-duplication black frame, not a generated oracle.
        raw=(ROOT/'runtime/user/captures/infra_first_present.bmp').read_bytes()
        assert hashlib.sha256(raw).hexdigest()=='3024a9b968e95abca44be721f9ce5b72669e37e687f3d2ac973ac36d9727b629'
        observed=bitmap_metadata(raw)
        assert observed['RGB_nonblack_pixels']==0
        bad=bytearray(raw);struct.pack_into('<I',bad,10,len(raw)+1)
        for value in (raw[:-1],bytes(bad),b'not an image'):
            try:bitmap_metadata(value)
            except ValueError:continue
            raise AssertionError('malformed capture admitted')
        print(json.dumps({'scope':'bound_capture_parser_control','actual_historical_black_frame':observed,'truncated_rejected':True,'bad_offset_rejected':True,'bad_magic_rejected':True,'R3':False}));return
    cold=json.loads((ROOT/'reports/reconstruction/infra/json2_compat_startup.json').read_text())
    assert hashlib.sha256((ROOT/'build/CTRPort-infra/ctrport.exe').read_bytes()).hexdigest()==cold['exe_sha256']
    records=[]
    for capture in cold.get('captures',[]):
        if capture.get('exit')!=0 or not capture.get('path'):
            records.append({'frame':capture['frame'],'status':capture.get('status','capture_failed')});continue
        path=(ROOT/capture['path']).resolve()
        assert path.is_relative_to((ROOT/'runtime/user/captures').resolve()), 'capture outside workspace capture directory'
        raw=path.read_bytes();assert capture.get('sha256')==hashlib.sha256(raw).hexdigest(), 'screenshot not bound to this run'
        records.append({'frame':capture['frame'],'path':capture['path'],'sha256':capture['sha256'],**bitmap_metadata(raw),'status':'requires visual observation and baseline comparison'})
    result={'scope':'latest_cold_capture_admission','exe_sha256':cold['exe_sha256'],'captures':records,'R3':False,'engine_replacements':0,'limitations':['A nonblack count alone cannot identify game content or exclude occlusion.','Visual observation and baseline comparison remain required.','No screenshot from an older run is imported.']}
    (ROOT/'reports/reconstruction/infra/cold_capture_admission.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result))
if __name__=='__main__':main()
