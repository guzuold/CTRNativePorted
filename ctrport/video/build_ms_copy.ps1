# SPDX-License-Identifier: GPL-2.0-only
# Whole public shadPS4 e3ce810f3a653f43ac64ebab63023de281a4103a shaders.
$ErrorActionPreference='Stop'
if($env:CTR_BUILD_LOCK_HELD -ne '1'){throw 'build-data lock required'}
$taskRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$taskBuild=Join-Path $taskRoot 'build/CTRPort-data/tiling-glslang-ninja'
$taskTool=Join-Path $taskBuild 'StandAlone/glslang.exe'
if(-not(Test-Path -LiteralPath $taskTool)){throw 'MISSING locally built pinned glslang; run build_tiling.ps1 first'}
$taskShaders=Join-Path $taskRoot 'engine/shadPS4/src/video_core/host_shaders'
$taskVariants=@(
 @{Name='ms_copy_vertex';Stage='vert';File='fs_tri.vert';Defines=@()},
 @{Name='ms_copy_from_single';Stage='frag';File='ms_image_blit.frag';Defines=@()},
 @{Name='ms_copy_from_multi';Stage='frag';File='ms_image_blit.frag';Defines=@('-DSRC_MSAA=1')}
)
foreach($taskVariant in $taskVariants){
 $taskArgs=@('-V','--target-env','vulkan1.1','-S',$taskVariant.Stage)+$taskVariant.Defines
 & $taskTool @taskArgs -o "$taskBuild/$($taskVariant.Name).spv" "$taskShaders/$($taskVariant.File)" *> "$taskBuild/$($taskVariant.Name).log"
 if($LASTEXITCODE -ne 0){Get-Content "$taskBuild/$($taskVariant.Name).log";throw 'Public multisample backing shader compile failed'}
}
Write-Output 'Whole public multisample backing shaders compiled; no GPU copy/draw or resolve authority.'
