# SPDX-License-Identifier: GPL-2.0-only
$ErrorActionPreference='Stop'
if ($env:CTR_BUILD_LOCK_HELD -ne '1') { throw 'Run through tools/lane_lock.py --name build-data --lane data' }
$taskRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$taskBuild=Join-Path $taskRoot 'build/CTRPort-data'
New-Item -ItemType Directory -Force $taskBuild | Out-Null
$taskVc='C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
$taskLines=cmd /c "`"$taskVc`" >nul && set"
foreach($taskLine in $taskLines){if($taskLine -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
# Public auxiliary SPIRV used only by authored RectList pipeline controls.
& "$PSScriptRoot/build_shader.ps1" -CompilerCompanion
if($LASTEXITCODE -ne 0){throw 'Public auxiliary compiler controls failed'}
$taskInclude=Join-Path $taskRoot 'engine/shadPS4/externals/vulkan-headers/include'
& cl.exe /nologo /c /O2 /W4 /WX /EHsc /std:c++17 /wd4191 "/I$taskInclude" "$PSScriptRoot/device_control.cpp" "/Fo$taskBuild/device_control.obj" *> "$taskBuild/build.log"
if($LASTEXITCODE -ne 0){throw 'Vulkan control compilation failed'}
& cl.exe /nologo /TC /c /O2 /W4 /WX /std:c17 "$PSScriptRoot/session_api_c_control.c" "/Fo$taskBuild/session_api_c_control.obj" >> "$taskBuild/build.log" 2>&1
if($LASTEXITCODE -ne 0){throw 'C session consumer compilation failed'}
& cl.exe /nologo /c /O2 /W4 /WX /EHsc /std:c++17 "$PSScriptRoot/gnm_init.cpp" "/Fo$taskBuild/gnm_init.obj" >> "$taskBuild/build.log" 2>&1
if($LASTEXITCODE -ne 0){throw 'Gnm init compilation failed'}
& lib.exe /nologo "/OUT:$taskBuild/ctr_video_candidate.lib" "$taskBuild/device_control.obj" "$taskBuild/session_api_c_control.obj" "$taskBuild/gnm_init.obj" >> "$taskBuild/build.log" 2>&1
if($LASTEXITCODE -ne 0){throw 'Vulkan library failed'}
& cl.exe /nologo /O2 /W4 /WX /EHsc /std:c++17 "$PSScriptRoot/control_main.cpp" "/Fo$taskBuild/control_main.obj" "/Fe$taskBuild/video_control.exe" "$taskBuild/ctr_video_candidate.lib" user32.lib >> "$taskBuild/build.log" 2>&1
if($LASTEXITCODE -ne 0){throw 'Vulkan control link failed'}
$taskTiling=Join-Path $taskBuild 'tiling-glslang-ninja'
if(-not(Test-Path -LiteralPath "$taskTiling/depth1_ms2_tile.spv") -or -not(Test-Path -LiteralPath "$taskTiling/depth1_ms2_detile.spv") -or -not(Test-Path -LiteralPath "$taskTiling/depth0_16_flattened_tile.spv") -or -not(Test-Path -LiteralPath "$taskTiling/depth0_16_flattened_detile.spv") -or -not(Test-Path -LiteralPath "$taskTiling/depth0_32_tile.spv") -or -not(Test-Path -LiteralPath "$taskTiling/depth0_32_detile.spv") -or -not(Test-Path -LiteralPath "$taskTiling/display32_tile.spv") -or -not(Test-Path -LiteralPath "$taskTiling/display32_detile.spv") -or
   -not(Test-Path -LiteralPath "$taskTiling/thin32_tile.spv") -or -not(Test-Path -LiteralPath "$taskTiling/thin32_detile.spv") -or -not(Test-Path -LiteralPath "$taskTiling/thin16_tile.spv") -or -not(Test-Path -LiteralPath "$taskTiling/thin16_detile.spv")) {
    & "$PSScriptRoot/build_tiling.ps1"
}
& "$taskTiling/StandAlone/glslang.exe" -V --target-env vulkan1.1 -S frag -o "$taskBuild/mrt_depth_control.frag.spv" "$PSScriptRoot/mrt_depth_control.frag" *> "$taskBuild/mrt_depth_fragment_compile.log"
if($LASTEXITCODE -ne 0){throw 'Authored dual MRT shader failed'}
& "$taskTiling/StandAlone/glslang.exe" -V --target-env vulkan1.1 -S comp -o "$taskBuild/compute_depth_ms_control.spv" "$PSScriptRoot/compute_depth_ms_control.comp" *> "$taskBuild/compute_depth_ms_compile.log"
if($LASTEXITCODE -ne 0){throw 'Authored MS depth shader failed'}
& "$taskTiling/StandAlone/glslang.exe" -V --target-env vulkan1.1 -S comp -o "$taskBuild/compute_depth_ms_full_control.spv" "$PSScriptRoot/compute_depth_ms_full_control.comp" *> "$taskBuild/compute_depth_ms_full_compile.log"
if($LASTEXITCODE -ne 0){throw 'Authored full MS depth shader failed'}
& "$taskTiling/StandAlone/glslang.exe" -V --target-env vulkan1.1 -S frag -o "$taskBuild/mrt_r32_control.frag.spv" "$PSScriptRoot/mrt_r32_control.frag" *> "$taskBuild/mrt_r32_compile.log"
if($LASTEXITCODE -ne 0){throw 'Authored R32float MRT shader failed'}
& "$taskTiling/StandAlone/glslang.exe" -V --target-env vulkan1.1 -S frag -o "$taskBuild/sampled_draw_control.frag.spv" "$PSScriptRoot/sampled_draw_control.frag" *> "$taskBuild/sampled_draw_compile.log"
if($LASTEXITCODE -ne 0){throw 'Sampled draw shader failed'}
& "$taskTiling/StandAlone/glslang.exe" -V --target-env vulkan1.1 -S comp -o "$taskBuild/compute_sampled_r32_control.spv" "$PSScriptRoot/compute_sampled_r32_control.comp" *> "$taskBuild/compute_sampled_r32_compile.log"
if($LASTEXITCODE -ne 0){throw 'Sampled R32 component shader failed'}
& "$taskTiling/StandAlone/glslang.exe" -V --target-env vulkan1.1 -S comp -o "$taskBuild/compute_sampled_1d_control.spv" "$PSScriptRoot/compute_sampled_1d_control.comp" *> "$taskBuild/compute_sampled_1d_compile.log"
if($LASTEXITCODE -ne 0){throw 'Sampled 1D shader failed'}
foreach($taskArray in @(0,1)) {
 $taskBcArgs=@('-V','--target-env','vulkan1.1','-S','comp')
 if($taskArray){$taskBcArgs+='-DBC1_ARRAY=1'}
 & "$taskTiling/StandAlone/glslang.exe" @taskBcArgs -o "$taskBuild/compute_sampled_bc1_$taskArray.spv" "$PSScriptRoot/compute_sampled_bc1_control.comp" *> "$taskBuild/compute_sampled_bc1_$taskArray.log"
 if($LASTEXITCODE -ne 0){throw 'BC1 sampled control shader failed'}
}
# Whole pinned public tiling.comp: Thin1DThin=13, array mode2, thin micro mode1.
foreach($taskDirection in @('tile','detile')) {
 $taskMicroArgs=@('-V','--target-env','vulkan1.1','-S','comp','-DBITS_PER_PIXEL=32','-DNUM_SAMPLES=1','-DARRAY_MODE=2','-DMICRO_TILE_MODE=1','-DMICRO_TILE_THICKNESS=1','-DPIPE_CONFIG=12','-DBANK_WIDTH=1','-DBANK_HEIGHT=1','-DNUM_BANKS=16','-DNUM_BANK_BITS=4','-DTILE_SPLIT_BYTES=512','-DMACRO_TILE_ASPECT=2')
 if($taskDirection -eq 'tile'){$taskMicroArgs+='-DIS_TILER=1'}
 & "$taskTiling/StandAlone/glslang.exe" @taskMicroArgs -o "$taskTiling/micro13_32_$taskDirection.spv" "$taskRoot/engine/shadPS4/src/video_core/host_shaders/tiling.comp" *> "$taskBuild/micro13_$taskDirection.log"
 if($LASTEXITCODE -ne 0){throw 'Public micro13 shader failed'}
}
foreach($taskDirection in @('tile','detile')) {
 $taskMicroArgs=@('-V','--target-env','vulkan1.1','-S','comp','-DBITS_PER_PIXEL=128','-DNUM_SAMPLES=1','-DARRAY_MODE=2','-DMICRO_TILE_MODE=1','-DMICRO_TILE_THICKNESS=1','-DPIPE_CONFIG=12','-DBANK_WIDTH=1','-DBANK_HEIGHT=1','-DNUM_BANKS=16','-DNUM_BANK_BITS=4','-DTILE_SPLIT_BYTES=512','-DMACRO_TILE_ASPECT=2')
 if($taskDirection -eq 'tile'){$taskMicroArgs+='-DIS_TILER=1'}
 & "$taskTiling/StandAlone/glslang.exe" @taskMicroArgs -o "$taskTiling/micro13_128_$taskDirection.spv" "$taskRoot/engine/shadPS4/src/video_core/host_shaders/tiling.comp" *> "$taskBuild/micro13_128_$taskDirection.log"
 if($LASTEXITCODE -ne 0){throw 'Public micro13 shader failed'}
}
foreach($taskDirection in @('tile','detile')) {
 $taskMicroArgs=@('-V','--target-env','vulkan1.1','-S','comp','-DBITS_PER_PIXEL=64','-DNUM_SAMPLES=1','-DARRAY_MODE=2','-DMICRO_TILE_MODE=1','-DMICRO_TILE_THICKNESS=1','-DPIPE_CONFIG=12','-DBANK_WIDTH=1','-DBANK_HEIGHT=1','-DNUM_BANKS=16','-DNUM_BANK_BITS=4','-DTILE_SPLIT_BYTES=512','-DMACRO_TILE_ASPECT=2')
 if($taskDirection -eq 'tile'){$taskMicroArgs+='-DIS_TILER=1'}
 & "$taskTiling/StandAlone/glslang.exe" @taskMicroArgs -o "$taskTiling/micro13_64_$taskDirection.spv" "$taskRoot/engine/shadPS4/src/video_core/host_shaders/tiling.comp" *> "$taskBuild/micro13_64_$taskDirection.log"
 if($LASTEXITCODE -ne 0){throw 'Public micro13 shader failed'}
}
& "$PSScriptRoot/build_ms_copy.ps1"
if($LASTEXITCODE -ne 0){throw 'Public backing-copy shaders failed'}
& "$taskTiling/StandAlone/glslang.exe" -V --target-env vulkan1.1 -S vert -o "$taskBuild/rectlist_control.vert.spv" "$PSScriptRoot/rectlist_control.vert" *> "$taskBuild/rectlist_vertex_compile.log"
if($LASTEXITCODE -ne 0){throw 'RectList authored vertex shader compile failed'}
& "$taskTiling/StandAlone/glslang.exe" -V --target-env vulkan1.1 -S comp -o "$taskBuild/compute_write_control.spv" "$PSScriptRoot/compute_write_control.comp" *> "$taskBuild/compute_write_compile.log"
if($LASTEXITCODE -ne 0){throw 'Authored compute writer compilation failed'}
& "$taskTiling/StandAlone/glslang.exe" -V --target-env vulkan1.1 -S comp -o "$taskBuild/compute_r8_channels_control.spv" "$PSScriptRoot/compute_r8_channels_control.comp" *> "$taskBuild/compute_r8_channels_compile.log"
if($LASTEXITCODE -ne 0){throw 'R8 channel control compilation failed'}
& "$taskTiling/StandAlone/glslang.exe" -V --target-env vulkan1.1 -S comp -o "$taskBuild/compute_sampled_control.spv" "$PSScriptRoot/compute_sampled_control.comp" *> "$taskBuild/compute_sampled_compile.log"
if($LASTEXITCODE -ne 0){throw 'Authored compute sampled reader compilation failed'}
& "$taskTiling/StandAlone/glslang.exe" -V --target-env vulkan1.1 -S comp -o "$taskBuild/compute_storage_control.spv" "$PSScriptRoot/compute_storage_control.comp" *> "$taskBuild/compute_storage_compile.log"
if($LASTEXITCODE -ne 0){throw 'Authored compute storage writer compilation failed'}
& "$taskTiling/StandAlone/glslang.exe" -V --target-env vulkan1.1 -S comp -o "$taskBuild/compute_storage_full_control.spv" "$PSScriptRoot/compute_storage_full_control.comp" *> "$taskBuild/compute_storage_full_compile.log"
if($LASTEXITCODE -ne 0){throw 'Authored full storage writer compilation failed'}
& "$taskTiling/StandAlone/glslang.exe" -V --target-env vulkan1.1 -S comp -o "$taskBuild/compute_storage_rgba_control.spv" "$PSScriptRoot/compute_storage_rgba_control.comp" *> "$taskBuild/compute_storage_rgba_compile.log"
if($LASTEXITCODE -ne 0){throw 'Authored RGBA8 storage writer compilation failed'}
& "$taskTiling/StandAlone/glslang.exe" -V --target-env vulkan1.1 -S comp -o "$taskBuild/compute_storage_rg8_control.spv" "$PSScriptRoot/compute_storage_rg8_control.comp" *> "$taskBuild/compute_storage_rg8_compile.log"
if($LASTEXITCODE -ne 0){throw 'Authored RGBA8 storage writer compilation failed'}
foreach($taskStage in @('vert','frag')) {
    & "$taskTiling/StandAlone/glslang.exe" -V --target-env vulkan1.1 -S $taskStage -o "$taskTiling/pipeline_control.$taskStage.spv" "$PSScriptRoot/pipeline_control.$taskStage" *> "$taskBuild/pipeline_$taskStage.log"
    if($LASTEXITCODE -ne 0){throw 'Authored pipeline control shader compilation failed'}
}
$env:CTR_VIDEO_TILING_SHADER_DIR=$taskTiling
cmd /c "`"$taskBuild/video_control.exe`" > `"$taskBuild/device_control.log`" 2>&1"
if($LASTEXITCODE -ne 0){throw 'Vulkan instance/device control failed'}
# Require actual execution evidence before publication; exit0 alone previously hid skipped controls.
$taskControlText=[IO.File]::ReadAllText((Join-Path $taskBuild 'device_control.log'))
foreach($taskMarker in @('storage dataformat3 ','storage dataformat4 ','storage dataformat10 ',
    'compute actual SSBO writer two exact','compute sampled texelFetch',
    'compute actual R32float STORAGE','RectList_depth_draw=','RG8 color attachment1920x1152 ',
    'MRT2 actual combined full1920x1080 draw color2/2 depth2 stencilEqual16',
    'MRT2 combined CABI actual draw fenced','Depth2DThin64 tile0 actual GPU2048x1152','Depth0 R32float actual1920x1080','RG8 sampled actual texelFetch',
    'MRT2 R32float/RGBA8 actual1920x1080','MRT2 color-only actual1920x1080',
    'sampled graphics changed upload after binding','sampled single-color actual64x64',
    'RectList D16 sample1 actual1024x1024','No attachment actual1024x1024',
    'RectList no attachment actual1024x1024','RectList no attachment actual viewport16384x16384','MS2 unequal depth sample0half','MS2 retile actual4147200 samples exact','Depth0 D16 actual1024x1024','D16 actual full1024x1024 linear GPU download','D16 actual drawn depth GPU download','D16 actual tiled download2097152B','Depth1 MS2 full1920x1152','MS2 full retained1920x1080','MS2 extent prewriter1920x1080','Depth lease actual MS2 survives','MS2 exact backing spans depth17694720','Depth residency exact address generation','Depth residency generations four plane overlap','R32 sampled full1920x1080','R32 sampled tile14 composition','R32 sampled half960x540','RG16float sampled64x64','RG16 tile13 composition','Immutable index owner U16 U32','Indexed actual U16 U32 triangle','Vk83 actual half-float indexed max3 stride40 exact124B','R16 tile0 tall2048x8192 actual GPU tiled33554432B roundtrip guards32 sampled16 replicatedR','TriangleStrip6 actual four vertices red64pixels','R8 tile8 actual1024 and2048 linear GPU','Indexed color1 MS2 retained D32S8','RGBA8 sampled1D64','RGBA8 sampled1D1024',
    'null sampled explicit capability real GPU texelFetch16','null sampled graphics filter0','null sampled graphics filter1',
    'single R32 retained D32S8 sample1','Nonindexed color1 MS2 retained D32S8 actual',
    'RectList D32S8 sample1 actual','singleDepth residency real960x540','D16 tall retained33554432 noStencil lease survivesRelease sampled16 quarterDepth replicatedR40404040','Inactive depth color1MS2 exact','Display2DThin tile10 RGBA8 storage1920x1080','singleDepth sampled alias real960x540')) {
    $taskOccurrences=([regex]::Matches($taskControlText,[regex]::Escape($taskMarker))).Count
    if($taskOccurrences -ne 7){throw "GPU control coverage missing: $taskMarker expected7 actual$taskOccurrences"}
}
if(([regex]::Matches($taskControlText,[regex]::Escape('mapped tiled snapshot slot0 actual Display2DThin'))).Count -ne 1){throw 'Mapped tiled presentation control missing: expected1'}
if(([regex]::Matches($taskControlText,[regex]::Escape('mapped tiled completion public previousSlot semantics'))).Count -ne 1){throw 'Mapped tiled label completion control missing: expected1'}
if(([regex]::Matches($taskControlText,[regex]::Escape('mapped tiled frame boundary actual barrier'))).Count -ne 1){throw 'Mapped tiled frame boundary control missing: expected1'}
if(([regex]::Matches($taskControlText,[regex]::Escape('mapped tiled identity gamma actual GPU present'))).Count -ne 1){throw 'Mapped identity gamma control missing: expected1'}
if(([regex]::Matches($taskControlText,[regex]::Escape('mapped tiled noEvent clock actual first present'))).Count -ne 1){throw 'Mapped no-event clock control missing: expected1'}
if(([regex]::Matches($taskControlText,[regex]::Escape('Display2DThin tile10 color upload actual'))).Count -ne 7){throw 'Tile10 color control missing: expected7'}
if(([regex]::Matches($taskControlText,[regex]::Escape('single RGBA8 retained D32S8 sample1'))).Count -ne 7){throw 'RGBA8 single depth GPU coverage missing: expected7'}
if(([regex]::Matches($taskControlText,[regex]::Escape('inactive MRT slot1 undefined null actualGPU'))).Count -ne 7){throw 'Inactive MRT slot GPU coverage missing: expected7'}
if(([regex]::Matches($taskControlText,[regex]::Escape('BC1 full mip layered GPU transfer'))).Count -ne 7){throw 'BC1 full mip/layer/sample coverage missing: expected7'}
if(([regex]::Matches($taskControlText,[regex]::Escape('BC1 public64 micro13 GPU'))).Count -ne 7){throw 'BC1 micro tiling coverage missing: expected7'}
if(([regex]::Matches($taskControlText,[regex]::Escape('BC3 public128 micro13 GPU'))).Count -ne 7){throw 'BC3 tiling coverage missing: expected7'}
if(([regex]::Matches($taskControlText,[regex]::Escape('BC3 compressed image7 shapes mip2'))).Count -ne 7){throw 'BC3 image coverage missing: expected7'}
if(([regex]::Matches($taskControlText,[regex]::Escape('RGBA8 micro13 actual1024x2308'))).Count -ne 7){throw 'RGBA8 micro13 image coverage missing: expected7'}
if(([regex]::Matches($taskControlText,[regex]::Escape('PM4 standalone wait passed'))).Count -ne 1){throw 'Standalone wait control missing: expected1'}
# Publish only after the complete actual-device/kernel/C17 suite succeeds.
# Infra can continue pinning the preceding verified generation while candidate tests run.
$taskPublished=Join-Path $taskBuild 'ctr_video.lib'
$taskReady=Join-Path $taskBuild 'ctr_video_ready.lib'
[IO.File]::Copy((Join-Path $taskBuild 'ctr_video_candidate.lib'),$taskReady,$true)
if([IO.File]::Exists($taskPublished)){[IO.File]::Replace($taskReady,$taskPublished,(Join-Path $taskBuild 'ctr_video_previous.lib'),$true)}
else{[IO.File]::Move($taskReady,$taskPublished)}
Get-Content "$taskBuild/device_control.log"
