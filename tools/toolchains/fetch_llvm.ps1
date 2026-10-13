# SPDX-License-Identifier: GPL-2.0-only
# Downloads the official LLVM 19.1.1 Windows release and extracts it into
# tools/toolchains/llvm-19.1.1. The archive is accepted only if its size and SHA256
# match the values pinned in tools/toolchains/README.md.
param([switch]$Force)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$name   = 'clang+llvm-19.1.1-x86_64-pc-windows-msvc.tar.xz'
$url    = 'https://github.com/llvm/llvm-project/releases/download/llvmorg-19.1.1/clang%2Bllvm-19.1.1-x86_64-pc-windows-msvc.tar.xz'
$size   = 844457100
$sha256 = '621fc299fceb1bbdae927e355d1073034c9a1bbdda5a46a27e217c56af72f72a'

$here    = $PSScriptRoot
$target  = Join-Path $here 'llvm-19.1.1'
$clang   = Join-Path $target 'bin/clang-cl.exe'
$archive = Join-Path $here $name

if ((Test-Path -LiteralPath $clang) -and -not $Force) {
    Write-Host "LLVM 19.1.1 already present: $clang"
    exit 0
}

function Test-Archive([string]$path) {
    if (-not (Test-Path -LiteralPath $path)) { return $false }
    if ((Get-Item -LiteralPath $path).Length -ne $size) { return $false }
    return ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -eq $sha256)
}

if (-not (Test-Archive $archive)) {
    Write-Host "Downloading $name ($size bytes)..."
    $partial = "$archive.partial"
    if (Get-Command curl.exe -ErrorAction SilentlyContinue) {
        & curl.exe -L --fail --retry 3 -o $partial $url
        if ($LASTEXITCODE -ne 0) { throw "Download failed (curl exit $LASTEXITCODE)" }
    } else {
        Invoke-WebRequest -Uri $url -OutFile $partial -UseBasicParsing
    }
    Move-Item -LiteralPath $partial -Destination $archive -Force
    if (-not (Test-Archive $archive)) {
        throw "Archive size or SHA256 does not match the pinned value; file kept at $archive for inspection"
    }
}
Write-Host 'Archive verified (size and SHA256).'

if (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target -Recurse -Force }
New-Item -ItemType Directory -Force $target | Out-Null
& tar.exe -xf $archive -C $target --strip-components=1
if ($LASTEXITCODE -ne 0) { throw "Extraction failed (tar exit $LASTEXITCODE)" }
if (-not (Test-Path -LiteralPath $clang)) { throw "clang-cl.exe not found after extraction in $target" }

Remove-Item -LiteralPath $archive -Force
Write-Host "LLVM 19.1.1 ready: $clang"
