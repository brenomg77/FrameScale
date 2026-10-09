[CmdletBinding()]
param(
    [string]$BuildDir = 'build/Release',
    # A unique payload directory is created below this directory on every run.
    [string]$PackageDir = 'build/installer',
    [string]$MakeNsis = 'build/setup-tools/nsis-3.11/makensis.exe',
    [string]$CMake = 'C:/Qt/Tools/CMake_64/bin/cmake.exe',
    [switch]$KeepStaging
)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$payload = $null
$stagingRoot = $null
$runId = [Guid]::NewGuid().ToString('N')
Push-Location $projectRoot
try {
    if (-not (Test-Path -LiteralPath $MakeNsis)) { throw 'NSIS compiler not found. Supply -MakeNsis with its path.' }
    $compiler = (Resolve-Path -LiteralPath $MakeNsis).Path
    $versionMatch = [regex]::Match((Get-Content CMakeLists.txt -Raw), 'project\(FrameScale VERSION ([0-9]+\.[0-9]+\.[0-9]+)')
    if (-not $versionMatch.Success) { throw 'CMake project version not found' }
    $version = $versionMatch.Groups[1].Value
    $buildPath = (Resolve-Path -LiteralPath $BuildDir).Path
    $cache = Get-Content -LiteralPath (Join-Path $buildPath 'CMakeCache.txt') -Raw
    if ($cache -notmatch 'CMAKE_BUILD_TYPE:STRING=Release') { throw 'Installer must use a Release build' }
    $homeMatch = [regex]::Match($cache, '(?m)^CMAKE_HOME_DIRECTORY:INTERNAL=(.+)\r?$')
    if (-not $homeMatch.Success -or [IO.Path]::GetFullPath($homeMatch.Groups[1].Value.Trim()) -ne $projectRoot) {
        throw 'The build directory belongs to another source tree.'
    }
    & (Join-Path $PSScriptRoot 'verify-runtime.ps1') -AppRoot $projectRoot
    & $CMake --build $buildPath --target FrameScale --parallel 3
    if ($LASTEXITCODE -ne 0) { throw 'Release build failed' }
    New-Item -ItemType Directory -Path $PackageDir -Force | Out-Null
    $stagingRoot = (Resolve-Path -LiteralPath $PackageDir).Path
    $payload = Join-Path $stagingRoot "payload-$runId"
    New-Item -ItemType Directory -Path $payload | Out-Null
    # Windows PowerShell treats native stderr warnings as errors when redirected.
    $ErrorActionPreference = 'Continue'
    & $CMake --install $buildPath --prefix $payload
    $deployExit = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    if ($deployExit -ne 0) { throw 'Qt deployment failed' }
    foreach ($required in @('bin/FrameScale.exe','bin/runtime/bin/yt-dlp.exe','bin/runtime/bin/deno.exe','bin/Qt6Core.dll','bin/qt.conf','plugins/platforms/qwindows.dll','bin/runtime/bin/ffmpeg.exe','bin/runtime/bin/ffprobe.exe','bin/runtime/bin/realesrgan-ncnn-vulkan.exe','bin/runtime/bin/realcugan-ncnn-vulkan.exe','bin/runtime/bin/rife-ncnn-vulkan.exe','bin/runtime/bin/mpv.exe','bin/runtime/manifest.json','bin/models')) {
        if (-not (Test-Path -LiteralPath (Join-Path $payload $required))) { throw "Package is incomplete: $required" }
    }
    & (Join-Path $PSScriptRoot 'verify-runtime.ps1') -AppRoot (Join-Path $payload 'bin')
    $builtExe = Join-Path $buildPath 'FrameScale.exe'
    if ((Get-FileHash -LiteralPath $builtExe -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath (Join-Path $payload 'bin/FrameScale.exe') -Algorithm SHA256).Hash) {
        throw 'The deployed executable does not match the current Release build.'
    }
    New-Item -ItemType Directory -Force dist,build/installer | Out-Null
    $files = @(Get-ChildItem -LiteralPath $payload -Recurse -File)
    $invalid = @($files | Where-Object { $_.Extension -in @('.pdb','.obj','.zip','.7z') -or $_.FullName.Substring($payload.Length) -match '\\(downloads|packages)\\' })
    if ($invalid.Count) { throw 'Package contains build files or downloaded archives; use a clean PackageDir' }
    $deleteLines = @($files | ForEach-Object { 'Delete "$INSTDIR\' + $_.FullName.Substring($payload.Length + 1).Replace('$','$$') + '"' })
    $deleteLines += Get-ChildItem -LiteralPath $payload -Recurse -Directory | Sort-Object { $_.FullName.Length } -Descending | ForEach-Object { 'RMDir "$INSTDIR\' + $_.FullName.Substring($payload.Length + 1).Replace('$','$$') + '"' }
    $manifest = Join-Path $stagingRoot "uninstall-files-$runId.nsh"
    [IO.File]::WriteAllLines($manifest, $deleteLines, [Text.UTF8Encoding]::new($true))
    $outputFile = Join-Path $projectRoot "dist/FrameScale-$version-Windows-x64-Setup.exe"
    $recordsDir = Join-Path $projectRoot 'build/release-records'
    New-Item -ItemType Directory -Path $recordsDir -Force | Out-Null
    $recordBase = Join-Path $recordsDir ([IO.Path]::GetFileName($outputFile))
    $pendingOutput = Join-Path $stagingRoot "FrameScale-$version-Windows-x64-Setup-$runId.pending.exe"
    $sourceFiles = @(
        Get-ChildItem -LiteralPath src,tests,assets,scripts,packaging -Recurse -File
        Get-Item -LiteralPath CMakeLists.txt,README.md,THIRD_PARTY_NOTICES.md,runtime/manifest.json,runtime/README.md
        Get-ChildItem -Path runtime/LICENSE-*.txt -File
    ) | Where-Object { $_.Extension -notin @('.pyc', '.pyo') -and $_.FullName -notmatch '\\__pycache__\\' } | Sort-Object FullName -Unique
    $sourceInventory = @($sourceFiles | ForEach-Object {
        [ordered]@{
            path = $_.FullName.Substring($projectRoot.Length + 1).Replace('\','/')
            size = $_.Length
            sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    })
    $kib = [math]::Ceiling(($files | Measure-Object Length -Sum).Sum / 1KB)
    & $compiler "/INPUTCHARSET" "UTF8" "/DAPP_VERSION=$version" "/DPAYLOAD_DIR=$payload" "/DOUTPUT_FILE=$pendingOutput" "/DUNINSTALL_FILES=$manifest" "/DPAYLOAD_KIB=$kib" 'packaging/windows/FrameScale.nsi'
    if ($LASTEXITCODE -ne 0) { throw 'Installer compilation failed' }
    # Publish locally only after compilation succeeds. Preserve the previous setup.
    $previousFiles = @($outputFile, ($recordBase + '.sha256'), ($recordBase + '.build.json'), ($recordBase + '.validation.json')) |
        Where-Object { Test-Path -LiteralPath $_ }
    if ($previousFiles) {
        $archiveDir = Join-Path $recordsDir "archive/$runId"
        New-Item -ItemType Directory -Path $archiveDir | Out-Null
        foreach ($previousFile in $previousFiles) {
            if (Test-Path -LiteralPath $previousFile) {
                Move-Item -LiteralPath $previousFile -Destination $archiveDir
            }
        }
    }
    Move-Item -LiteralPath $pendingOutput -Destination $outputFile
    $hash = (Get-FileHash -LiteralPath $outputFile -Algorithm SHA256).Hash.ToLowerInvariant()
    [IO.File]::WriteAllText($recordBase + '.sha256', "$hash  $([IO.Path]::GetFileName($outputFile))`n")
    $record = [ordered]@{
        schemaVersion = 1
        generatedUtc = [DateTime]::UtcNow.ToString('o')
        appVersion = $version
        buildDirectory = $buildPath
        setupFile = [IO.Path]::GetFileName($outputFile)
        setupSha256 = $hash
        executableSha256 = (Get-FileHash -LiteralPath $builtExe -Algorithm SHA256).Hash.ToLowerInvariant()
        payloadFileCount = $files.Count
        runtimeManifestSha256 = (Get-FileHash -LiteralPath (Join-Path $payload 'bin/runtime/manifest.json') -Algorithm SHA256).Hash.ToLowerInvariant()
        sourceInventory = $sourceInventory
        verification = 'Runtime/model hashes, license presence and deployed executable equality passed. Tests are run separately; this record is not a test or license certification.'
    }
    [IO.File]::WriteAllText($recordBase + '.build.json', ($record | ConvertTo-Json -Depth 7), [Text.UTF8Encoding]::new($false))
    Get-Item -LiteralPath $outputFile | Select-Object FullName,Length
    if ($KeepStaging) { Write-Output "Verified payload retained at: $payload" }
} finally {
    # Delete only the fresh staging directory made by this invocation, never the
    # caller's PackageDir or a payload from a different build.
    if ($payload -and -not $KeepStaging -and (Test-Path -LiteralPath $payload)) {
        $resolvedPayload = (Resolve-Path -LiteralPath $payload).Path
        $expectedPayload = [IO.Path]::GetFullPath((Join-Path $stagingRoot "payload-$runId"))
        if ($resolvedPayload -ne $expectedPayload -or -not $resolvedPayload.StartsWith($stagingRoot.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Refusing to remove a staging directory outside the expected location.'
        }
        Remove-Item -LiteralPath $resolvedPayload -Recurse -Force
    }
    Pop-Location
}
