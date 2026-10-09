#requires -Version 5.1
<#
.SYNOPSIS
Validates bundled engines with generated fixtures; retains outputs, logs and summary.json.
.EXAMPLE
.\tests\validate-media-pipelines.ps1 -BuildDir .\build\Release
#>
[CmdletBinding()]
param(
    [string]$BuildDir = (Join-Path $PSScriptRoot '..\build\Release'),
    [string]$QtBin,
    [ValidateRange(10, 3600)][int]$TimeoutSeconds = 180
)
$ErrorActionPreference = 'Stop'
$BuildDir = (Resolve-Path -LiteralPath $BuildDir).Path
$runDir = Join-Path $BuildDir ('media-validation\' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N'))
$null = New-Item -ItemType Directory -Path $runDir
$utf8 = New-Object System.Text.UTF8Encoding($false)
$results = New-Object System.Collections.Generic.List[object]
$childPath = $env:PATH
$started = [DateTime]::UtcNow
$setupError = $null
$script:abortMatrix = $false
function Assert-That([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}
# ProcessStartInfo.Arguments uses Windows CRT quoting, not PowerShell quoting.
function Quote-Argument([string]$Value) {
    '"' + [regex]::Replace([regex]::Replace($Value, '(\\*)"', '$1$1\"'), '(\\+)$', '$1$1') + '"'
}
function Invoke-Tool([string]$Executable, [string[]]$Arguments, [string]$LogName, [bool]$RequireEmptyStderr = $false) {
    $info = New-Object System.Diagnostics.ProcessStartInfo
    $info.FileName = $Executable
    $info.Arguments = (($Arguments | ForEach-Object { Quote-Argument $_ }) -join ' ')
    $info.WorkingDirectory = $runDir
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.WindowStyle = [System.Diagnostics.ProcessWindowStyle]::Hidden
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.EnvironmentVariables['PATH'] = $childPath
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $info
    $stdout = $null
    $stderr = $null
    try {
        Assert-That ($process.Start()) "Could not start $Executable"
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
            # Stop the harness and its engine descendants before another GPU case.
            $killInfo = New-Object System.Diagnostics.ProcessStartInfo
            $killInfo.FileName = Join-Path $env:SystemRoot 'System32\taskkill.exe'
            $killInfo.Arguments = "/PID $($process.Id) /T /F"
            $killInfo.UseShellExecute = $false
            $killInfo.CreateNoWindow = $true
            $killInfo.WindowStyle = [System.Diagnostics.ProcessWindowStyle]::Hidden
            $script:abortMatrix = $true
            $killer = [System.Diagnostics.Process]::Start($killInfo)
            try {
                Assert-That ($killer.WaitForExit(10000) -and $killer.ExitCode -eq 0) "Could not stop process tree: $LogName"
            } finally { $killer.Dispose() }
            Assert-That ($process.WaitForExit(10000)) "Process remains active: $LogName"
            $script:abortMatrix = $false
            throw "Process timed out after $TimeoutSeconds seconds: $LogName"
        }
        Assert-That ($stdout.Wait(10000) -and $stderr.Wait(10000)) "Output capture timed out: $LogName"
        Assert-That ($process.ExitCode -eq 0) "Exit $($process.ExitCode): $LogName (see $LogName.log)"
        if ($RequireEmptyStderr) { Assert-That ([string]::IsNullOrWhiteSpace($stderr.Result)) "Decode reported errors: $LogName (see $LogName.log)" }
        return $stdout.Result
    } finally {
        $outText = if ($null -ne $stdout -and $stdout.IsCompleted) { $stdout.Result } else { '[stdout incomplete]' }
        $errText = if ($null -ne $stderr -and $stderr.IsCompleted) { $stderr.Result } else { '[stderr incomplete]' }
        [IO.File]::WriteAllText((Join-Path $runDir "$LogName.log"),
            "$Executable $($info.Arguments)" + [Environment]::NewLine + $outText + [Environment]::NewLine + $errText, $utf8)
        $process.Dispose()
    }
}
function Read-Probe([string]$Path, [string]$Name) {
    $json = Invoke-Tool $ffprobe @('-v', 'error', '-count_frames', '-show_streams', '-show_format', '-of', 'json', $Path) $Name
    return ($json | ConvertFrom-Json)
}
function Number([string]$Value) {
    return [double]::Parse($Value, [Globalization.CultureInfo]::InvariantCulture)
}
function Rate([string]$Value) {
    $parts = $Value.Split('/')
    if ($parts.Count -eq 1) { return (Number $Value) }
    return (Number $parts[0]) / (Number $parts[1])
}
$cases = New-Object System.Collections.Generic.List[object]
function Add-Case([string]$Name, [string]$InputFile, [string]$Media, [string]$Operation,
                  [string]$Engine, [string]$Model, [int]$Scale = 2, [string]$Extension = 'mp4',
                  [bool]$AacFallback = $false) {
    $cases.Add([pscustomobject]@{ Name = $Name; Input = $InputFile; Media = $Media; Operation = $Operation
        Engine = $Engine; Model = $Model; Scale = $Scale; Extension = $Extension; AacFallback = $AacFallback })
}
try {
    $cachePath = Join-Path $BuildDir 'CMakeCache.txt'
    $cache = if (Test-Path -LiteralPath $cachePath) { Get-Content -LiteralPath $cachePath } else { @() }
    if (-not $QtBin) {
        $qtEntry = $cache | Where-Object { $_ -match '^Qt6_DIR:[^=]+=' } | Select-Object -First 1
        if ($qtEntry) { $QtBin = [IO.Path]::GetFullPath((Join-Path ($qtEntry -split '=', 2)[1] '..\..\..\bin')) }
    }
    if ($QtBin) {
        Assert-That (Test-Path -LiteralPath $QtBin -PathType Container) "QtBin does not exist: $QtBin"
        $childPath = (Resolve-Path -LiteralPath $QtBin).Path + ';' + $childPath
    }
    $compilerEntry = $cache | Where-Object { $_ -match '^CMAKE_CXX_COMPILER:[^=]+=' } | Select-Object -First 1
    if ($compilerEntry) { $childPath = (Split-Path ($compilerEntry -split '=', 2)[1]) + ';' + $childPath }
    $smoke = Join-Path $BuildDir 'FrameScaleProcessingSmoke.exe'
    $ffmpeg = Join-Path $BuildDir 'runtime\bin\ffmpeg.exe'
    $ffprobe = Join-Path $BuildDir 'runtime\bin\ffprobe.exe'
    foreach ($tool in @($smoke, $ffmpeg, $ffprobe)) {
        Assert-That (Test-Path -LiteralPath $tool -PathType Leaf) "Build the project first; missing $tool"
    }
    $inputImage = Join-Path $runDir 'input.png'
    $inputVideo = Join-Path $runDir 'input-two-audio.mp4'
    $inputWma = Join-Path $runDir 'input-wma.mkv'
    $inputVfr = Join-Path $runDir 'input-vfr.mp4'
    $inputRotated = Join-Path $runDir 'input-rotated.mp4'
    $null = Invoke-Tool $ffmpeg @('-y', '-v', 'error', '-f', 'lavfi', '-i', 'testsrc=size=32x24:rate=1', '-frames:v', '1', $inputImage) 'fixture-image'
    $null = Invoke-Tool $ffmpeg @('-y', '-v', 'error', '-f', 'lavfi', '-i', 'testsrc2=size=32x24:rate=4:duration=1',
        '-f', 'lavfi', '-i', 'sine=frequency=440:duration=1', '-f', 'lavfi', '-i', 'sine=frequency=880:duration=1',
        '-map', '0:v', '-map', '1:a', '-map', '2:a', '-c:v', 'libx264', '-pix_fmt', 'yuv420p', '-c:a', 'aac', $inputVideo) 'fixture-video'
    $null = Invoke-Tool $ffmpeg @('-y', '-v', 'error', '-i', $inputVideo, '-map', '0', '-c:v', 'copy', '-c:a', 'wmav2', $inputWma) 'fixture-wma'
    $null = Invoke-Tool $ffmpeg @('-y', '-v', 'error', '-display_rotation:v:0', '90', '-i', $inputVideo, '-map', '0', '-c', 'copy', $inputRotated) 'fixture-rotation'
    foreach ($color in @('red', 'green', 'blue')) {
        $null = Invoke-Tool $ffmpeg @('-y', '-v', 'error', '-f', 'lavfi', '-i', "color=c=$($color):size=32x24", '-frames:v', '1',
            (Join-Path $runDir "$color.png")) "fixture-$color"
    }
    $concat = Join-Path $runDir 'vfr.ffconcat'
    [IO.File]::WriteAllLines($concat, @('ffconcat version 1.0', 'file red.png', 'duration 0.12', 'file green.png',
        'duration 0.40', 'file blue.png', 'duration 0.48', 'file blue.png'), $utf8)
    $null = Invoke-Tool $ffmpeg @('-y', '-v', 'error', '-f', 'concat', '-safe', '0', '-i', $concat,
        '-fps_mode', 'vfr', '-c:v', 'libx264', '-bf', '0', '-pix_fmt', 'yuv420p', $inputVfr) 'fixture-vfr'
    $vfrTiming = (Invoke-Tool $ffprobe @('-v', 'error', '-select_streams', 'v:0', '-show_frames',
        '-show_entries', 'frame=best_effort_timestamp_time,duration_time', '-of', 'json', $inputVfr) 'fixture-vfr-timing') | ConvertFrom-Json
    $timestamps = @($vfrTiming.frames | ForEach-Object { Number $_.best_effort_timestamp_time })
    $intervals = @(for ($i = 1; $i -lt $timestamps.Count; $i++) { [Math]::Round($timestamps[$i] - $timestamps[$i - 1], 5) })
    Assert-That (@($intervals | Select-Object -Unique).Count -gt 1) 'VFR fixture has no varying frame intervals'
    $vfrProbe = Read-Probe $inputVfr 'fixture-vfr-probe'
    $vfrStream = @($vfrProbe.streams | Where-Object { $_.codec_type -eq 'video' })[0]
    $lastFrameDuration = Number $vfrTiming.frames[-1].duration_time
    $presentationDuration = $timestamps[-1] + $lastFrameDuration - $timestamps[0]
    Assert-That ($lastFrameDuration -gt 0 -and [Math]::Abs((Number $vfrStream.duration) - $presentationDuration) -lt 0.00001) 'VFR stream duration contradicts its presentation timestamps'
    Assert-That ([Math]::Abs((Number $vfrProbe.format.duration) - $presentationDuration) -lt 0.00001) 'VFR container duration contradicts its presentation timestamps'
    $rotationProbe = Read-Probe $inputRotated 'fixture-rotation-probe'
    $rotation = @($rotationProbe.streams | Where-Object { $_.codec_type -eq 'video' })[0]
    Assert-That (@($rotation.side_data_list | Where-Object { [Math]::Abs([int]$_.rotation) -eq 90 }).Count -gt 0) 'Rotation fixture has no 90 degree display matrix'
    foreach ($fixture in @(@($inputVideo, 'aac'), @($inputWma, 'wmav2'))) {
        $fixtureProbe = Read-Probe $fixture[0] ('fixture-audio-' + $fixture[1])
        $fixtureAudio = @($fixtureProbe.streams | Where-Object { $_.codec_type -eq 'audio' })
        Assert-That ($fixtureAudio.Count -eq 2) 'Expected two audio streams in the fixture'
        foreach ($stream in $fixtureAudio) {
            Assert-That ($stream.codec_name -eq $fixture[1] -and [int]$stream.nb_read_frames -gt 0) 'Incorrect or empty fixture audio'
        }
    }
    $engines = @(@('esrgan', 'realesr-animevideov3'), @('cugan', 'realcugan-se'), @('anime4k', 'anime4k-v4-a'))
    foreach ($engine in $engines) {
        Add-Case "image-$($engine[0])" $inputImage image upscale $engine[0] $engine[1] 2 png
        Add-Case "video-$($engine[0])" $inputVideo video upscale $engine[0] $engine[1]
        Add-Case "both-$($engine[0])" $inputVideo video both $engine[0] $engine[1]
    }
    Add-Case 'rife-only' $inputVideo video interpolate esrgan realesr-animevideov3
    foreach ($extension in @('jpg', 'bmp', 'webp')) {
        $formatInput = Join-Path $runDir "input.$extension"
        $null = Invoke-Tool $ffmpeg @('-y', '-v', 'error', '-i', $inputImage, '-frames:v', '1', $formatInput) "fixture-$extension"
        Add-Case "format-$extension" $formatInput image upscale esrgan realesr-animevideov3 2 $extension
    }
    Add-Case 'esrgan-3x' $inputImage image upscale esrgan realesr-animevideov3 3 png
    Add-Case 'cugan-4x' $inputImage image upscale cugan realcugan-se 4 png
    Add-Case 'anime4k-4x' $inputImage image upscale anime4k anime4k-v4-a+a 4 png
    Add-Case 'vfr-rife' $inputVfr video interpolate esrgan realesr-animevideov3
    Add-Case 'vfr-upscale' $inputVfr video upscale esrgan realesr-animevideov3
    Add-Case 'wma-aac-fallback' $inputWma video upscale esrgan realesr-animevideov3 2 mp4 $true
    Add-Case 'rotated-video' $inputRotated video upscale esrgan realesr-animevideov3
    foreach ($case in $cases) {
        $watch = [Diagnostics.Stopwatch]::StartNew()
        $output = Join-Path $runDir ($case.Name + '.' + $case.Extension)
        $failure = $null
        $observed = $null
        Write-Host "Validating $($case.Name)..."
        try {
            $source = Read-Probe $case.Input ($case.Name + '-source')
            $sourceVideo = @($source.streams | Where-Object { $_.codec_type -eq 'video' })[0]
            $expectedWidth = [int]$sourceVideo.width
            $expectedHeight = [int]$sourceVideo.height
            if (@($sourceVideo.side_data_list | Where-Object { [Math]::Abs([int]$_.rotation) % 180 -eq 90 }).Count) {
                $expectedWidth, $expectedHeight = $expectedHeight, $expectedWidth
            }
            if ($case.Operation -ne 'interpolate') { $expectedWidth *= $case.Scale; $expectedHeight *= $case.Scale }
            $null = Invoke-Tool $smoke @($case.Input, $output, $case.Media, $case.Operation, $case.Engine, $case.Model,
                [string]$case.Scale, '8', '0') $case.Name
            $observed = Read-Probe $output ($case.Name + '-probe')
            $video = @($observed.streams | Where-Object { $_.codec_type -eq 'video' })
            $audio = @($observed.streams | Where-Object { $_.codec_type -eq 'audio' })
            Assert-That ($video.Count -eq 1) 'Expected exactly one video/image stream'
            Assert-That ($video[0].width -eq $expectedWidth -and $video[0].height -eq $expectedHeight) "Unexpected dimensions; expected $($expectedWidth)x$expectedHeight"
            if ($case.Media -eq 'video') {
                # WMA remuxing extends the audio/container by encoder delay;
                # its visual fixture remains exactly four frames in one second.
                $durationText = if ($case.Name -eq 'wma-aac-fallback') { '1' } elseif ($sourceVideo.duration -and $sourceVideo.duration -ne 'N/A') { $sourceVideo.duration } else { $source.format.duration }
                $duration = Number $durationText
                $expectedFrames = [int]$sourceVideo.nb_read_frames
                $expectedFps = $expectedFrames / $duration
                if ($case.Operation -ne 'upscale') { $expectedFrames = [int][Math]::Ceiling($duration * 8 - 1e-9); $expectedFps = 8 }
                Assert-That ($observed.format.format_name.Split(',') -contains 'mp4') 'Expected MP4 container'
                Assert-That ($video[0].codec_name -eq 'h264' -and $video[0].pix_fmt -eq 'yuv420p') 'Expected H.264/yuv420p'
                Assert-That ([int]$video[0].nb_read_frames -eq $expectedFrames) "Unexpected frame count; expected $expectedFrames"
                Assert-That ([Math]::Abs((Rate $video[0].avg_frame_rate) - $expectedFps) -lt 0.0001) "Unexpected FPS; expected $expectedFps"
                Assert-That ([Math]::Abs((Number $video[0].duration) - $expectedFrames / $expectedFps) -lt 0.05) 'Unexpected output duration'
                $sourceAudio = @($source.streams | Where-Object { $_.codec_type -eq 'audio' })
                Assert-That ($audio.Count -eq $sourceAudio.Count) 'Audio stream count changed'
                for ($i = 0; $i -lt $audio.Count; $i++) {
                    $expectedCodec = if ($case.AacFallback) { 'aac' } else { $sourceAudio[$i].codec_name }
                    Assert-That ($audio[$i].codec_name -eq $expectedCodec) "Unexpected audio codec at stream $i"
                    Assert-That ([int]$audio[$i].nb_read_frames -gt 0) "Empty audio stream $i"
                }
                Assert-That (@($video[0].side_data_list | Where-Object { [int]$_.rotation % 360 -ne 0 }).Count -eq 0) 'Output retains rotation metadata'
                Assert-That (@($observed.streams).Count -eq 1 + $audio.Count) 'Unexpected extra output streams'
            } else {
                $codec = @{ png = 'png'; jpg = 'mjpeg'; bmp = 'bmp'; webp = 'webp' }[$case.Extension]
                Assert-That ($video[0].codec_name -eq $codec) "Unexpected image codec; expected $codec"
            }
            $decode = Invoke-Tool $ffmpeg @('-v', 'error', '-xerror', '-err_detect', 'explode', '-i', $output,
                '-map', '0:v:0', '-map', '0:a?', '-f', 'null', '-') ($case.Name + '-decode') $true
            Assert-That ([string]::IsNullOrWhiteSpace($decode)) 'Decode produced unexpected stdout'
            if ($case.Name -eq 'vfr-upscale') {
                # Compare sampled color order, so correct metadata alone cannot hide lost VFR timing.
                $expectedColors = Join-Path $runDir 'vfr-expected.rgb'
                $actualColors = Join-Path $runDir 'vfr-actual.rgb'
                $rateText = $expectedFps.ToString('F9', [Globalization.CultureInfo]::InvariantCulture)
                $null = Invoke-Tool $ffmpeg @('-y', '-v', 'error', '-i', $case.Input, '-vf',
                    "setpts=PTS-STARTPTS,fps=$rateText,scale=1:1", '-frames:v', [string]$expectedFrames,
                    '-pix_fmt', 'rgb24', '-f', 'rawvideo', $expectedColors) 'vfr-reference-colors'
                $null = Invoke-Tool $ffmpeg @('-y', '-v', 'error', '-i', $output, '-vf', 'scale=1:1',
                    '-pix_fmt', 'rgb24', '-f', 'rawvideo', $actualColors) 'vfr-output-colors'
                $reference = [IO.File]::ReadAllBytes($expectedColors)
                $actual = [IO.File]::ReadAllBytes($actualColors)
                Assert-That ($reference.Length -eq $expectedFrames * 3 -and $actual.Length -eq $reference.Length) 'Incorrect VFR color sample count'
                for ($i = 0; $i -lt $reference.Length; $i += 3) {
                    $expectedDominant = 0
                    $actualDominant = 0
                    for ($channel = 1; $channel -lt 3; $channel++) {
                        if ($reference[$i + $channel] -gt $reference[$i + $expectedDominant]) { $expectedDominant = $channel }
                        if ($actual[$i + $channel] -gt $actual[$i + $actualDominant]) { $actualDominant = $channel }
                    }
                    Assert-That ($actualDominant -eq $expectedDominant) "VFR sampling selected the wrong color at frame $($i / 3)"
                }
            }
        } catch { $failure = $_.Exception.Message; Write-Warning "$($case.Name): $failure" }
        $results.Add([pscustomobject]@{ name = $case.Name; passed = ($null -eq $failure); seconds = $watch.Elapsed.TotalSeconds
            input = $case.Input; output = $output; error = $failure; probe = $observed })
        if ($script:abortMatrix) { throw 'Aborting matrix: a timed-out process tree could not be stopped.' }
    }
} catch { $setupError = $_.Exception.Message; Write-Warning "Validation setup failed: $setupError" }
finally {
    $failed = @($results | Where-Object { -not $_.passed }).Count
    $summary = [ordered]@{ startedUtc = $started.ToString('o'); completedUtc = [DateTime]::UtcNow.ToString('o')
        buildDir = $BuildDir; qtBin = $QtBin; timeoutSeconds = $TimeoutSeconds; runDir = $runDir
        setupError = $setupError; passed = ($null -eq $setupError -and $failed -eq 0 -and $results.Count -gt 0)
        caseCount = $results.Count; failedCount = $failed; cases = @($results.ToArray()) }
    [IO.File]::WriteAllText((Join-Path $runDir 'summary.json'), ($summary | ConvertTo-Json -Depth 15), $utf8)
    Write-Host "Validation: $($results.Count - $failed)/$($results.Count) passed. Report: $runDir\summary.json"
}
if (-not $summary.passed) { exit 1 }
exit 0
