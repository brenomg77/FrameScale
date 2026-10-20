[CmdletBinding()]
param([Parameter(Mandatory = $true)][string]$NsisRoot)
$ErrorActionPreference = 'Stop'
& python (Join-Path $PSScriptRoot 'compact-ui.py') --nsis-root $NsisRoot
if ($LASTEXITCODE -ne 0) {throw 'Compact installer template generation failed.'}
