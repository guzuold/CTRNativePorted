# SPDX-License-Identifier: GPL-2.0-or-later
param([Parameter(Mandatory=$true)][string]$Clang,
      [string]$OutputDirectory='build/CTRPort-friends-test',
      [switch]$BuildRuntimeLibrary)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$taskOut=Join-Path $taskRoot $OutputDirectory
New-Item -ItemType Directory -Force $taskOut | Out-Null
$taskVc='C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
foreach($line in (cmd /c "`"$taskVc`" >nul && set")){
    if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}
}
$sources=@('hle_tests/invitation_dialog_test.cpp','hle/invitation_dialog.cpp','hle/ctr_hle.cpp','hle/invitation_flow.cpp','hle/invitation_ui.cpp','hle/online_config.cpp','online_server/json.cpp') |
    ForEach-Object {Join-Path (Join-Path $taskRoot 'ctrport') $_}
& $Clang /nologo /std:c++20 /EHsc /DCTR_HLE_BUILD /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /W4 /WX @sources "/Fo$taskOut/" "/Fe$taskOut/invitation_dialog_test.exe" /link user32.lib gdi32.lib gdiplus.lib shlwapi.lib bcrypt.lib
if($LASTEXITCODE -ne 0){throw 'Invitation test build failed'}
if($BuildRuntimeLibrary) {
    $taskObjects=Join-Path $taskOut 'hle_obj'
    New-Item -ItemType Directory -Force $taskObjects | Out-Null
    $taskSources=Get-ChildItem (Join-Path $taskRoot 'ctrport/hle') -Filter '*.cpp' | Select-Object -ExpandProperty FullName
    $taskSources += Join-Path $taskRoot 'ctrport/online_server/json.cpp'
    $taskBuiltins=Join-Path (Split-Path (Split-Path $Clang)) 'lib/clang/19/lib/windows/clang_rt.builtins-x86_64.lib'
    & $Clang /nologo /LD /MD /O2 /std:c++latest /EHsc /DCTR_HLE_BUILD /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS @taskSources "/Fo$taskObjects/" "/Fe$taskOut/ctr_hle.dll" /link user32.lib gdi32.lib gdiplus.lib shlwapi.lib bcrypt.lib $taskBuiltins *> (Join-Path $taskOut 'hle-build.log')
    if($LASTEXITCODE -ne 0){Get-Content (Join-Path $taskOut 'hle-build.log') -Tail 30;throw 'HLE library build failed'}
}

