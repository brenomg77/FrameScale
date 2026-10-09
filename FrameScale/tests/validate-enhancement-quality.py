"""Objective enhancement regressions through the real ProcessingJob + FFmpeg.

Only generated media is used. The synthetic quality checks are not a claim
that one preset improves every photo/anime. --with-ai also executes the three
packaged restoration models at native output resolution (requires Vulkan).
"""
import argparse
import json
import os
from pathlib import Path
import random
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--build-dir", type=Path, default=ROOT / "build/Release")
parser.add_argument("--with-ai", action="store_true")
parser.add_argument("--qt-bin", default="")
args = parser.parse_args()
SMOKE = args.build_dir.resolve() / "FrameScaleProcessingSmoke.exe"
FFMPEG = ROOT / "runtime/bin/ffmpeg.exe"
OUT = Path(tempfile.mkdtemp(prefix="enhancement-quality-check-", dir=ROOT / "build"))
ENV = os.environ.copy()
if args.qt_bin:
    ENV['PATH'] = args.qt_bin + os.pathsep + ENV.get('PATH', '')
REPORT = {"scope": "generated noisy, blurred and alpha images; no perceptual guarantee", "cases": []}


def run(name, command, data=None):
    result = subprocess.run(list(map(str, command)), input=data, env=ENV, cwd=ROOT,
                            capture_output=True, timeout=120)
    (OUT / (name + ".log")).write_bytes(result.stderr)
    if result.returncode:
        raise AssertionError((name, result.returncode, result.stderr.decode(errors="replace")[-2500:]))
    return result.stdout


def image(name, data, width, height, pixel_format="rgb24"):
    path = OUT / (name + ".png")
    run(name, [FFMPEG, "-v", "error", "-f", "rawvideo", "-pixel_format", pixel_format,
        "-video_size", f"{width}x{height}", "-i", "pipe:0", "-frames:v", "1", path], bytes(data))
    return path


def pixels(path, pixel_format="rgb24"):
    return run("decode-" + path.stem, [FFMPEG, "-v", "error", "-xerror", "-i", path,
        "-frames:v", "1", "-pix_fmt", pixel_format, "-f", "rawvideo", "pipe:1"])


def process(name, source, *settings):
    path = OUT / (name + ".png")
    run(name, [SMOKE, source, path, "image", "copy", "esrgan", "realesr-animevideov3",
        "1", "60", *settings])
    return path


def mse(reference, actual):
    assert len(reference) == len(actual)
    return sum((a - b) ** 2 for a, b in zip(reference, actual)) / len(reference)


def check(name, action):
    try:
        measurements = action() or {}
        REPORT["cases"].append({"name": name, "passed": True, **measurements})
    except Exception as error:
        REPORT["cases"].append({"name": name, "passed": False, "error": str(error)})
    print(json.dumps(REPORT["cases"][-1]), flush=True)
    (OUT / "report.json").write_text(json.dumps(REPORT, indent=2), encoding="utf-8")


width, height = 128, 96
clean = bytearray()
for y in range(height):
    for x in range(width):
        level = 65 + x // 4 + y // 5
        color = (level, level + 8, level + 16)
        if (x - 26) ** 2 + (y - 28) ** 2 < 17 ** 2:
            color = (210, 210, 210)
        if 70 <= x < 112 and 15 <= y < 60:
            color = (180, 140, 90)
        if 20 <= x < 100 and 74 <= y < 77:
            color = (225, 225, 225)
        if 79 <= x < 106 and 20 <= y < 55 and (x // 3) % 2 == 0:
            color = (120, 95, 60)
        clean.extend(color)
rng = random.Random(17)
noisy = bytes(max(0, min(255, round(c + rng.gauss(0, 9)))) for c in clean)
clean_path = image("clean", clean, width, height)
noisy_path = image("noisy", noisy, width, height)


def denoise_quality():
    before = mse(clean, noisy)
    mild = pixels(process("gentle", noisy_path, "--enhancement=1", "--enhance-denoise=25"))
    strong = pixels(process("strong-noise", noisy_path, "--enhancement=1", "--enhance-denoise=60"))
    after_mild, after_strong = mse(clean, mild), mse(clean, strong)
    assert after_mild < before * .9, (before, after_mild)
    assert after_strong < after_mild, (after_mild, after_strong)
    return {"mse_before": before, "mse_gentle": after_mild, "mse_strong": after_strong,
            "gentle_reduction_percent": 100 * (1 - after_mild / before)}


def gentle_sharpen():
    low = pixels(process("sharp-1", noisy_path, "--enhancement=1", "--enhance-sharpen=1"))
    high = pixels(process("sharp-50", noisy_path, "--enhancement=1", "--enhance-sharpen=50"))
    low_change, high_change = mse(noisy, low), mse(noisy, high)
    assert 0 < low_change < 6, low_change  # Old CAS jumps far above this at 1.
    assert high_change > low_change * 3, (low_change, high_change)
    return {"mse_change_at_1": low_change, "mse_change_at_50": high_change}


def blurred_edges():
    blurred = OUT / "blurred.png"
    run("blur", [FFMPEG, "-v", "error", "-i", clean_path, "-vf", "gblur=sigma=0.8:steps=2", blurred])
    before = mse(clean, pixels(blurred))
    after = mse(clean, pixels(process("blur-restored", blurred, "--enhancement=1", "--enhance-sharpen=50")))
    assert after < before, (before, after)
    return {"mse_before": before, "mse_after": after}


def bypass():
    off = pixels(process("disabled", noisy_path, "--enhancement=0", "--enhance-denoise=100", "--enhance-sharpen=100",
                         "--enhance-clarity=100", "--enhance-contrast=100", "--enhance-vibrance=100"))
    neutral = pixels(process("neutral", noisy_path, "--enhancement=1"))
    assert off == noisy and neutral == noisy, "Disabled or neutral controls changed source pixels"


def transparent_source():
    rgba = bytearray()
    for i in range(width * height):
        rgba.extend(noisy[i * 3:i * 3 + 3])
        rgba.append((i % width) * 255 // (width - 1))
    source = image("alpha", rgba, width, height, "rgba")
    output = pixels(process("alpha-cleaned", source, "--enhancement=1", "--enhance-denoise=25",
                            "--enhance-sharpen=15", "--enhance-clarity=20", "--enhance-contrast=30",
                            "--enhance-vibrance=25"), "rgba")
    assert output[3::4] == rgba[3::4], "Enhancement changed the source alpha coverage"


def video_filters():
    source = OUT / "noisy-video.mkv"
    output = OUT / "cleaned-video.mkv"
    run("video-fixture", [FFMPEG, "-v", "error", "-loop", "1", "-framerate", "3", "-i", noisy_path,
        "-frames:v", "6", "-c:v", "ffv1", "-pix_fmt", "yuv444p", source])
    run("video-enhancement", [SMOKE, source, output, "video", "copy", "esrgan", "realesr-animevideov3",
        "1", "3", "--enhancement=1", "--enhance-denoise=25", "--enhance-sharpen=15",
        "--crf=0", "--pixel-format=yuv444p", "--no-audio"])
    data = run("decode-video", [FFMPEG, "-v", "error", "-xerror", "-i", output,
        "-pix_fmt", "rgb24", "-f", "rawvideo", "pipe:1"])
    assert len(data) == width * height * 3 * 6, "Enhancement changed frame count or resolution"
    before = mse(clean, pixels(source))
    after = mse(clean, data[:len(clean)])
    assert after < before, (before, after)
    assert all(data[start:start + len(clean)] == data[:len(clean)] for start in range(0, len(data), len(clean))), \
        "Spatial filters changed across identical source frames"
    return {"frames": 6, "mse_before": before, "mse_after": after}


def restoration(model):
    # Keep inference small; all three models still execute actual GPU weights.
    small = OUT / ("ai-source-" + str(model) + ".png")
    run("resize-ai-" + str(model), [FFMPEG, "-v", "error", "-i", noisy_path, "-vf", "scale=64:48", small])
    source = pixels(small)
    result = pixels(process("ai-model-" + str(model), small, "--enhancement=1", "--enhance-model=" + str(model)))
    assert len(result) == 64 * 48 * 3, "Native-resolution restoration changed dimensions"
    assert result != source, "Restoration model was not applied"
    return {"width": 64, "height": 48, "pixels_changed": True}


def color_curve():
    ramp = bytes(channel for y in range(32) for x in range(256) for channel in (x, x, x))
    source = image("ramp", ramp, 256, 32)
    for amount in (-100, 1, 30, 100):
        output = pixels(process("contrast-" + str(amount), source, "--enhancement=1", "--enhance-contrast=" + str(amount)))
        row = output[:256 * 3:3]
        # 8 -> 16 -> 8 bit FFmpeg conversions can round an endpoint by 1 LSB.
        assert row[0] <= 1 and row[-1] >= 254, (amount, row[0], row[-1])
        assert all(a <= b for a, b in zip(row, row[1:])), "Curve reversed tones"
        assert all(max(output[i:i + 3]) - min(output[i:i + 3]) <= 1 for i in range(0, len(output), 3)), \
            "Gray gained a color cast beyond 8-bit rounding"
        assert row.count(0) <= 3 and row.count(255) <= 3, "Contrast clipped shadow/highlight intervals"
        if amount == 30:
            assert row[64] < 64 and row[192] > 192, "Contrast did not separate midtones"
    return {"tested_values": [-100, 1, 30, 100], "monotonic": True, "endpoint_error_max_8bit": 1}


def creative_controls():
    for name in ("vibrance",):
        low = pixels(process(name + "-1", clean_path, "--enhancement=1", "--enhance-" + name + "=1"))
        high = pixels(process(name + "-60", clean_path, "--enhancement=1", "--enhance-" + name + "=60"))
        assert mse(clean, high) > mse(clean, low), (name, mse(clean, low), mse(clean, high))
    removed = pixels(process("removed-clarity", clean_path,
        "--enhancement=1", "--enhance-clarity=100"))
    neutral_result = pixels(process("removed-clarity-neutral", clean_path,
        "--enhancement=1"))
    assert removed == neutral_result, "Removed clarity still changes the output"
    neutral = bytes([128, 128, 128] * 64 * 48)
    source = image("gray", neutral, 64, 48)
    output = pixels(process("gray-vibrance", source, "--enhancement=1", "--enhance-vibrance=100"))
    assert output == neutral, "Vibrance colorized neutral gray"


def graded_video():
    source = OUT / "color-video.mkv"
    output = OUT / "graded-video.mkv"
    run("color-video-fixture", [FFMPEG, "-v", "error", "-loop", "1", "-framerate", "3", "-i", clean_path,
        "-frames:v", "6", "-c:v", "ffv1", "-pix_fmt", "bgr0", source])
    run("color-video-processing", [SMOKE, source, output, "video", "copy", "esrgan", "realesr-animevideov3",
        "1", "3", "--enhancement=1", "--enhance-clarity=25", "--enhance-contrast=30", "--enhance-vibrance=25",
        "--crf=0", "--pixel-format=yuv444p", "--no-audio"])
    data = run("decode-color-video", [FFMPEG, "-v", "error", "-xerror", "-i", output,
        "-pix_fmt", "rgb24", "-f", "rawvideo", "pipe:1"])
    assert len(data) == len(clean) * 6, "Color finishing changed duration or dimensions"
    assert data[:len(clean)] != clean, "Color finishing was not applied"
    assert all(data[start:start + len(clean)] == data[:len(clean)] for start in range(0, len(data), len(clean))), \
        "Color finishing changed across identical frames"
    return {"frames": 6, "identical_frames_remain_identical": True}


check("denoise-improves-known-noise", denoise_quality)
check("sharpen-has-gentle-low-end", gentle_sharpen)
check("sharpen-improves-known-blur", blurred_edges)
check("disabled-and-zero-bypass", bypass)
check("alpha-preserved", transparent_source)
check("video-filters-preserve-cadence", video_filters)
check("contrast-preserves-tones", color_curve)
check("removed-clarity-and-vibrance", creative_controls)
check("graded-video-preserves-cadence", graded_video)
if args.with_ai:
    for model in (1, 2, 3):
        check("native-restoration-model-" + str(model), lambda m=model: restoration(m))
print("Report:", OUT / "report.json")
raise SystemExit(0 if all(case["passed"] for case in REPORT["cases"]) else 1)
