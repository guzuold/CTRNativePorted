# SPDX-License-Identifier: GPL-2.0-only
# Real filesystem/ZIP tests, synthetic program payloads; never starts the game.
param([string]$OutputDirectory = 'runtime/user/update-tests')
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/launcher_update.ps1" -Mode Library
$suiteRoot = Join-Path (Split-Path $PSScriptRoot -Parent) $OutputDirectory
$suiteRoot = Join-Path $suiteRoot ([guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($suiteRoot) | Out-Null
$script:passed = 0
function Assert($Condition, [string]$Message) {
    if (-not $Condition) { throw "FAIL: $Message" }
    $script:passed++
}
function Reject([scriptblock]$Action, [string]$Message) {
    $rejected = $false
    try { & $Action | Out-Null } catch { $rejected = $true }
    Assert $rejected $Message
}
function Release([string]$Tag) {
    return [pscustomobject]@{tag_name=$Tag; draft=$false; prerelease=$true; body='fixture'; assets=@(
        [pscustomobject]@{name="CTRNativePorted-$Tag-win64.zip"; size=100; digest=('sha256:' + ('a'*64));
            browser_download_url="https://github.com/guzuold/CTRNativePorted/releases/download/$Tag/CTRNativePorted-$Tag-win64.zip"})}
}
function Make-Zip([string]$Path, [string[]]$Names) {
    $zip=[IO.Compression.ZipFile]::Open($Path, 'Create')
    try {
        foreach($name in $Names) {
            $entry=$zip.CreateEntry($name)
            $writer=[IO.StreamWriter]::new($entry.Open())
            try { $writer.Write("new:$name") } finally { $writer.Dispose() }
        }
    } finally { $zip.Dispose() }
}
function Fixture([string]$Name) {
    $root=Join-Path $suiteRoot $Name
    [IO.Directory]::CreateDirectory($root) | Out-Null
    [IO.File]::WriteAllText((Join-Path $root 'launcher.exe'),'old launcher')
    $updates=Get-UpdateDirectory $root
    $txn=Join-Path $updates ('txn-'+[guid]::NewGuid().ToString('N'))
    [IO.Directory]::CreateDirectory($txn) | Out-Null
    $zip=Join-Path $txn 'package.zip'
    Make-Zip $zip @('launcher.exe','build/CTRPort-gpu/ctrport.exe','build/CTRPort-gpu/ctr_gpu.dll','build/CTRPort-gpu/ctr_hle.dll','README.md')
    $files=@(Expand-CheckedPackage $zip (Join-Path $txn 'stage'))
    Write-JsonFile (Join-Path $txn 'plan.json') @{root=$root;version='v0.4.1';state='Prepared';files=$files}
    [IO.File]::WriteAllText((Join-Path $root 'runtime/user/save-canary.bin'),'do not change')
    return @{root=$root;txn=$txn}
}
$offer=Select-Update @((Release 'v0.9.0'),(Release 'v0.10.0'),(Release 'v0.4.0')) '0.4.1'
Assert ($offer.version -eq 'v0.10.0') 'numeric ordering and prereleases'
Assert ($null -eq (Select-Update @((Release 'v0.4.0')) '0.4.1')) 'no downgrade'
Assert ($null -eq (Select-Update @((Release 'v0.4.1')) '0.4.1')) 'equal version'
$draft=Release 'v9.0.0'; $draft.draft=$true
Assert ($null -eq (Select-Update @($draft) '0.4.1')) 'ignore drafts'
foreach($bad in @('1.2','1.02.3','1.2.3/evil','1.2.3-beta','-1.2.3')) { Reject { Get-ReleaseVersion $bad } "invalid version $bad" }
$bad=Release 'v0.5.0'; $bad.assets[0].browser_download_url='https://evil.example/a.zip'
Assert ([bool](Select-Update @($bad) '0.4.1').error) 'reject off-repo download'
$bad=Release 'v0.5.0'; $bad.assets[0].digest=''
Assert ([bool](Select-Update @($bad) '0.4.1').error) 'require digest'
foreach($path in @('../outside','/launcher.exe','runtime/user/save.bin','dump/eboot.bin','assets/launcher/../evil.png',
    'assets/launcher/con.png','assets/launcher/a.png:evil','assets\launcher\a.png','build/CTRPort-gpu/evil.exe','launcher.exe.')) {
    Reject { Assert-PackagePath $path } "reject path $path"
}
$fixture=Fixture 'success'
Install-Update $fixture.root $fixture.txn
Assert ([IO.File]::ReadAllText((Join-Path $fixture.root 'launcher.exe')) -eq 'new:launcher.exe') 'launcher replaced'
Assert ((Read-JsonFile (Join-Path $fixture.txn 'journal.json')).state -eq 'Complete') 'durable success'
Assert ([IO.File]::ReadAllText((Join-Path $fixture.root 'runtime/user/save-canary.bin')) -eq 'do not change') 'preserve user data'
Recover-Update $fixture.root
Assert (-not (Test-Path -LiteralPath (Join-Path $fixture.root 'runtime/user/updates/pending.json'))) 'success pending cleared'
Reject { Install-Update $fixture.root $fixture.txn } 'cannot replay transaction'
$fixture=Fixture 'corrupt'
[IO.File]::WriteAllText((Join-Path $fixture.txn 'stage/launcher.exe'),'tampered')
Reject { Install-Update $fixture.root $fixture.txn } 'staged tampering rejected'
Assert ([IO.File]::ReadAllText((Join-Path $fixture.root 'launcher.exe')) -eq 'old launcher') 'tampering leaves install intact'
$fixture=Fixture 'locked'
$locked=[IO.File]::Open((Join-Path $fixture.root 'launcher.exe'),'Open','Read','None')
try { Reject { Install-Update $fixture.root $fixture.txn } 'locked executable rejected' } finally { $locked.Dispose() }
Assert ([IO.File]::ReadAllText((Join-Path $fixture.root 'launcher.exe')) -eq 'old launcher') 'locked install intact'
$fixture=Fixture 'rollback'
# Inject a disk failure after the first real replacement. Restore still executes
# the original filesystem code, verifying the externally visible rollback.
$originalReplace=${function:Replace-ProgramFile}
$script:copies=0
function Replace-ProgramFile([string]$Source,[string]$Target) {
    $script:copies++
    if($script:copies -eq 2) { throw 'Injected disk failure' }
    & $originalReplace $Source $Target
}
try { Reject { Install-Update $fixture.root $fixture.txn } 'installation failure' }
finally { ${function:Replace-ProgramFile}=$originalReplace }
Assert ([IO.File]::ReadAllText((Join-Path $fixture.root 'launcher.exe')) -eq 'old launcher') 'rollback restores launcher'
Assert (-not (Test-Path -LiteralPath (Join-Path $fixture.root 'build/CTRPort-gpu/ctrport.exe'))) 'rollback removes newly added files'
Assert ((Read-JsonFile (Join-Path $fixture.txn 'journal.json')).state -eq 'RolledBack') 'rollback journal'
$fixture=Fixture 'interrupted'
Install-Update $fixture.root $fixture.txn
$journal=Read-JsonFile (Join-Path $fixture.txn 'journal.json'); $journal.state='Applying'
Write-JsonFile (Join-Path $fixture.txn 'journal.json') $journal
Recover-Update $fixture.root
Assert ([IO.File]::ReadAllText((Join-Path $fixture.root 'launcher.exe')) -eq 'old launcher') 'interrupted transaction recovered'
$badzip=Join-Path $suiteRoot 'duplicate.zip'
Make-Zip $badzip @('launcher.exe','LAUNCHER.EXE')
Reject { Expand-CheckedPackage $badzip (Join-Path $suiteRoot 'duplicate') } 'case insensitive ZIP duplicates'
$badzip=Join-Path $suiteRoot 'escape.zip'
Make-Zip $badzip @('../outside')
Reject { Expand-CheckedPackage $badzip (Join-Path $suiteRoot 'escape') } 'ZIP traversal'
Assert (-not (Test-Path -LiteralPath (Join-Path $suiteRoot 'outside'))) 'no traversal write'
$badzip=Join-Path $suiteRoot 'incomplete.zip'
Make-Zip $badzip @('launcher.exe')
Reject { Expand-CheckedPackage $badzip (Join-Path $suiteRoot 'incomplete') } 'incomplete runtime'
Reject { Assert-Download $badzip 1 ('0'*64) } 'download size mismatch'
Reject { Assert-Download $badzip (Get-Item $badzip).Length ('0'*64) } 'download checksum mismatch'
$fixture=Fixture 'junction'
$link=Join-Path $fixture.root 'assets'
New-Item -ItemType Junction -Path $link -Target $suiteRoot | Out-Null
try { Reject { Assert-NoReparse (Join-Path $link 'launcher/logo.png') } 'destination junction' }
finally { [IO.Directory]::Delete($link) } # unlink only; never traverse the fixture junction
Reject { Assert-Transaction $fixture.root $suiteRoot } 'outside transaction'
[IO.Directory]::CreateDirectory((Join-Path $fixture.root '.git')) | Out-Null
Reject { Get-UpdateDirectory $fixture.root } 'source checkout protected'
$report=@{passed=$script:passed;failed=0;scope='synthetic Windows filesystem and ZIP tests; no retail execution';artifacts=$suiteRoot}
Write-JsonFile (Join-Path $suiteRoot 'report.json') $report
$report | ConvertTo-Json
