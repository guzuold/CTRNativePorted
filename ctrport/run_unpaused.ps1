# SPDX-License-Identifier: GPL-2.0-only
$ErrorActionPreference='Stop'
Set-Location (Split-Path -Parent $PSScriptRoot)
$env:PYTHONPATH='.'
python -B ctrport/run_unpaused.py
