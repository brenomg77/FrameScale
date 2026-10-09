"""Real ProcessingJob validation of Enhancement, using generated media only."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
from test_paths import test_paths

ROOT = Path(__file__).resolve().parents[1]
BUILD, ENV = test_paths(__doc__)
OUT = Path(tempfile.mkdtemp(prefix="enhancement-validation-", dir=ROOT / "build"))
FFMPEG = BUILD / "runtime/bin/ffmpeg.exe"
FFPROBE = BUILD / "runtime/bin/ffprobe.exe"
SMOKE = BUILD / "FrameScaleProcessingSmoke.exe"
RESULTS = []


def run(name, command, expected=0):
    p = subprocess.run(list(map(str, command)), env=ENV, capture_output=True, timeout=120)
    (OUT / (name + ".log")).write_bytes(p.stdout + b"\n" + p.stderr)
    assert p.returncode == expected, (name, p.returncode, p.stderr.decode(errors="replace")[-2000:])
    return p.stdout


def probe(path):
    return json.loads(run("probe-" + path.stem, [FFPROBE, "-v", "error", "-count_frames",
        "-show_streams", "-show_format", "-of", "json", path]))


def pixels(path):
    data = run("pixels-" + path.stem, [FFMPEG, "-v", "error", "-xerror", "-i", path,
        "-map", "0:v:0", "-pix_fmt", "rgb24", "-f", "rawvideo", "pipe:1"])
    return hashlib.sha256(data).hexdigest()


source = OUT / "source.mp4"
run("fixture", [FFMPEG, "-v", "error", "-f", "lavfi", "-i", "testsrc2=size=128x96:rate=4:duration=2",
    "-f", "lavfi", "-i", "sine=frequency=400:duration=2", "-vf", "noise=alls=20:allf=t+u:all_seed=7",
    "-c:v", "libx264", "-crf", "25", "-c:a", "aac", "-metadata", "title=Enhancement validation",
    "-metadata", "comment=metadata retained", "-shortest", source])
photo = OUT / "source.png"
run("photo", [FFMPEG, "-v", "error", "-i", source, "-frames:v", "1", photo])


def case(name, options=(), suffix=".mp4", media="video", scale=1, operation="upscale",
         frames=8, dimensions=(128, 96), metadata=True):
    path = OUT / (name + suffix)
    run(name, [SMOKE, photo if media == "image" else source, path, media, operation, "esrgan",
        "realesr-animevideov3", scale, "8", "--crf=0", "--pixel-format=yuv444p", *options])
    if not suffix:  # image sequence
        images = sorted(path.glob("frame_*.png"))
        assert len(images) == frames
        stream = probe(images[0])["streams"][0]
        assert (stream["width"], stream["height"]) == dimensions
    elif suffix == ".wav":
        info = probe(path)
        assert len(info["streams"]) == 1 and info["streams"][0]["codec_type"] == "audio"
    else:
        info = probe(path)
        stream = next(s for s in info["streams"] if s["codec_type"] == "video")
        assert int(stream["nb_read_frames"]) == (1 if media == "image" else frames)
        assert (stream["width"], stream["height"]) == dimensions
        run("decode-" + name, [FFMPEG, "-v", "error", "-xerror", "-i", path, "-f", "null", "-"])
        if media == "video" and metadata and suffix == ".mp4":
            tags = info["format"]["tags"]
            assert tags["title"] == "Enhancement validation" and tags["comment"] == "metadata retained"
            assert tags["encoding_crf"] == "0"
            assert any(s["codec_type"] == "audio" for s in info["streams"])
    return path


def check(name, action):
    try:
        action()
        RESULTS.append({"case": name, "passed": True})
    except Exception as error:
        RESULTS.append({"case": name, "passed": False, "error": str(error)})
    print(name, RESULTS[-1]["passed"], flush=True)
    (OUT / "summary.json").write_text(json.dumps(RESULTS, indent=2), encoding="utf-8")


base = pixels(case("baseline"))
all_filters = ["--enhancement=1", "--enhance-denoise=30", "--enhance-sharpen=30",
               "--enhance-deblock=50", "--enhance-deband=40", "--enhance-grain=10"]


def changed_filter(name):
    output = case(name, ["--enhancement=1", "--enhance-" + name + "=50"])
    assert pixels(output) != base, name + " did not alter output pixels"


for name in ("denoise", "sharpen", "deblock", "deband", "grain"):
    check(name, lambda n=name: changed_filter(n))


def disabled():
    output = case("disabled", [*all_filters, "--enhancement=0"])
    assert pixels(output) == base, "Disabled Enhancement changed the output"


check("disabled-bypass", disabled)
check("deinterlace-cadence", lambda: case("deinterlace", ["--enhancement=1", "--enhance-deinterlace=2"]))
check("trim-with-metadata", lambda: case("trim", [*all_filters, "--trim-start=.5", "--trim-end=1.5"], frames=4))
check("upscale-and-interpolation", lambda: case("both", all_filters, scale=1.5, operation="both", frames=16, dimensions=(192, 144)))
check("sequence", lambda: case("sequence", [*all_filters, "--format=.png"], suffix=""))
check("gif", lambda: case("animated", all_filters, suffix=".gif"))
check("audio-bypass", lambda: case("audio", all_filters, suffix=".wav"))


def image_filters():
    baseline = case("image-base", suffix=".png", media="image")
    output = case("image", all_filters, suffix=".png", media="image")
    assert pixels(baseline) != pixels(output)
    case("image-upscale", all_filters, suffix=".png", media="image", scale=1.5, dimensions=(192, 144))


check("images", image_filters)
def restore_one_x(model):
    output = case("restore-1x-" + str(model), ["--enhancement=1", "--enhance-model=" + str(model)], operation="copy", scale=7)
    assert pixels(output) != base, "Restoration model was bypassed at native resolution"
    image = case("restore-image-" + str(model), ["--enhancement=1", "--enhance-model=" + str(model)], operation="copy", scale=7, media="image", suffix=".png")
    assert pixels(image) != pixels(photo), "Image restoration model was bypassed"


for model in (1, 2, 3):
    check("restoration-1x-model-" + str(model), lambda m=model: restore_one_x(m))


def disabled_ai():
    output = case("disabled-ai", ["--enhancement=0", "--enhance-model=1"], operation="copy")
    assert pixels(output) == base, "Disabled AI restoration changed pixels"


check("disabled-ai-bypass", disabled_ai)


def noise_quality():
    import random
    rng = random.Random(17)
    clean, noisy = bytearray(), bytearray()
    for y in range(96):
        for x in range(128):
            level = 80 + x // 3 + y // 5
            clean.extend([level] * 3)
            value = max(0, min(255, round(level + rng.gauss(0, 9))))
            noisy.extend([value] * 3)
    header = b"P6\n128 96\n255\n"
    clean_path, noisy_path = OUT / "clean.ppm", OUT / "noisy.ppm"
    clean_path.write_bytes(header + clean)
    noisy_path.write_bytes(header + noisy)
    noisy_png = OUT / "noisy.png"
    run("noise-fixture", [FFMPEG, "-v", "error", "-i", noisy_path, noisy_png])
    result = OUT / "denoised.png"
    run("noise-quality", [SMOKE, noisy_png, result, "image", "copy", "esrgan", "realesr-animevideov3", "1", "8", "--enhancement=1", "--enhance-denoise=100"])
    data = run("denoised-pixels", [FFMPEG, "-v", "error", "-i", result, "-pix_fmt", "rgb24", "-f", "rawvideo", "pipe:1"])
    before = sum((a-b)**2 for a,b in zip(clean,noisy)) / len(clean)
    after = sum((a-b)**2 for a,b in zip(clean,data)) / len(clean)
    assert len(data) == len(clean) and after < before * .75, (before, after)
    (OUT / "noise-quality.json").write_text(json.dumps({"mse_before":before,"mse_after":after,"reduction_percent":100*(1-after/before)},indent=2))


check("denoise-quality", noise_quality)
print("Report:", OUT / "summary.json")
raise SystemExit(0 if all(r["passed"] for r in RESULTS) else 1)
