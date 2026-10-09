"""Encode all ProRes profiles through ProcessingJob and inspect actual output.

Uses a small synthetic clip, without a GPU or UI automation. Outputs stay in a
unique build directory; no user media is modified.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import time

parser = argparse.ArgumentParser()
parser.add_argument("--build", type=Path, default=Path(__file__).resolve().parents[1] / "build/Release")
parser.add_argument("--qt-bin", default="")
args = parser.parse_args()
build = args.build.resolve()
repo = Path(__file__).resolve().parents[1]
folder = build / "prores-validation" / time.strftime("%Y%m%d-%H%M%S")
folder.mkdir(parents=True, exist_ok=False)
env = os.environ.copy()
env["PATH"] = args.qt_bin + os.pathsep + env.get("PATH", "")
ffmpeg = repo / "runtime/bin/ffmpeg.exe"
ffprobe = repo / "runtime/bin/ffprobe.exe"
smoke = build / "FrameScaleProcessingSmoke.exe"


def run(name, command):
    result = subprocess.run([str(x) for x in command], env=env,
                            capture_output=True, timeout=60)
    (folder / (name + ".log")).write_bytes(result.stdout + b"\n" + result.stderr)
    if result.returncode:
        raise AssertionError(name + ": " + result.stderr.decode("utf-8", errors="replace")[-3000:])
    return result


source = folder / "source.mp4"
run("fixture", [ffmpeg, "-v", "error", "-y", "-f", "lavfi", "-i",
                "testsrc2=size=128x72:rate=4:duration=1", "-c:v", "libx264",
                "-pix_fmt", "yuv420p", source])
profiles = [("Proxy", "apco"), ("LT", "apcs"), ("422", "apcn"),
            ("HQ", "apch"), ("4444", "ap4h"), ("4444-XQ", "ap4x")]
results = []
for profile, (name, tag) in enumerate(profiles):
    output = folder / (name + ".mov")
    pixels = "yuv422p10le" if profile < 4 else "yuv444p10le"
    run(name, [smoke, source, output, "video", "copy", "esrgan",
               "realesr-animevideov3", "1", "4", "--no-audio",
               "--codec=prores_ks", f"--prores-profile={profile}",
               f"--pixel-format={pixels}"])
    data = json.loads(run(name + "-probe", [ffprobe, "-v", "error", "-count_frames",
                     "-show_streams", "-show_format", "-of", "json", output]).stdout)
    video = next(s for s in data["streams"] if s["codec_type"] == "video")
    assert video["codec_name"] == "prores", video
    assert video["codec_tag_string"] == tag, video
    assert video["pix_fmt"] == ("yuv422p10le" if profile < 4 else "yuv444p12le"), video
    assert (video["width"], video["height"]) == (128, 72), video
    assert int(video["nb_read_frames"]) == 4, video
    assert abs(float(data["format"]["duration"]) - 1) < .001, data["format"]
    assert data["format"]["tags"]["encoding_profile"].startswith("ProRes "), data
    run(name + "-decode", [ffmpeg, "-v", "error", "-xerror", "-i", output, "-f", "null", "-"])
    results.append({"profile": name, "tag": tag, "passed": True})
    print(name + ": passed", flush=True)

# Old presets/options do not contain prores_profile and must still mean HQ.
run("legacy-hq", [smoke, source, folder / "legacy.mov", "video", "copy", "esrgan",
                  "realesr-animevideov3", "1", "4", "--no-audio",
                  "--codec=prores_ks", "--pixel-format=yuv422p10le"])
legacy = json.loads(run("legacy-probe", [ffprobe, "-v", "error", "-show_streams",
                       "-of", "json", folder / "legacy.mov"]).stdout)
assert legacy["streams"][0]["codec_tag_string"] == "apch", legacy
results.append({"profile": "legacy-HQ", "passed": True})
(folder / "results.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
print(f"All six profiles and legacy HQ passed. Results: {folder}", flush=True)
