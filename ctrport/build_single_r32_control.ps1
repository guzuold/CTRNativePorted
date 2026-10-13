# SPDX-License-Identifier: GPL-2.0-only
# Invoke under build-infra lane lock. Uses the existing pinned project compiler.
$ErrorActionPreference='Stop'
$taskCompiler='build/CTRPort-data/tiling-glslang-ninja/StandAlone/glslang.exe'
New-Item -ItemType Directory -Force build/CTRPort-infra/controls | Out-Null
foreach($taskStage in @('vert','frag')) {
 & $taskCompiler -V --target-env vulkan1.1 -S $taskStage -o "build/CTRPort-infra/controls/single_r32.$taskStage.spv" "ctrport/controls/single_r32.$taskStage"
 if($LASTEXITCODE -ne 0){throw 'Authored R32 control shader compilation failed'}
}
