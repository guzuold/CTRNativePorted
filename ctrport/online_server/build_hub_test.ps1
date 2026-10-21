# SPDX-License-Identifier: GPL-2.0-or-later
param([switch]$LegacyElection, [switch]$Baseline, [string]$BaselineRef="f4d7cc6", [string]$ObjectDirectory='build/CTRPort-dev/online_server_obj',[string]$OutputDirectory='build/hub-test')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if(![IO.Path]::IsPathRooted($ObjectDirectory)){$ObjectDirectory=Join-Path $root $ObjectDirectory}
if(![IO.Path]::IsPathRooted($OutputDirectory)){$OutputDirectory=Join-Path $root $OutputDirectory}
New-Item -ItemType Directory -Force $OutputDirectory|Out-Null
$lines=cmd /c '"C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat" >nul && set'
foreach($line in $lines){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
$cl=Join-Path $env:VCToolsInstallDir 'bin/HostX64/x64/cl.exe'
$objects=Get-ChildItem $ObjectDirectory -Filter *.obj|Where-Object Name -notin @('server_main.obj','server_test.obj','hub_election_test.obj')|ForEach-Object FullName
if($Baseline){
    $baselineSource=Join-Path $OutputDirectory 'baseline_server.cpp'
    $original=git -C $root show "${BaselineRef}:ctrport/online_server/server.cpp"
    if($LASTEXITCODE -ne 0){throw 'Cannot read pinned baseline'}
    $original=$original -replace 'open\("lobby/tcp/3074", SOCK_STREAM, (?:local_host|config.host), 3074, s\)', 'open("lobby/tcp/" + std::to_string(config.lobby_port), SOCK_STREAM, config.host, config.lobby_port, s)'
    # Only a test port and log accessor bridge for the new launcher object; no hub behavior changes.
    $original += 'namespace ctr_online { void Server::record_event(const char* name, Json fields) { impl_->record(name,std::move(fields)); } }'
    [IO.File]::WriteAllLines($baselineSource,$original,[Text.UTF8Encoding]::new($false))
    & $cl /nologo /c /std:c++20 /EHsc /O2 /W4 /WX /DWIN32_LEAN_AND_MEAN /DNOMINMAX "/I$PSScriptRoot" $baselineSource "/Fo$OutputDirectory/baseline_server.obj"
    if($LASTEXITCODE -ne 0){throw 'Baseline server build failed'}
    $objects=@($objects | Where-Object { [IO.Path]::GetFileName($_) -ne 'server.obj' }) + @(Join-Path $OutputDirectory 'baseline_server.obj')
}
$testSource=Join-Path $PSScriptRoot 'hub_election_test.cpp'
if($LegacyElection){
    Copy-Item (Join-Path $root 'docs/security/hub-evidence/before-election.hpp.txt') (Join-Path $OutputDirectory 'hub_election.hpp')
    $testSource=Join-Path $OutputDirectory 'before-election.cpp'
    Copy-Item (Join-Path $root 'docs/security/hub-evidence/before-election.cpp.txt') $testSource
}
& $cl /nologo /std:c++20 /EHsc /O2 /W4 /WX /DWIN32_LEAN_AND_MEAN /DNOMINMAX "/I$PSScriptRoot" $testSource $objects "/Fo$OutputDirectory/hub_election_test.obj" "/Fe$OutputDirectory/hub_election_test.exe" /link /SUBSYSTEM:CONSOLE
if($LASTEXITCODE -ne 0){throw 'Hub election build failed'}
