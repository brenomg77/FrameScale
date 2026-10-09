"""Regression: Enhancement must not carry prior-frame edges into moving footage."""
import os, subprocess, tempfile, json
from pathlib import Path
from test_paths import test_paths
ROOT = Path(__file__).resolve().parents[1]
BUILD, ENV = test_paths(__doc__)
OUT = Path(tempfile.mkdtemp(prefix="denoise-motion-", dir=ROOT / "build"))
FF = BUILD / "runtime/bin/ffmpeg.exe"
def run(args, data=None):
    p = subprocess.run(list(map(str,args)), input=data, capture_output=True, env=ENV, timeout=120)
    assert p.returncode == 0, p.stderr.decode(errors="replace")[-2000:]
    return p.stdout
w,h,n=96,64,8
frames=[]
for i in range(n):
    frames.append(bytes(112 if 8+i*8 <= x < 24+i*8 and 16 <= y < 48 else 80 for y in range(h) for x in range(w)))
results=[]
for name, ordered in (("forward",frames),("reverse",frames[::-1])):
    source=OUT/(name+".mkv"); target=OUT/(name+".mp4")
    run([FF,"-v","error","-f","rawvideo","-pix_fmt","gray","-s",f"{w}x{h}","-r","8","-i","pipe:0","-c:v","ffv1",source],b"".join(ordered))
    run([BUILD/"FrameScaleProcessingSmoke.exe",source,target,"video","copy","esrgan","realesr-animevideov3","1","8","--enhancement=1","--enhance-denoise=100","--enhance-sharpen=40","--crf=0","--pixel-format=yuv444p","--no-audio"])
    raw=run([FF,"-v","error","-i",target,"-pix_fmt","gray","-f","rawvideo","pipe:1"])
    assert len(raw)==w*h*n
    results.append([raw[i*w*h:(i+1)*w*h] for i in range(n)])
error=max(abs(a-b) for f,g in zip(results[0],results[1][::-1]) for a,b in zip(f,g))
report={"frames":n,"maximum_history_dependent_pixel_error":error,"passed":error<=1}
(OUT/"summary.json").write_text(json.dumps(report,indent=2))
print(OUT,report)
assert report["passed"], "Previous frames are contaminating denoise output"
