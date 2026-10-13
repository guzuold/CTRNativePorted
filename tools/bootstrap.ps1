# SPDX-License-Identifier: GPL-2.0-only
# Prepares a fresh clone for building:
#   1. fetches the pinned shadPS4 v0.18.0 sources (engine/shadPS4, with submodules);
#   2. downloads and verifies LLVM 19.1.1 into tools/toolchains/llvm-19.1.1;
#   3. checks the other prerequisites the build scripts expect.
# Nothing is installed system-wide.
param([switch]$SkipLlvm)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$shadCommit = 'e3ce810f3a653f43ac64ebab63023de281a4103a'   # tag v.0.18.0
$problems = @()

Write-Host '[1/3] shadPS4 v0.18.0 sources'
$shad = Join-Path $root 'engine/shadPS4'
if (-not (Test-Path -LiteralPath (Join-Path $shad '.git'))) {
    if (Test-Path -LiteralPath $shad) {
        throw "$shad exists but is not a git clone; rename or remove it, then run this script again"
    }
    New-Item -ItemType Directory -Force (Split-Path -Parent $shad) | Out-Null
    & git clone --no-checkout https://github.com/shadps4-emu/shadPS4.git $shad
    if ($LASTEXITCODE -ne 0) { throw 'git clone of shadPS4 failed' }
}
& git -C $shad fetch --tags origin
& git -C $shad checkout --detach $shadCommit
if ($LASTEXITCODE -ne 0) { throw "checkout of $shadCommit failed" }
& git -C $shad submodule update --init --recursive
if ($LASTEXITCODE -ne 0) { throw 'shadPS4 submodule update failed' }
$head = (& git -C $shad rev-parse HEAD).Trim()
if ($head -ne $shadCommit) { throw "engine/shadPS4 is at $head, expected $shadCommit (v.0.18.0)" }
Write-Host "      engine/shadPS4 at $head"

Write-Host '[2/3] LLVM 19.1.1'
if ($SkipLlvm) {
    if ($env:CTR_CLANG_CL -and (Test-Path -LiteralPath $env:CTR_CLANG_CL)) { Write-Host "      using CTR_CLANG_CL=$env:CTR_CLANG_CL" }
    else { $problems += 'LLVM skipped and CTR_CLANG_CL not set; ctrport/gpu/build.ps1 needs tools/toolchains/llvm-19.1.1' }
} else {
    & (Join-Path $root 'tools/toolchains/fetch_llvm.ps1')
}

Write-Host '[3/3] Other prerequisites'
# The build scripts currently use this fixed Visual Studio path.
$vs = 'C:/Program Files/Microsoft Visual Studio/2022/Community'
if (-not (Test-Path -LiteralPath "$vs/VC/Auxiliary/Build/vcvars64.bat")) {
    $problems += "Visual Studio 2022 Community not found at $vs (the build scripts expect this path)"
}
if (-not (Test-Path -LiteralPath "$vs/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe")) {
    $problems += 'CMake from Visual Studio 2022 not found (component "C++ CMake tools for Windows")'
}
$python = Get-Command python -ErrorAction SilentlyContinue
if (-not $python) { $problems += 'python not found in PATH (Python 3.12 is used by the build scripts)' }
else {
    $ver = (& python -c "import sys;print('%d.%d'%sys.version_info[:2])").Trim()
    if ($ver -ne '3.12') { Write-Host "      note: python $ver found, the scripts were written for 3.12" }
}

if ($problems.Count) {
    Write-Host ''
    Write-Host 'Missing prerequisites:'
    $problems | ForEach-Object { Write-Host "  - $_" }
    exit 1
}
Write-Host ''
Write-Host 'Bootstrap complete. Continue with the build steps in README.md.'
