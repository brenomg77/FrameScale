[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Setup,
    [string]$WorkDir = 'build/installer-verification',
    [string]$Payload = ''
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$setupPath = (Resolve-Path -LiteralPath $Setup).Path
$payloadPath = if ($Payload) { (Resolve-Path -LiteralPath $Payload).Path } else { '' }
New-Item -ItemType Directory -Path $WorkDir -Force | Out-Null
$workRoot = (Resolve-Path -LiteralPath $WorkDir).Path
if (-not $workRoot.StartsWith($projectRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Installer verification must use a work directory inside the FrameScale source tree.'
}
$runRoot = Join-Path $workRoot ([Guid]::NewGuid().ToString('N'))
$target = Join-Path $runRoot 'installed'
New-Item -ItemType Directory -Path $runRoot | Out-Null
$registryKeys = @(
    'HKCU\Software\FrameScale\Installer',
    'HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\FrameScale'
)
$registryStates = @()
$shortcutDirectory = Join-Path ([Environment]::GetFolderPath('Programs')) 'FrameScale'
$shortcutDirectoryExisted = Test-Path -LiteralPath $shortcutDirectory -PathType Container
$shortcutPaths = @(
    (Join-Path $shortcutDirectory 'FrameScale.lnk'),
    (Join-Path $shortcutDirectory 'Uninstall.lnk'),
    (Join-Path ([Environment]::GetFolderPath('DesktopDirectory')) 'FrameScale.lnk')
)
$shortcutStates = @()
function Invoke-Registry([string[]]$Arguments) {
    # reg.exe can print success on stderr. Capture both pipes directly so
    # PowerShell's Stop preference cannot interrupt restoration on success.
    $info = New-Object System.Diagnostics.ProcessStartInfo
    $info.FileName = Join-Path $env:SystemRoot 'System32/reg.exe'
    $info.Arguments = (($Arguments | ForEach-Object { '"' + $_.Replace('"', '\"') + '"' }) -join ' ')
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $process = [Diagnostics.Process]::Start($info)
    try {
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        if ($process.ExitCode -ne 0) { throw "Registry command failed: $($stdout.Result) $($stderr.Result)" }
    } finally { $process.Dispose() }
}
function Test-RegistryKey([string]$Name) {
    $hive = [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::CurrentUser, [Microsoft.Win32.RegistryView]::Registry64)
    $key = $null
    try {
        $key = $hive.OpenSubKey($Name.Substring(5))
        return $null -ne $key
    } finally {
        if ($key) { $key.Dispose() }
        $hive.Dispose()
    }
}
# Complete the backup before invoking any installer that writes these entries.
foreach ($key in $registryKeys) {
    $exists = Test-RegistryKey $key
    $backup = Join-Path $runRoot "registry-$($registryStates.Count).reg"
    if ($exists) {
        Invoke-Registry @('export', $key, $backup, '/y', '/reg:64')
    }
    $registryStates += [pscustomobject]@{ key = $key; existed = $exists; backup = $backup }
}
foreach ($shortcut in $shortcutPaths) {
    $backup = Join-Path $runRoot "shortcut-$($shortcutStates.Count).lnk"
    $exists = Test-Path -LiteralPath $shortcut -PathType Leaf
    if ($exists) { Copy-Item -LiteralPath $shortcut -Destination $backup }
    $shortcutStates += [pscustomobject]@{ path = $shortcut; existed = $exists; backup = $backup }
}
function Invoke-Setup {
    $process = Start-Process -FilePath $setupPath -ArgumentList "/S /D=$target" -WindowStyle Hidden -PassThru -Wait
    if ($process.ExitCode -ne 0) { throw "Installer failed: $($process.ExitCode)" }
}
function Assert-Payload {
    if (-not $payloadPath) { return }
    foreach ($file in Get-ChildItem -LiteralPath $payloadPath -Recurse -File) {
        $relative = $file.FullName.Substring($payloadPath.Length + 1)
        $installed = Join-Path $target $relative
        if (-not (Test-Path -LiteralPath $installed -PathType Leaf) -or
            (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash -ne
            (Get-FileHash -LiteralPath $installed -Algorithm SHA256).Hash) {
            throw "Installed file differs from the payload: $relative"
        }
    }
}
try {
    Invoke-Setup
    Assert-Payload
    & (Join-Path $PSScriptRoot 'verify-runtime.ps1') -AppRoot (Join-Path $target 'bin')
    $recordPath = Join-Path $projectRoot ('build/release-records/' + [IO.Path]::GetFileName($setupPath) + '.build.json')
    if (-not (Test-Path -LiteralPath $recordPath)) { $recordPath = $setupPath + '.build.json' }
    if (-not (Test-Path -LiteralPath $recordPath)) { throw 'Setup build record is missing.' }
    $record = Get-Content -LiteralPath $recordPath -Raw | ConvertFrom-Json
    if ((Get-FileHash -LiteralPath $setupPath -Algorithm SHA256).Hash -ne $record.setupSha256) { throw 'Setup hash does not match build record.' }
    $installedExe = Join-Path $target 'bin/FrameScale.exe'
    if ((Get-FileHash -LiteralPath $installedExe -Algorithm SHA256).Hash -ne $record.executableSha256) { throw 'Installed executable differs from the recorded build.' }
    $sentinel = Join-Path $target 'user-file-preserved.txt'
    Set-Content -LiteralPath $sentinel -Value 'FrameScale installer preservation test' -Encoding UTF8
    $lock = [IO.File]::Open($installedExe, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::None)
    try {
        $blocked = Start-Process -FilePath $setupPath -ArgumentList "/S /D=$target" -WindowStyle Hidden -PassThru -Wait
        if ($blocked.ExitCode -eq 0) { throw 'Installer did not reject a locked application executable.' }
    } finally { $lock.Dispose() }
    Invoke-Setup
    Assert-Payload
    & (Join-Path $PSScriptRoot 'verify-runtime.ps1') -AppRoot (Join-Path $target 'bin')
    if (-not (Test-Path -LiteralPath $sentinel)) { throw 'Upgrade removed a user file.' }
    # NSIS _?= suppresses the temporary self-copy, so -Wait observes completion.
    $uninstaller = Join-Path $target 'Uninstall.exe'
    $uninstall = Start-Process -FilePath $uninstaller -ArgumentList "/S _?=$target" -WindowStyle Hidden -PassThru -Wait
    if ($uninstall.ExitCode -ne 0) { throw "Uninstall failed: $($uninstall.ExitCode)" }
    if (Test-Path -LiteralPath $installedExe) { throw 'Uninstall left the application executable.' }
    if (-not (Test-Path -LiteralPath $sentinel)) { throw 'Uninstall removed a user file.' }
    $result = [ordered]@{ setup = $setupPath; target = $target; install = 'passed'; payloadHashesCompared = [bool]$payloadPath; lockedExecutableRejected = $true; upgrade = 'passed'; uninstall = 'passed'; userFilePreserved = $true }
    $result | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $runRoot 'result.json') -Encoding UTF8
    Write-Output "Installer verification passed; evidence: $runRoot"
} finally {
    foreach ($state in $registryStates) {
        if (Test-RegistryKey $state.key) {
            Invoke-Registry @('delete', $state.key, '/f', '/reg:64')
        }
        if ($state.existed) {
            Invoke-Registry @('import', $state.backup, '/reg:64')
        }
    }
    foreach ($state in $shortcutStates) {
        if ($state.existed) {
            New-Item -ItemType Directory -Path (Split-Path -Parent $state.path) -Force | Out-Null
            Copy-Item -LiteralPath $state.backup -Destination $state.path -Force
        } elseif (Test-Path -LiteralPath $state.path) {
            Remove-Item -LiteralPath $state.path -Force
        }
    }
    if (-not $shortcutDirectoryExisted -and (Test-Path -LiteralPath $shortcutDirectory)) {
        if (@(Get-ChildItem -LiteralPath $shortcutDirectory -Force).Count -eq 0) { Remove-Item -LiteralPath $shortcutDirectory }
    }
}
Write-Output 'Original registry entries and shortcuts restored.'
