"""Exercise real restoration + enlargement, including 8x/10x and source alpha.

Uses generated media and the packaged Vulkan engines; no UI automation.
"""
from pathlib import Path
import json
import os
import subprocess
import tempfile
from test_paths import test_paths

ROOT = Path(__file__).resolve().parents[1]
BUILD, ENV = test_paths(__doc__)
OUT = Path(tempfile.mkdtemp(prefix="combined-processing-", dir=ROOT / "build"))
FFMPEG = ROOT / "runtime/bin/ffmpeg.exe"
FFPROBE = ROOT / "runtime/bin/ffprobe.exe"
SMOKE = BUILD / "FrameScaleProcessingSmoke.exe"
cases = []


def run(name, arguments):
    result = subprocess.run(list(map(str, arguments)), capture_output=True, env=ENV,
                            cwd=ROOT, timeout=300)
    (OUT / (name + ".log")).write_bytes(result.stdout + result.stderr)
    if result.returncode:
        raise AssertionError((name, result.returncode, result.stderr.decode(errors="replace")[-1800:]))
    return result.stdout


def probe(path):
    return json.loads(run("probe-" + path.stem, [FFPROBE, "-v", "error", "-select_streams", "v:0",
        "-count_frames", "-show_entries", "stream=width,height,nb_read_frames:format=duration", "-of", "json", path]))


source = OUT / "source.png"
video = OUT / "source.mp4"
run("fixture-image", [FFMPEG, "-v", "error", "-f", "lavfi", "-i",
    "testsrc2=s=64x48:r=2,format=rgba,colorchannelmixer=aa=0.5", "-frames:v", "1", source])
run("fixture-video", [FFMPEG, "-v", "error", "-f", "lavfi", "-i",
    "testsrc2=s=64x48:r=2:d=3", "-c:v", "libx264", "-pix_fmt", "yuv420p", video])


def check(name, media, operation, engine, model, scale, restore, expected_passes):
    dest = OUT / (name + (".png" if media == "image" else ".mp4"))
    log = run(name, [SMOKE, source if media == "image" else video, dest, media, operation,
        engine, model, scale, 4, "--enhancement=1", f"--enhance-model={restore}",
        "--enhance-denoise=15", "--enhance-sharpen=10", "--enhance-clarity=20",
        "--enhance-contrast=25", "--enhance-vibrance=15", "--preset=ultrafast", "--no-audio"])
    details = probe(dest)
    stream = details["streams"][0]
    final_scale = 1 if operation == "copy" else scale
    assert (stream["width"], stream["height"]) == (64 * final_scale, 48 * final_scale), details
    # Stage text confirms different networks actually ran, rather than merely
    # producing the right dimensions through one overridden network.
    stage_text = log.decode(errors="replace")
    assert ("Preparando imagem restaurada" in stage_text) == (expected_passes == 2), stage_text
    if media == "image":
        alpha = run(name + "-alpha", [FFMPEG, "-v", "error", "-i", dest,
            "-vf", "alphaextract", "-frames:v", "1", "-pix_fmt", "gray", "-f", "rawvideo", "pipe:1"])
        reference = run(name + "-expected-alpha", [FFMPEG, "-v", "error", "-i", source,
            "-vf", f"alphaextract,scale={stream['width']}:{stream['height']}:flags=lanczos",
            "-frames:v", "1", "-pix_fmt", "gray", "-f", "rawvideo", "pipe:1"])
        assert alpha == reference, "source transparency changed"
    else:
        assert int(stream["nb_read_frames"]) == (12 if operation == "both" else 6), details
        assert abs(float(details["format"]["duration"]) - 3) < 0.05, details
        run(name + "-decode", [FFMPEG, "-v", "error", "-xerror", "-i", dest, "-f", "null", "-"])
    cases.append({"name": name, "passed": True, "width": stream["width"], "height": stream["height"]})
    (OUT / "report.json").write_text(json.dumps(cases, indent=2), encoding="utf-8")
    print(json.dumps(cases[-1]), flush=True)


check("different-models-8x", "image", "upscale", "cugan", "realcugan-se", 8, 1, 2)
check("same-model-10x", "image", "upscale", "esrgan", "realesrgan-plus-anime-x4", 10, 2, 1)
check("filters-with-8x", "image", "upscale", "esrgan", "realesr-animevideov3", 8, 0, 1)
check("restoration-without-enlargement", "image", "copy", "cugan", "realcugan-se", 10, 1, 1)
check("video-combined-8x", "video", "upscale", "esrgan", "realesr-animevideov3", 8, 3, 2)
check("video-interpolation-combined", "video", "both", "esrgan", "realesr-animevideov3", 2, 3, 2)
print(OUT, flush=True)
