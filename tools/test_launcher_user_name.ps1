# SPDX-License-Identifier: GPL-2.0-only
param([string]$Runtime = "$PSScriptRoot/../build/CTRPort-dev/ctrport.exe")
$ErrorActionPreference='Stop'
$previousName=$env:CTR_ONLINE_ID
$previousOnline=$env:CTR_ONLINE
try {
    foreach($online in @('0','1')) {
        $env:CTR_ONLINE=$online
        foreach($case in @(
            @{Name='';Expected='CTRPlayer'},
            @{Name='guzuold';Expected='guzuold'},
            @{Name='Racer_012345678-';Expected='Racer_012345678-'},
            @{Name='ab';Expected='CTRPlayer'},
            @{Name='Name With Space';Expected='CTRPlayer'},
            @{Name='12345678901234567';Expected='CTRPlayer'}
        )) {
            $env:CTR_ONLINE_ID=$case.Name
            & $Runtime --user-name-control $case.Expected
            if($LASTEXITCODE -ne 0){throw "User name check failed (online=$online, name=$($case.Name), exit=$LASTEXITCODE)"}
        }
    }
    Write-Output 'PASS: 12 user-name cases; direct service and guest ABI, short buffer and canaries'
} finally {
    $env:CTR_ONLINE_ID=$previousName
    $env:CTR_ONLINE=$previousOnline
}

