# SPDX-License-Identifier: GPL-2.0-only
# End-to-end process handoff with synthetic Windows executables, not the game.
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/launcher_update.ps1" -Mode Library
$testRoot = Join-Path (Split-Path $PSScriptRoot -Parent) ('runtime/user/update-process-tests/' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($testRoot) | Out-Null
$launcher=Join-Path $testRoot 'launcher.exe'
$source=@'
using System;
using System.IO;
using System.Threading;
class Fixture {
    static readonly string Revision="REVISION";
    static void Main() {
        string root=AppDomain.CurrentDomain.BaseDirectory;
        string dir=Path.Combine(root,"runtime","user","updates");
        Directory.CreateDirectory(dir);
        using(var guard=new FileStream(Path.Combine(dir,"install.lock"),FileMode.OpenOrCreate,FileAccess.ReadWrite,FileShare.None)) {
            if(Revision == "old") {
                File.WriteAllText(Path.Combine(root,"waiting.txt"),"ready");
                for(int i=0;i<600 && !File.Exists(Path.Combine(root,"exit.txt"));++i) Thread.Sleep(100);
            } else File.WriteAllText(Path.Combine(root,"restarted.txt"),"new");
        }
    }
}
'@
Add-Type -TypeDefinition ($source.Replace('REVISION','old')) -OutputAssembly $launcher -OutputType WindowsApplication
$updates=Get-UpdateDirectory $testRoot
$txn=Join-Path $updates ('txn-'+[guid]::NewGuid().ToString('N'))
$stage=Join-Path $txn 'stage'
[IO.Directory]::CreateDirectory($stage) | Out-Null
# Separate compilation process avoids Add-Type's duplicate class cache.
$newSource=Join-Path $testRoot 'fixture.cs'
[IO.File]::WriteAllText($newSource, $source.Replace('REVISION','new'))
$compiler=Join-Path $testRoot 'compile.ps1'
[IO.File]::WriteAllText($compiler, 'param($InputFile,$OutputFile); Add-Type -TypeDefinition ([IO.File]::ReadAllText($InputFile)) -OutputAssembly $OutputFile -OutputType WindowsApplication')
& "$env:SystemRoot/System32/WindowsPowerShell/v1.0/powershell.exe" -NoProfile -File $compiler -InputFile $newSource -OutputFile (Join-Path $stage 'launcher.exe')
if($LASTEXITCODE) { throw 'Fixture compile failed' }
Copy-Item -LiteralPath "$PSScriptRoot/launcher_update.ps1" -Destination (Join-Path $txn 'apply.ps1')
$file=@{path='launcher.exe';sha256=(Get-FileHash (Join-Path $stage 'launcher.exe')).Hash}
Write-JsonFile (Join-Path $txn 'plan.json') @{root=$testRoot;version='v0.4.1';state='Prepared';files=@($file)}
$oldHash=(Get-FileHash $launcher).Hash
$parent=Start-Process -FilePath $launcher -PassThru -WindowStyle Hidden
$helper=$null
try {
    $limit=[DateTime]::UtcNow.AddSeconds(10)
    while(-not (Test-Path (Join-Path $testRoot 'waiting.txt')) -and [DateTime]::UtcNow -lt $limit) { Start-Sleep -Milliseconds 100 }
    if(-not (Test-Path (Join-Path $testRoot 'waiting.txt'))) { throw 'Fixture parent did not start' }
    $arguments="-NoProfile -NonInteractive -ExecutionPolicy Bypass -File `"$txn/apply.ps1`" -Mode Apply -Root `"$testRoot`" -Transaction `"$txn`" -ParentId $($parent.Id)"
    $helper=Start-Process -FilePath "$env:SystemRoot/System32/WindowsPowerShell/v1.0/powershell.exe" -ArgumentList $arguments -PassThru -WindowStyle Hidden
    Start-Sleep -Seconds 2
    if($helper.HasExited -or (Get-FileHash $launcher).Hash -ne $oldHash) { throw 'Helper did not wait for parent exit' }
    [IO.File]::WriteAllText((Join-Path $testRoot 'exit.txt'),'exit')
    if(-not $helper.WaitForExit(30000) -or $helper.ExitCode -ne 0) { throw 'Update helper failed or timed out' }
    $limit=[DateTime]::UtcNow.AddSeconds(10)
    while(-not (Test-Path (Join-Path $testRoot 'restarted.txt')) -and [DateTime]::UtcNow -lt $limit) { Start-Sleep -Milliseconds 100 }
    if(-not (Test-Path (Join-Path $testRoot 'restarted.txt'))) { throw 'Updated launcher did not restart' }
    if((Get-FileHash $launcher).Hash -eq $oldHash) { throw 'Launcher was not replaced' }
    if(Test-Path (Join-Path $updates 'pending.json')) { throw 'Pending transaction was not cleared' }
    $report=@{passed=5;failed=0;scope='synthetic Windows executables: parent wait, install, restart, checksum, journal cleanup';artifacts=$testRoot}
    Write-JsonFile (Join-Path $testRoot 'report.json') $report
    $report | ConvertTo-Json
} finally {
    # Only processes created by this fixture may be terminated.
    if(-not $parent.HasExited) { $parent.Kill() }
    if($null -ne $helper -and -not $helper.HasExited) { $helper.Kill() }
}
