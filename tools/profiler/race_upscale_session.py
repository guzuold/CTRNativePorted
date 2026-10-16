#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Isolated offline race test using the existing physical-controller input overlay.

run remains alive until its stop file appears. Other commands address only its
own input/capture files, never global keyboard focus or the user's saves.
"""
import argparse, ctypes, datetime, json, os, shutil, subprocess, sys, time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT))
from tools.lane_lock import lane_lock

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('command',choices=['run','pad','shot','stop','status','measure']);p.add_argument('--work',type=Path,required=True)
    p.add_argument('--mode',choices=['off','fsr1','dlss'],default='dlss');p.add_argument('--height',type=int,choices=[720,900,1080,1440,1800,2160],default=1080)
    p.add_argument('--mask',type=int,default=0);p.add_argument('--duration',type=float,default=.18)
    a=p.parse_args();work=a.work.resolve()
    if not work.is_relative_to(ROOT/'runtime/user/instances'):p.error('Work must be under runtime/user/instances')
    if a.command=='run':
        work.mkdir(parents=True,exist_ok=False);(work/'runtime/user/input').mkdir(parents=True)
        (work/'runtime/user/logs').mkdir(parents=True)
        for name in ['ctr_gpu.dll','ctr_hle.dll']:shutil.copy2(ROOT/'build/CTRPort-gpu'/name,work/name)
        shutil.copytree(ROOT/'runtime/user/savedata',work/'runtime/user/savedata')
        base=(ROOT/'dump/CUSA14876').resolve();patch=Path('dump/CUSA14876-patch')
        env={k:v for k,v in os.environ.items() if not k.startswith('CTR_')}
        env.update(CTR_GPU_LIBRARY='1',CTR_TLS_PATCH='1',CTR_GPU_LOG='error',CTR_NO_FRAME_OBSERVATION='1',CTR_BASE_DIR=str(base),CTR_PATCH_DIR=str(patch),CTR_SCENE_HEIGHT=str(a.height),CTR_UPSCALER=a.mode,CTR_GPU_FSR='0',CTR_GPU_WIDTH='1920',CTR_GPU_HEIGHT='1080',CTR_60FPS_PATCH='1',CTR_PAD_TEST_INPUT='1',CTR_AUDIO_MUTE='1',CTR_FRAME_TIMES=str(work/'frames.csv'),CTR_UPSCALE_CAPTURE_DIR=str(work),CTR_PRESENT_CAPTURE_TRIGGER=str(work/'capture.trigger'),CTR_DLSS_DUMP_TRIGGER=str(work/'motion.trigger'),CTR_DLSS_DUMP_DIR=str(work),CTR_DLSS_DUMP_FRAMES='3')
        cmd=[str(ROOT/'build/CTRPort-dev/ctrport.exe'),'--entry-after-libc-init-images',str(patch/'eboot.bin')]+[str(x) for x in sorted((base/'sce_module').glob('*.prx'))]
        with lane_lock('shared','claude',timeout=60), (work/'stdout.log').open('wb') as out,(work/'stderr.log').open('wb') as err:
            child=subprocess.Popen(cmd,cwd=work,env=env,stdout=out,stderr=err)
            (work/'session.json').write_text(json.dumps({'pid':child.pid,'mode':a.mode,'height':a.height,'started':datetime.datetime.now().isoformat(),'monotonic_ns':time.perf_counter_ns()},indent=2))
            print(work,child.pid,flush=True)
            try:
                while child.poll() is None and not (work/'stop').exists():time.sleep(.25)
            finally:
                code=child.poll()
                if code is None:child.terminate();child.wait(15)
                (work/'exit.json').write_text(json.dumps({'code':code,'controlled_stop':code is None}))
        return
    if a.command=='measure':
        import numpy as np
        start=time.perf_counter_ns()
        time.sleep(32)
        frames=np.loadtxt(work/'frames.csv',delimiter=',',dtype=np.int64)
        stamps=frames[(frames[:,1]>=start)&(frames[:,1]<start+30_000_000_000),1]
        if len(stamps)<3:raise RuntimeError('No guest presentations in measurement interval')
        delta=np.diff(stamps)/1e6
        result=dict(start_ns=start,end_ns=start+30_000_000_000,frames=len(stamps),seconds=float((stamps[-1]-stamps[0])/1e9),fps=float(1000/delta.mean()),p95_ms=float(np.percentile(delta,95)),p99_ms=float(np.percentile(delta,99)),one_percent_low_fps=float(1000/np.sort(delta)[-max(1,int(np.ceil(len(delta)*.01))):].mean()),scenario='Crash Cove; stationary at starting grid after green; AI active; no input/capture during measurement')
        (work/'benchmark.json').write_text(json.dumps(result,indent=2));print(json.dumps(result));return
    if a.command=='pad':
        if not 0<=a.mask<=65535 or not 0<a.duration<=1.5:p.error('mask/duration outside bounds')
        kernel=ctypes.WinDLL('kernel32');kernel.GetTickCount64.restype=ctypes.c_uint64
        (work/'runtime/user/input/pad.txt').write_text(f'{a.mask} {kernel.GetTickCount64()+int(a.duration*1000)}')
        time.sleep(a.duration+.12)
    elif a.command=='shot':
        start=time.time();(work/'capture.trigger').touch()
        while time.time()-start<15:
            files=[f for f in work.glob('present_rgba8*.raw') if f.stat().st_mtime>start]
            if files:
                import re
                from PIL import Image
                f=files[0];w,h=map(int,re.search(r'_(\d+)x(\d+)\.raw',f.name).groups())
                data=f.read_bytes()
                if len(data)!=w*h*4:time.sleep(.1);continue
                out=work/f'view_{datetime.datetime.now():%H%M%S}.png'
                Image.frombytes('RGBA',(w,h),data,'raw','BGRA').convert('RGB').resize((1280,720)).save(out);print(out);return
            time.sleep(.1)
        raise RuntimeError('No completed GPU capture')
    elif a.command=='stop':(work/'stop').touch()
    else:
        print((work/'session.json').read_text());print('exited',(work/'exit.json').exists())
        lines=(work/'frames.csv').read_text().splitlines() if (work/'frames.csv').exists() else []
        print('frames flushed',len(lines),'last',lines[-1:] )
if __name__=='__main__':main()
