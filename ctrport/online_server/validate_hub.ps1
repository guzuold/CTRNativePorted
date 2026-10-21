# SPDX-License-Identifier: GPL-2.0-or-later
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $root
$active=Get-Process launcher,ctrport -ErrorAction SilentlyContinue | Where-Object {$_.Path -like 'D:\CTR-Native Ported\*'}
if($active){throw 'User launcher/game is running; no default-port tests started.'}
# Probe exclusive binds without sending requests: process visibility can be restricted.
foreach($port in @(3074,8780)){
 $probe=[Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback,$port)
 $probe.Server.ExclusiveAddressUse=$true
 try{$probe.Start()}finally{$probe.Stop()}
}
$out=Join-Path $root 'docs/security/hub-evidence'
$results=@()
$groups=@(
 @{exe='build/CTRPort-claude/ctr_online_server_test.exe';prefix='server';modes=@('','--security-json','--security-blob','--security-http','--security-ports','--connection-policy-only','--friend-restart-only','--peer-only','--party-migration-only','--room-invite-only','--invite-pull-only')},
 @{exe='build/CTRPort-gpu/ctr_hle_online_test.exe';prefix='hle';modes=@('','--ingress','--offline','--relay','--relay-block','--relay-auto-direct','--relay-auto-fallback')})
foreach($group in $groups){foreach($mode in $group.modes){
 $name=$group.prefix+'-'+$(if($mode){$mode.TrimStart('-')}else{'default'})
 $lines=if($mode){& $group.exe $mode 2>&1}else{& $group.exe 2>&1}
 $exit=$LASTEXITCODE
 $lines | Set-Content (Join-Path $out ($name+'.log')) -Encoding UTF8
 $results+=@{suite=$name;exit=$exit}
 Write-Output "$name exit=$exit"
}}
$results|ConvertTo-Json|Set-Content (Join-Path $out 'suite-exits.json') -Encoding UTF8
if($results | Where-Object {$_.exit -ne 0}){exit 1}
