"""Optional feasibility probe, not a quality benchmark or a production preset.

Uses only the bundled FFmpeg. Creates a new directory under build on every run;
does not read or overwrite user media. No model or Python dependency download.
"""

import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
FFMPEG = ROOT / "runtime/bin/ffmpeg.exe"
FFPROBE = ROOT / "runtime/bin/ffprobe.exe"
(ROOT / "build").mkdir(exist_ok=True)
OUTPUT = Path(tempfile.mkdtemp(prefix="enhancement-probe-", dir=ROOT / "build"))


def run(name, executable, arguments):
    started = time.perf_counter()
    result = subprocess.run(
        [str(executable), *map(str, arguments)],
        capture_output=True,
        timeout=90,
    )
    (OUTPUT / f"{name}.log").write_bytes(result.stderr)
    if result.returncode:
        raise RuntimeError(f"{name}: {result.stderr.decode(errors='replace')[-3000:]}")
    return result.stdout, time.perf_counter() - started


source = OUTPUT / "source.mp4"
run("fixture", FFMPEG, [
    "-v", "error", "-f", "lavfi", "-i",
    "testsrc2=size=320x180:rate=12:duration=2",
    "-vf", "noise=alls=12:allf=t+u:all_seed=7",
    "-c:v", "libx264", "-crf", "32", "-pix_fmt", "yuv420p", source,
])


def decode_hash(name, path):
    data, _ = run(name, FFMPEG, [
        "-v", "error", "-i", path, "-map", "0:v:0", "-an",
        "-pix_fmt", "yuv420p", "-f", "rawvideo", "pipe:1",
    ])
    assert len(data) == 320 * 180 * 3 // 2 * 24, (name, len(data))
    return hashlib.sha256(data).hexdigest()


original_hash = decode_hash("decode-source", source)
filters = {
    "denoise": "hqdn3d=2:1.5:3:2.25",
    "sharpen": "cas=strength=0.25",
    "deblock": "deblock=filter=weak:block=8",
    "deband": "deband",
    "grain": "noise=alls=4:allf=t+u:all_seed=7",
    "deinterlace": "setfield=tff,bwdif=mode=send_frame:parity=tff:deint=all",
    "detail-blend": "split[original][filter];[filter]hqdn3d=2:1.5:3:2.25[clean];"
                    "[clean][original]blend=all_expr='A*0.75+B*0.25'",
}
report = {"fixture": "320x180, 12 fps, 2 seconds, H.264 with synthetic noise",
          "purpose": "Executable capability and stream integrity; not perceptual quality",
          "cases": []}
for name, expression in filters.items():
    output = OUTPUT / f"{name}.mkv"
    _, elapsed = run(name, FFMPEG, [
        "-v", "error", "-threads", "2", "-i", source,
        "-filter_complex" if name == "detail-blend" else "-vf", expression,
        "-threads", "2", "-c:v", "ffv1", "-pix_fmt", "yuv420p", output,
    ])
    data, _ = run(f"probe-{name}", FFPROBE, [
        "-v", "error", "-count_frames", "-show_streams", "-show_format",
        "-of", "json", output,
    ])
    probe = json.loads(data)
    stream = probe["streams"][0]
    assert (stream["width"], stream["height"]) == (320, 180)
    assert int(stream["nb_read_frames"]) == 24
    assert abs(float(probe["format"]["duration"]) - 2) < 0.01
    checksum = decode_hash(f"decode-{name}", output)
    report["cases"].append({"filter": name, "expression": expression,
                            "frames": 24, "duration": 2,
                            "pixels_changed": checksum != original_hash,
                            "elapsed_seconds": round(elapsed, 3), "passed": True})

(OUTPUT / "report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
print(json.dumps({"passed": len(report["cases"]), "report": str(OUTPUT / "report.json")}, indent=2))
