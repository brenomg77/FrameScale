[CmdletBinding()]
param(
    # Use the source root, or the installed application's bin directory.
    [string]$AppRoot = (Join-Path $PSScriptRoot '..'),
    [string]$ManifestPath = ''
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = (Resolve-Path -LiteralPath $AppRoot).Path.TrimEnd('\', '/')
if (-not $ManifestPath) { $ManifestPath = Join-Path $root 'runtime/manifest.json' }
$manifest = Get-Content -LiteralPath $ManifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($manifest.schemaVersion -ne 2) { throw 'Unsupported runtime manifest schema (expected 2).' }
if (@($manifest.artifacts).Count -eq 0) { throw 'The runtime inventory is empty.' }
$expected = @{}
foreach ($artifact in $manifest.artifacts) {
    $relative = [string]$artifact.path
    if ($relative -notmatch '^(runtime/bin|models)/[^:]+$' -or $relative -match '(^|/)\.\.?(/|$)' -or $relative.Contains('\')) {
        throw "Invalid inventory path: $relative"
    }
    if ($expected.ContainsKey($relative)) { throw "Duplicate inventory path: $relative" }
    $expected[$relative] = $true
    if ($artifact.sha256 -notmatch '^[0-9a-fA-F]{64}$' -or [long]$artifact.size -lt 1) {
        throw "Invalid integrity metadata: $relative"
    }
    $fullPath = [IO.Path]::GetFullPath((Join-Path $root $relative))
    if (-not $fullPath.StartsWith($root + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Inventory path escapes the application directory: $relative"
    }
    if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) { throw "Missing runtime artifact: $relative" }
    $item = Get-Item -LiteralPath $fullPath -Force
    $ancestor = $item
    while ($ancestor -and $ancestor.FullName -ne $root) {
        if ($ancestor.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Linked runtime artifact is not allowed: $relative" }
        if ($ancestor -is [IO.FileInfo]) { $ancestor = $ancestor.Directory } else { $ancestor = $ancestor.Parent }
    }
    if ($item.Length -ne [long]$artifact.size) { throw "Runtime size mismatch: $relative" }
    if ((Get-FileHash -LiteralPath $fullPath -Algorithm SHA256).Hash -ne $artifact.sha256) {
        throw "Runtime SHA-256 mismatch: $relative"
    }
}
foreach ($directory in @('runtime/bin', 'models')) {
    $directoryPath = Join-Path $root $directory
    if (-not (Test-Path -LiteralPath $directoryPath -PathType Container)) { throw "Missing runtime directory: $directory" }
    foreach ($file in Get-ChildItem -LiteralPath $directoryPath -File -Recurse -Force) {
        $relative = $file.FullName.Substring($root.Length + 1).Replace('\', '/')
        if (-not $expected.ContainsKey($relative)) { throw "Unlisted runtime artifact: $relative" }
    }
}
foreach ($license in $manifest.licenseFiles) {
    if ($license -notmatch '^LICENSE-[A-Za-z0-9.-]+\.txt$') { throw "Invalid license filename: $license" }
    $sourceLicense = Join-Path $root "runtime/$license"
    $installedLicense = Join-Path $root "runtime/licenses/$license"
    if (-not (Test-Path -LiteralPath $sourceLicense -PathType Leaf) -and -not (Test-Path -LiteralPath $installedLicense -PathType Leaf)) {
        throw "Missing license text: $license"
    }
}
Write-Output "Runtime integrity verified: $($expected.Count) artifacts and $(@($manifest.licenseFiles).Count) license texts in $root"
