$vc='C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat'
$lines=cmd /c "`"$vc`" >nul && set"
foreach($line in $lines){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process')}}
& (Join-Path $env:VCToolsInstallDir 'bin/HostX64/x64/cl.exe') /nologo /W4 /WX /EHsc /std:c++17 ctrport/trace_exit.cpp /Fobuild/CTRPort-infra/trace_exit.obj /Febuild/CTRPort-infra/trace_exit.exe *> build/CTRPort-infra/trace_build.log
if($LASTEXITCODE -ne 0){Get-Content build/CTRPort-infra/trace_build.log;throw 'trace build failed'}
