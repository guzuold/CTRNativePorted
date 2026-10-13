# SPDX-License-Identifier: GPL-2.0-only
function Copy-CtrBuildLibrary {
 param([string]$Source,[string]$Destination)
 $taskSourcePath=[IO.Path]::GetFullPath($Source)
 $taskDestinationPath=[IO.Path]::GetFullPath($Destination)
 if($taskSourcePath -eq $taskDestinationPath){throw 'Build library source and destination must differ'}
 $taskSourceHash=(Get-FileHash -LiteralPath $taskSourcePath -Algorithm SHA256).Hash.ToLowerInvariant()
 Copy-Item -LiteralPath $taskSourcePath -Destination $taskDestinationPath -Force -ErrorAction Stop
 $taskCopyHash=(Get-FileHash -LiteralPath $taskDestinationPath -Algorithm SHA256).Hash.ToLowerInvariant()
 if($taskCopyHash -ne $taskSourceHash){throw 'Video library changed during snapshot; retry stable publication'}
 return @{path=$taskDestinationPath;sha256=$taskCopyHash;published_source=$taskSourcePath}
}
