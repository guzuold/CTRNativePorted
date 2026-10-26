# SPDX-License-Identifier: GPL-2.0-only
# Windows PowerShell 5.1 helper. No game files or user data are installed.
[CmdletBinding()]
param(
    [ValidateSet('Check','Prepare','Apply','Recover','Library')][string]$Mode = 'Check',
    [string]$Root, [string]$CurrentVersion, [string]$Transaction,
    [string]$Result, [int]$ParentId = 0
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
Add-Type -AssemblyName System.IO.Compression.FileSystem

# Release tags: vMAJOR.MINOR.PATCH, or vMAJOR.MINOR.PATCH-uN for a release whose package needs update
# helper generation N (it adds program files that older helpers refuse). Helpers of 0.6.1 and older
# accept only the plain form, so they skip such releases and install the step release that brings
# a helper of the needed generation (UPDATES.md, "New program files").
$UpdaterGeneration = 2
function Get-ReleaseVersion([string]$Text) {
    if ($Text -cnotmatch '^v?(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(-u[1-9][0-9]*)?$') { throw "Unsupported release version: $Text" }
    return [version]"$($Matches[1]).$($Matches[2]).$($Matches[3])"
}
function Get-ReleaseGeneration([string]$Text) {
    if ($Text -cmatch '-u([1-9][0-9]*)$') { return [int]$Matches[1] }
    return 1
}
function Write-JsonFile([string]$Path, $Value) {
    $temp = "$Path.new"
    [IO.File]::WriteAllText($temp, ($Value | ConvertTo-Json -Depth 12), [Text.UTF8Encoding]::new($false))
    $flush = [IO.File]::Open($temp, 'Open', 'ReadWrite', 'None')
    try { $flush.Flush($true) } finally { $flush.Dispose() }
    if ([IO.File]::Exists($Path)) { [IO.File]::Replace($temp, $Path, [NullString]::Value) }
    else { [IO.File]::Move($temp, $Path) }
}
function Read-JsonFile([string]$Path) {
    return ([IO.File]::ReadAllText($Path) | ConvertFrom-Json)
}
function Replace-ProgramFile([string]$Source, [string]$Target) {
    # A crash must never leave a half-written launcher that cannot run recovery.
    $temp = "$Target.ctr-update-new"
    Assert-NoReparse $temp
    [IO.File]::Copy($Source, $temp, $true)
    $flush = [IO.File]::Open($temp, 'Open', 'ReadWrite', 'None')
    try { $flush.Flush($true) } finally { $flush.Dispose() }
    if ([IO.File]::Exists($Target)) { [IO.File]::Replace($temp, $Target, [NullString]::Value) }
    else { [IO.File]::Move($temp, $Target) }
}
function Assert-NoReparse([string]$Path) {
    $cursor = [IO.Path]::GetFullPath($Path)
    while ($cursor) {
        if (Test-Path -LiteralPath $cursor) {
            if ((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "Junctions and symbolic links are not allowed: $cursor"
            }
        }
        $parent = [IO.Directory]::GetParent($cursor)
        if ($null -eq $parent) { break }
        $cursor = $parent.FullName
    }
}
function Get-UpdateDirectory([string]$InstallRoot) {
    if (-not [IO.Path]::IsPathRooted($InstallRoot)) { throw 'Install root must be absolute.' }
    $InstallRoot = [IO.Path]::GetFullPath($InstallRoot).TrimEnd('\')
    Assert-NoReparse $InstallRoot
    if (Test-Path -LiteralPath (Join-Path $InstallRoot '.git')) { throw 'Updates are disabled in a source checkout.' }
    if (-not (Test-Path -LiteralPath (Join-Path $InstallRoot 'launcher.exe') -PathType Leaf)) {
        throw 'Only the portable release layout (launcher.exe at the root) can be updated.'
    }
    $dir = Join-Path $InstallRoot 'runtime/user/updates'
    Assert-NoReparse $dir
    [IO.Directory]::CreateDirectory($dir) | Out-Null
    return $dir
}
function Assert-Transaction([string]$InstallRoot, [string]$Path) {
    $updates = Get-UpdateDirectory $InstallRoot
    $full = [IO.Path]::GetFullPath($Path)
    if ([IO.Path]::GetDirectoryName($full) -ine $updates -or [IO.Path]::GetFileName($full) -notmatch '^txn-[a-f0-9]{32}$') {
        throw 'Invalid update transaction path.'
    }
    Assert-NoReparse $full
    return $full
}
function Assert-PackagePath([string]$Name) {
    if ($Name -notmatch '^[A-Za-z0-9_. /-]+$' -or $Name.Contains('\')) { throw "Unsafe ZIP path: $Name" }
    foreach ($part in $Name.Split('/')) {
        if (-not $part -or $part -eq '.' -or $part -eq '..' -or $part.EndsWith('.') -or $part.EndsWith(' ') -or
            $part -match '^(CON|PRN|AUX|NUL|COM[0-9]|LPT[0-9])(\.|$)') { throw "Unsafe ZIP path: $Name" }
    }
    # Closed list of program-owned locations; never runtime/, dump/, config or saves.
    $allowed = $Name -match '^(launcher\.exe|CTRNativePorted\.exe|launcher_update\.ps1|update-version\.json|LICENSE|[A-Za-z0-9_-]+\.(md|txt))$' -or
        $Name -match '^licenses/[A-Za-z0-9_. -]+\.(txt|md)$' -or
        $Name -match '^assets/(launcher|xbox-default)/[A-Za-z0-9_. -]+\.(png|json|md|txt)$' -or
        $Name -match '^build/CTRPort-gpu/(ctr_gpu\.dll|ctr_d3d\.dll|ctr_shaders\.exe|ctr_hle\.dll|ctr_online_server\.exe|ctrport\.exe|msvcp140\.dll|msvcp140_atomic_wait\.dll|vcruntime140\.dll|vcruntime140_1\.dll)$'
    if (-not $allowed) { throw "Package contains an unmanaged path: $Name" }
}
function Select-Update($Releases, [string]$Installed) {
    $current = Get-ReleaseVersion $Installed
    $best = $null
    foreach ($release in $Releases) {
        if ($release.draft) { continue }
        try { $version = Get-ReleaseVersion ([string]$release.tag_name) } catch { continue }
        if ((Get-ReleaseGeneration ([string]$release.tag_name)) -gt $UpdaterGeneration) { continue }
        if ($version -le $current -or ($null -ne $best -and $version -le (Get-ReleaseVersion $best.version))) { continue }
        $tag = [string]$release.tag_name
        $name = "CTRNativePorted-$tag-win64.zip"
        $assets = @($release.assets | Where-Object { $_.name -ceq $name })
        # Do not silently offer an older release when the newest has a broken package.
        $best = [ordered]@{ version=$tag; prerelease=[bool]$release.prerelease; notes=[string]$release.body
            url=''; sha256=''; size=0; error='' }
        if ($assets.Count -ne 1) { $best.error = 'Release ZIP is missing or ambiguous.'; continue }
        $asset = $assets[0]
        $expected = "https://github.com/guzuold/CTRNativePorted/releases/download/$tag/$name"
        if ($asset.browser_download_url -cne $expected -or $asset.digest -cnotmatch '^sha256:[0-9a-f]{64}$' -or
            [long]$asset.size -le 0 -or [long]$asset.size -gt 536870912) {
            $best.error = 'Release URL, SHA-256 or size is invalid.'; continue
        }
        $best.url = $expected; $best.sha256 = $asset.digest.Substring(7); $best.size = [long]$asset.size
    }
    return $best
}
function Get-OnlineUpdate([string]$Installed) {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    $releases = Invoke-RestMethod -Uri 'https://api.github.com/repos/guzuold/CTRNativePorted/releases?per_page=100' `
        -Headers @{'User-Agent'='CTRNativePorted-Updater'; 'Accept'='application/vnd.github+json'} -TimeoutSec 20
    return Select-Update $releases $Installed
}
function Assert-Download([string]$Path, [long]$Size, [string]$Sha256) {
    if ((Get-Item -LiteralPath $Path).Length -ne $Size -or
        (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ine $Sha256) { throw 'Downloaded ZIP checksum/size mismatch.' }
}
function Expand-CheckedPackage([string]$ZipPath, [string]$Stage) {
    $zip = [IO.Compression.ZipFile]::OpenRead($ZipPath)
    try {
        $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
        $files = @(); [long]$total = 0
        if ($zip.Entries.Count -gt 4096) { throw 'Too many ZIP entries.' }
        # Validate the entire directory before writing anything.
        foreach ($entry in $zip.Entries) {
            if ($entry.FullName.EndsWith('/')) { continue }
            Assert-PackagePath $entry.FullName
            if (-not $seen.Add($entry.FullName)) { throw 'Duplicate ZIP path.' }
            $unixType = ($entry.ExternalAttributes -shr 16) -band 0xF000
            if ($unixType -ne 0 -and $unixType -ne 0x8000) { throw 'ZIP links and special files are forbidden.' }
            $total += $entry.Length
            if ($total -gt 2147483648 -or $entry.Length -gt 536870912) { throw 'Unpacked ZIP is too large.' }
        }
        foreach ($required in @('launcher.exe','build/CTRPort-gpu/ctrport.exe','build/CTRPort-gpu/ctr_gpu.dll','build/CTRPort-gpu/ctr_hle.dll')) {
            if (-not $seen.Contains($required)) { throw "Incomplete release: $required" }
        }
        foreach ($entry in $zip.Entries) {
            if ($entry.FullName.EndsWith('/')) { continue }
            $destination = Join-Path $Stage $entry.FullName
            Assert-NoReparse $destination
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination)) | Out-Null
            [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $destination, $false)
            $files += [ordered]@{ path=$entry.FullName; sha256=(Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant() }
        }
        return $files
    } finally { $zip.Dispose() }
}
function Prepare-Update([string]$InstallRoot, [string]$Installed, $Offer) {
    if ($Offer.error) { throw $Offer.error }
    # Re-fetch metadata so an altered local offer cannot supply arbitrary URLs or hashes.
    $fresh = Get-OnlineUpdate $Installed
    if ($null -eq $fresh -or $fresh.version -cne $Offer.version -or $fresh.sha256 -cne $Offer.sha256 -or $fresh.error) {
        throw 'Release changed. Check for updates again.'
    }
    $updates = Get-UpdateDirectory $InstallRoot
    $txn = Join-Path $updates ('txn-' + [guid]::NewGuid().ToString('N'))
    [IO.Directory]::CreateDirectory($txn) | Out-Null
    $zip = Join-Path $txn 'package.zip'
    Invoke-WebRequest -UseBasicParsing -Uri $fresh.url -OutFile $zip -TimeoutSec 300
    Assert-Download $zip $fresh.size $fresh.sha256
    $files = @(Expand-CheckedPackage $zip (Join-Path $txn 'stage'))
    $versionFile = Join-Path $txn 'stage/update-version.json'
    if (Test-Path -LiteralPath $versionFile) {
        if ((Get-ReleaseVersion (Read-JsonFile $versionFile).version) -ne (Get-ReleaseVersion $fresh.version)) {
            throw 'The version in the package does not match the release tag.'
        }
    }
    # Run the currently installed helper from outside the program files being replaced.
    Copy-Item -LiteralPath $PSCommandPath -Destination (Join-Path $txn 'apply.ps1')
    $plan = [ordered]@{ root=[IO.Path]::GetFullPath($InstallRoot); version=$fresh.version; state='Prepared'; files=$files }
    Write-JsonFile (Join-Path $txn 'plan.json') $plan
    return $txn
}
function Restore-Update([string]$InstallRoot, [string]$Txn, $Journal) {
    foreach ($file in $Journal.files) {
        Assert-PackagePath $file.path
        $target = Join-Path $InstallRoot $file.path
        Assert-NoReparse $target
        if ($file.existed) {
            $backup = Join-Path (Join-Path $Txn 'backup') $file.path
            Assert-NoReparse $backup
            if ((Get-FileHash -LiteralPath $backup -Algorithm SHA256).Hash -ine $file.oldhash) { throw "Invalid rollback copy: $($file.path)" }
            Replace-ProgramFile $backup $target
        } elseif ([IO.File]::Exists($target)) {
            [IO.File]::Delete($target)
        }
    }
    $Journal.state = 'RolledBack'
    Write-JsonFile (Join-Path $Txn 'journal.json') $Journal
}
function Install-Update([string]$InstallRoot, [string]$Txn) {
    $Txn = Assert-Transaction $InstallRoot $Txn
    $plan = Read-JsonFile (Join-Path $Txn 'plan.json')
    if ($plan.root -ine [IO.Path]::GetFullPath($InstallRoot) -or $plan.state -ne 'Prepared') { throw 'Invalid update plan.' }
    $journalPath = Join-Path $Txn 'journal.json'
    if (Test-Path -LiteralPath $journalPath) { throw 'Transaction has already been applied; recover it first.' }
    $journal = [ordered]@{state='Applying'; files=@()}
    # All backups are complete before the durable journal permits any replacement.
    foreach ($file in $plan.files) {
        Assert-PackagePath $file.path
        $source = Join-Path (Join-Path $Txn 'stage') $file.path
        $target = Join-Path $InstallRoot $file.path
        Assert-NoReparse $source; Assert-NoReparse $target
        if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ine $file.sha256) { throw "Staged file checksum mismatch: $($file.path)" }
        $existed = [IO.File]::Exists($target); $oldhash = ''
        if ($existed) {
            # Refuse files in use before changing any file.
            $probe = [IO.File]::Open($target, 'Open', 'ReadWrite', 'None'); $probe.Dispose()
            $backup = Join-Path (Join-Path $Txn 'backup') $file.path
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($backup)) | Out-Null
            [IO.File]::Copy($target, $backup, $false)
            $oldhash = (Get-FileHash -LiteralPath $backup -Algorithm SHA256).Hash
        } elseif (Test-Path -LiteralPath $target) { throw "A directory blocks a program file: $target" }
        $journal.files += [ordered]@{path=$file.path; existed=$existed; oldhash=$oldhash}
    }
    Write-JsonFile $journalPath $journal
    Write-JsonFile (Join-Path (Get-UpdateDirectory $InstallRoot) 'pending.json') @{transaction=$Txn}
    try {
        foreach ($file in $plan.files) {
            $target = Join-Path $InstallRoot $file.path
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target)) | Out-Null
            Replace-ProgramFile (Join-Path (Join-Path $Txn 'stage') $file.path) $target
        }
        foreach ($file in $plan.files) {
            if ((Get-FileHash -LiteralPath (Join-Path $InstallRoot $file.path) -Algorithm SHA256).Hash -ine $file.sha256) {
                throw "Installed file checksum mismatch: $($file.path)"
            }
        }
        $journal.state = 'Complete'
        Write-JsonFile $journalPath $journal
    } catch {
        $failure = $_
        Restore-Update $InstallRoot $Txn $journal
        throw $failure
    }
}
function Recover-Update([string]$InstallRoot) {
    $pending = Join-Path (Get-UpdateDirectory $InstallRoot) 'pending.json'
    if (-not (Test-Path -LiteralPath $pending)) { return }
    $txn = Assert-Transaction $InstallRoot (Read-JsonFile $pending).transaction
    $journal = Read-JsonFile (Join-Path $txn 'journal.json')
    if ($journal.state -eq 'Applying') { Restore-Update $InstallRoot $txn $journal }
    elseif ($journal.state -notin @('Complete','RolledBack')) { throw 'Unknown update journal state.' }
    [IO.File]::Delete($pending)
}

if ($Mode -eq 'Library') { return }
$canRestart = $false
try {
    if ($Mode -eq 'Check') {
        $offer = Get-OnlineUpdate $CurrentVersion
        Write-JsonFile $Result @{ok=$true; available=($null -ne $offer); offer=$offer}
    } elseif ($Mode -eq 'Prepare') {
        $offer = (Read-JsonFile $Result).offer
        $txn = Prepare-Update $Root $CurrentVersion $offer
        Write-JsonFile $Result @{ok=$true; transaction=$txn}
    } else {
        # Open before waiting, so PID reuse cannot target a different process.
        if ($ParentId -gt 0) {
            try { $parentProcess = [Diagnostics.Process]::GetProcessById($ParentId) } catch { $parentProcess = $null }
            if ($null -ne $parentProcess -and -not $parentProcess.WaitForExit(120000)) { throw 'Launcher did not exit within two minutes.' }
        }
        $updates = Get-UpdateDirectory $Root
        $lock = [IO.File]::Open((Join-Path $updates 'install.lock'), 'OpenOrCreate', 'ReadWrite', 'None')
        $canRestart = $true
        try {
            Recover-Update $Root
            if ($Mode -eq 'Apply') { Install-Update $Root $Transaction; Recover-Update $Root }
        } finally { $lock.Dispose() }
        Start-Process -FilePath (Join-Path $Root 'launcher.exe') -WorkingDirectory $Root -WindowStyle Normal
    }
} catch {
    $errorText = $_.Exception.Message
    if ($Mode -in @('Check','Prepare')) {
        Write-JsonFile $Result @{ok=$false; error=$errorText}
    } else {
        # Keep a recoverable journal and backups on any failure.
        Add-Type -AssemblyName System.Windows.Forms
        [Windows.Forms.MessageBox]::Show("Update not completed: $errorText`nBackups are in runtime/user/updates.", 'CTRNativePorted update') | Out-Null
        # Restart only after a verified complete rollback. An incomplete rollback
        # must remain pending for the next recovery attempt.
        if ($canRestart -and $Mode -eq 'Apply' -and $Transaction) {
            try {
                $txn = Assert-Transaction $Root $Transaction
                $journalFile = Join-Path $txn 'journal.json'
                $safe = -not (Test-Path -LiteralPath $journalFile)
                if (-not $safe) { $safe = (Read-JsonFile $journalFile).state -eq 'RolledBack' }
                if ($safe) {
                    $updates = Get-UpdateDirectory $Root
                    $recoveryLock = [IO.File]::Open((Join-Path $updates 'install.lock'), 'OpenOrCreate', 'ReadWrite', 'None')
                    try { Recover-Update $Root } finally { $recoveryLock.Dispose() }
                    Start-Process -FilePath (Join-Path $Root 'launcher.exe') -WorkingDirectory $Root -WindowStyle Normal
                }
            } catch { } # original error and preserved journal remain authoritative
        }
    }
    exit 1
}
