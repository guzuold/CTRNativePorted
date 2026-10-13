# SPDX-License-Identifier: GPL-2.0-only
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSHOME 'Modules/Microsoft.PowerShell.Utility/Microsoft.PowerShell.Utility.psd1')
. "$PSScriptRoot/pin_build_library.ps1"
$taskFixture=Join-Path (Split-Path $PSScriptRoot) 'build/CTRPort-infra/library-pin-fixture'
New-Item -ItemType Directory -Force -Path $taskFixture | Out-Null
$taskSource=Join-Path $taskFixture 'published.bin';$taskCopy=Join-Path $taskFixture 'pinned.bin'
[IO.File]::WriteAllBytes($taskSource,[byte[]](1,2,3,4))
$taskPin=Copy-CtrBuildLibrary -Source $taskSource -Destination $taskCopy
[IO.File]::WriteAllBytes($taskSource,[byte[]](5,6,7,8))
if((Get-FileHash -LiteralPath $taskCopy -Algorithm SHA256).Hash.ToLowerInvariant() -ne $taskPin.sha256){throw 'Published mutation changed pinned input'}
$taskRejected=$false
try{Copy-CtrBuildLibrary -Source $taskSource -Destination $taskSource | Out-Null}catch{$taskRejected=$true}
if(!$taskRejected -or [IO.File]::ReadAllBytes($taskSource)[0] -ne 5){throw 'Same-path input was not preserved'}
$taskReport=@{scope='build_library_pin_control';passed=$true;published_mutation_isolated=$true;same_path_rejected=$true;synthetic=$true;GPU_execution=$false;timestamp=[DateTime]::UtcNow.ToString('o')}
$taskReport | ConvertTo-Json | Set-Content -LiteralPath (Join-Path (Split-Path $PSScriptRoot) 'reports/reconstruction/infra/build_library_pin_control.json') -Encoding UTF8
$taskReport | ConvertTo-Json -Compress
