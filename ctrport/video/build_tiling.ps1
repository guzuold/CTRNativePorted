# SPDX-License-Identifier: GPL-2.0-only
$ErrorActionPreference='Stop'
if($env:CTR_BUILD_LOCK_HELD -ne '1'){throw 'build-data lock required'}
$taskRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$taskBuild=Join-Path $taskRoot 'build/CTRPort-data/tiling-glslang-ninja'
$taskCmake='C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
if(-not(Test-Path -LiteralPath $taskCmake)){throw 'MISSING installed VS CMake'}
$taskVc='C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
$taskLines=cmd /c "`"$taskVc`" >nul && set"
foreach($taskLine in $taskLines){if($taskLine -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
$taskNinja='C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
New-Item -ItemType Directory -Force $taskBuild | Out-Null
& $taskCmake -S "$taskRoot/engine/shadPS4/externals/glslang" -B $taskBuild -G Ninja "-DCMAKE_MAKE_PROGRAM=$taskNinja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TRY_COMPILE_CONFIGURATION=Release -DCMAKE_POLICY_DEFAULT_CMP0141=NEW -DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded -DCMAKE_SUPPRESS_REGENERATION=ON -DBUILD_EXTERNAL=OFF -DENABLE_OPT=OFF -DENABLE_HLSL=OFF -DENABLE_SPVREMAPPER=OFF -DGLSLANG_TESTS=OFF -DGLSLANG_ENABLE_INSTALL=OFF -DENABLE_GLSLANG_BINARIES=ON *> "$taskBuild/configure.log"
if($LASTEXITCODE -ne 0){Get-Content "$taskBuild/configure.log" -Tail 15;throw 'glslang configure failed'}
& $taskCmake --build $taskBuild --config Release --target glslang-standalone --parallel 4 *> "$taskBuild/build.log"
if($LASTEXITCODE -ne 0){Get-Content "$taskBuild/build.log" -Tail 15;throw 'glslang build failed'}
Write-Output 'Pinned upstream glslang executable built locally for tiling shader; no install or game shader input.'

$taskTool=Join-Path $taskBuild 'StandAlone/glslang.exe'
if(-not(Test-Path -LiteralPath $taskTool)){throw 'MISSING built glslang executable'}
$taskDefinitions=@('-DBITS_PER_PIXEL=32','-DNUM_SAMPLES=1','-DARRAY_MODE=4','-DMICRO_TILE_MODE=0','-DMICRO_TILE_THICKNESS=1','-DPIPE_CONFIG=12','-DBANK_WIDTH=1','-DBANK_HEIGHT=1','-DNUM_BANKS=16','-DNUM_BANK_BITS=4','-DTILE_SPLIT_BYTES=512','-DMACRO_TILE_ASPECT=2')
foreach($taskDirection in @('detile','tile')) {
    $taskArgs=@('-V','--target-env','vulkan1.1','-S','comp')+$taskDefinitions
    if($taskDirection -eq 'tile'){$taskArgs+='-DIS_TILER=1'}
    & $taskTool @taskArgs -o "$taskBuild/display32_$taskDirection.spv" "$taskRoot/engine/shadPS4/src/video_core/host_shaders/tiling.comp" *> "$taskBuild/$taskDirection.log"
    if($LASTEXITCODE -ne 0){Get-Content "$taskBuild/$taskDirection.log";throw "Tiling shader compile failed: $taskDirection"}
}
Write-Output 'Whole public Display2DThin32/sample1 tiler+detiler shaders compiled; GPU dispatch and retail pixel comparison pending.'

# Thin2DThin (tile14) differs from display10 in MICRO_TILE_MODE, not allocation extent.
$taskThinDefinitions=$taskDefinitions | ForEach-Object { if($_ -eq '-DMICRO_TILE_MODE=0'){'-DMICRO_TILE_MODE=1'}else{$_} }
foreach($taskDirection in @('detile','tile')) {
    $taskArgs=@('-V','--target-env','vulkan1.1','-S','comp')+$taskThinDefinitions
    if($taskDirection -eq 'tile'){$taskArgs+='-DIS_TILER=1'}
    & $taskTool @taskArgs -o "$taskBuild/thin32_$taskDirection.spv" "$taskRoot/engine/shadPS4/src/video_core/host_shaders/tiling.comp" *> "$taskBuild/thin_$taskDirection.log"
    if($LASTEXITCODE -ne 0){Get-Content "$taskBuild/thin_$taskDirection.log";throw "Thin tiling shader compile failed: $taskDirection"}
}
Write-Output 'Whole public Thin2DThin32/sample1 shaders compiled separately; never substitute display microtiling.'

# Public CalculateTileSplit(tile14,16bpp,1sample): max(256,2*128)=256.
# CalculateMacrotileMode: bit_width(128/64)-1=1 => Mode_1x2_16.
# Bank height2/aspect2 gives macro extent128x128 (32bpp is128x64).
$taskThin16Definitions=$taskThinDefinitions | ForEach-Object {
    switch($_) {
        '-DBITS_PER_PIXEL=32' {'-DBITS_PER_PIXEL=16'}
        '-DBANK_HEIGHT=1' {'-DBANK_HEIGHT=2'}
        '-DTILE_SPLIT_BYTES=512' {'-DTILE_SPLIT_BYTES=256'}
        default {$_}
    }
}
foreach($taskDirection in @('detile','tile')) {
    $taskArgs=@('-V','--target-env','vulkan1.1','-S','comp')+$taskThin16Definitions
    if($taskDirection -eq 'tile'){$taskArgs+='-DIS_TILER=1'}
    & $taskTool @taskArgs -o "$taskBuild/thin16_$taskDirection.spv" "$taskRoot/engine/shadPS4/src/video_core/host_shaders/tiling.comp" *> "$taskBuild/thin16_$taskDirection.log"
    if($LASTEXITCODE -ne 0){Get-Content "$taskBuild/thin16_$taskDirection.log";throw "Thin16 tiling shader compile failed: $taskDirection"}
}
Write-Output 'Whole public Thin2DThin16/sample1 shaders compiled; GPU execution and RG8 image transfer still pending.'

# Tile0 Depth2DThin64,32bpp/sample1: hardware split64 => macro mode0,
# bankheight4/aspect4 => macro256x128; whole public depth microtiling.
$taskDepthDefinitions=$taskDefinitions | ForEach-Object {
 switch($_) {
  '-DMICRO_TILE_MODE=0' {'-DMICRO_TILE_MODE=2'}
  '-DBANK_HEIGHT=1' {'-DBANK_HEIGHT=4'}
  '-DTILE_SPLIT_BYTES=512' {'-DTILE_SPLIT_BYTES=64'}
  '-DMACRO_TILE_ASPECT=2' {'-DMACRO_TILE_ASPECT=4'}
  default {$_}
 }
}
foreach($taskDirection in @('detile','tile')) {
 $taskArgs=@('-V','--target-env','vulkan1.1','-S','comp')+$taskDepthDefinitions
 if($taskDirection -eq 'tile'){$taskArgs+='-DIS_TILER=1'}
 & $taskTool @taskArgs -o "$taskBuild/depth0_32_$taskDirection.spv" "$taskRoot/engine/shadPS4/src/video_core/host_shaders/tiling.comp" *> "$taskBuild/depth0_$taskDirection.log"
 if($LASTEXITCODE -ne 0){Get-Content "$taskBuild/depth0_$taskDirection.log";throw "Depth0 tiling shader compile failed: $taskDirection"}
}
Write-Output 'Whole public Depth2DThin64 tile0 32bpp sample1 shaders compiled; GPU transfer not yet admitted.'

# Tile0 D16/sample1 shares depth microtiling/split64/macro mode0 with D32.
$taskDepth16Definitions=$taskDepthDefinitions | ForEach-Object {if($_ -eq '-DBITS_PER_PIXEL=32'){'-DBITS_PER_PIXEL=16'}else{$_}}
# Authored dispatch flattening only; whole public tile0 address calculation unchanged.
$taskTallSource=[IO.File]::ReadAllText("$taskRoot/engine/shadPS4/src/video_core/host_shaders/tiling.comp").Replace('gl_GlobalInvocationID.x','(gl_GlobalInvocationID.x + gl_GlobalInvocationID.y * gl_NumWorkGroups.x * 64u)')
$taskTallPath=Join-Path $taskBuild 'depth0_16_dispatch.comp'
[IO.File]::WriteAllText($taskTallPath,$taskTallSource)
foreach($taskDirection in @('detile','tile')) {
 $taskArgs=@('-V','--target-env','vulkan1.1','-S','comp')+$taskDepth16Definitions
 if($taskDirection -eq 'tile'){$taskArgs+='-DIS_TILER=1'}
 & $taskTool @taskArgs -o "$taskBuild/depth0_16_flattened_$taskDirection.spv" $taskTallPath *> "$taskBuild/depth0_16_$taskDirection.log"
 if($LASTEXITCODE -ne 0){throw "Depth0 D16 compile failed: $taskDirection"}
}

# Whole public address functions; explicit interleaved sample dispatch adapter.
$taskPublicSource=[IO.File]::ReadAllText("$taskRoot/engine/shadPS4/src/video_core/host_shaders/tiling.comp")
$taskMainPosition=$taskPublicSource.IndexOf('void main() {')
if($taskMainPosition -lt 0){throw 'Missing public tiling main boundary'}
$taskSampleMain=[IO.File]::ReadAllText("$PSScriptRoot/depth_ms2_main.glsl")
$taskSampleSource=Join-Path $taskBuild 'depth1_ms2_adapter.comp'
[IO.File]::WriteAllText($taskSampleSource,$taskPublicSource.Substring(0,$taskMainPosition)+$taskSampleMain)
$taskMs2Definitions=$taskDepthDefinitions | ForEach-Object {
 switch($_) {
  '-DNUM_SAMPLES=1' {'-DNUM_SAMPLES=2'}
  '-DBANK_HEIGHT=4' {'-DBANK_HEIGHT=2'}
  '-DTILE_SPLIT_BYTES=64' {'-DTILE_SPLIT_BYTES=128'}
  '-DMACRO_TILE_ASPECT=4' {'-DMACRO_TILE_ASPECT=2'}
  default {$_}
 }
}
foreach($taskDirection in @('detile','tile')) {
 $taskArgs=@('-V','--target-env','vulkan1.1','-S','comp')+$taskMs2Definitions
 if($taskDirection -eq 'tile'){$taskArgs+='-DIS_TILER=1'}
 & $taskTool @taskArgs -o "$taskBuild/depth1_ms2_$taskDirection.spv" $taskSampleSource *> "$taskBuild/depth1_ms2_$taskDirection.log"
 if($LASTEXITCODE -ne 0){throw "Depth1 MS2 compile failed: $taskDirection"}
}
