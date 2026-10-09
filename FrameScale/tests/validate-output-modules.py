"""Real FFmpeg / engine integration tests for output modules and fractional scale.
Creates unique fixtures under build; never overwrites the user's media.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time

parser=argparse.ArgumentParser()
parser.add_argument("--build",type=Path,default=Path(__file__).resolve().parents[1]/"build/Release")
parser.add_argument("--qt-bin",default="")
args=parser.parse_args()
build=args.build.resolve()
folder=build/"output-module-validation"/time.strftime("%Y%m%d-%H%M%S")
folder.mkdir(parents=True,exist_ok=False)
env=os.environ.copy();env["PATH"]=args.qt_bin+os.pathsep+env.get("PATH", "")
ffmpeg=build/"runtime/bin/ffmpeg.exe"
ffprobe=build/"runtime/bin/ffprobe.exe"
smoke=build/"FrameScaleProcessingSmoke.exe"
results=[]
def run(name,command,success=True):
    p=subprocess.run([str(x) for x in command],env=env,capture_output=True,timeout=120)
    (folder/(name+".log")).write_bytes(p.stdout+b"\n"+p.stderr)
    if (p.returncode==0)!=success: raise AssertionError(name+": "+p.stderr.decode("utf-8",errors="replace")[-3500:])
    return p
source=folder/"source.mp4"
run("fixture",[ffmpeg,"-v","error","-y","-f","lavfi","-i","testsrc2=size=102x70:rate=4:duration=2","-f","lavfi","-i","sine=frequency=440:sample_rate=48000:duration=2","-c:v","libx264","-pix_fmt","yuv420p","-c:a","aac","-metadata","title=FrameScale output modules","-metadata","comment=preserved","-shortest",source])
image=folder/"source.png"
run("image-fixture",[ffmpeg,"-v","error","-i",source,"-frames:v","1",image])
silent=folder/"silent.mp4"
run("silent-fixture",[ffmpeg,"-v","error","-i",source,"-an","-c:v","copy",silent])
def probe(path):
    p=subprocess.run([str(ffprobe),"-v","error","-count_frames","-show_streams","-show_format","-of","json",str(path)],capture_output=True,env=env,check=True)
    return json.loads(p.stdout)
def case(name,suffix,options,scale=1,operation="upscale",expected_codec=None,dimensions=(102,70),sequence=False,audio=False,frames=8,source_path=source,media="video"):
    output=folder/(name+suffix)
    command=[smoke,source_path,output,media,operation,"esrgan","realesr-animevideov3",str(scale),"8",*options]
    try:
        run(name,command)
        if sequence:
            files=sorted(output.glob("frame_*"))
            assert len(files)==frames,(len(files),frames)
            assert files[0].stem=="frame_00000001" and files[-1].stem==f"frame_{frames:08d}"
            d=probe(files[0]);v=d["streams"][0]
            assert (v["width"],v["height"])==dimensions
        else:
            d=probe(output)
            v=next(x for x in d["streams"] if x["codec_type"]==("audio" if audio else "video"))
            if expected_codec: assert v["codec_name"]==expected_codec,v["codec_name"]
            if audio: assert len(d["streams"])==1
            else:
                assert (v["width"],v["height"])==dimensions,(v["width"],v["height"])
                assert int(v["nb_read_frames"])==(1 if media=="image" else frames),v["nb_read_frames"]
            run(name+"-decode",[ffmpeg,"-v","error","-xerror","-i",output,"-f","null","-"])
            if audio:
                assert abs(float(d["format"]["duration"])-1)<.15
                assert v["sample_rate"]=="44100" and v["channels"]==1
                assert d["format"].get("tags",{}).get("title")=="FrameScale output modules"
        results.append({"case":name,"passed":True,"output":str(output)})
    except Exception as e:
        results.append({"case":name,"passed":False,"error":str(e)})
    print(name,results[-1]["passed"],flush=True)
    return output
case("avi-mjpeg",".avi",["--codec=mjpeg","--pixel-format=yuvj420p"],expected_codec="mjpeg")
case("avi-h264",".avi",["--codec=libx264"],expected_codec="h264")
for suffix in (".mp3",".wav"):
    case("audio"+suffix[1:],suffix,["--trim-start=0.5","--trim-end=1.5","--sample-rate=44100","--channels=1"],scale=10,operation="both",audio=True,expected_codec="mp3" if suffix==".mp3" else "pcm_s16le")
case("fractional-video",".mp4",[],scale=1.5,expected_codec="h264",dimensions=(154,106))
case("fractional-image",".jpg",[],scale=1.25,dimensions=(128,88),media="image",source_path=image,expected_codec="mjpeg")
case("sequence-png","",["--format=.png"],sequence=True)
seq=case("sequence-jpeg","",["--format=.jpg"],scale=1.5,sequence=True,dimensions=(153,105))
case("sequence-interpolated","",["--format=.png"],operation="both",scale=1.5,sequence=True,dimensions=(153,105),frames=16)
for mode in ("cbr","vbr"):
    case(mode,".mp4",["--rate-control="+mode,"--bit-rate=1000000","--profile=high","--level=4.1","--audio-codec=aac","--sample-rate=44100","--channels=1"],expected_codec="h264")
case("quicktime-hq",".mov",["--codec=prores_ks","--pixel-format=yuv422p10le","--audio-codec=pcm_s16le"],expected_codec="prores")
case("gif",".gif",[],expected_codec="gif")
pcm_source=folder/"source-pcm.mkv"
run("pcm-fixture",[ffmpeg,"-v","error","-i",source,"-c:v","copy","-c:a","pcm_s16le",pcm_source])
converted=case("compatible-audio-mp4",".mp4",["--audio-codec=copy"],source_path=pcm_source,expected_codec="h264")
try:
    assert next(s for s in probe(converted)["streams"] if s["codec_type"]=="audio")["codec_name"]=="aac"
    results.append({"case":"mp4-audio-fallback","passed":True})
except Exception as e: results.append({"case":"mp4-audio-fallback","passed":False,"error":str(e)})
try:
    run("missing-audio",[smoke,silent,folder/"no-audio.mp3","video","upscale","esrgan","realesr-animevideov3","1","8"],success=False)
    assert not (folder/"no-audio.mp3").exists()
    protected=folder/"protected";protected.mkdir();(protected/"keep.txt").write_text("keep me")
    run("protected-directory",[smoke,source,protected,"video","upscale","esrgan","realesr-animevideov3","1","8","--format=.png","--overwrite"],success=False)
    assert (protected/"keep.txt").read_text()=="keep me"
    command=[smoke,source,seq,"video","upscale","esrgan","realesr-animevideov3","1","8","--format=.jpg"]
    original=hashlib.sha256((seq/"frame_00000001.jpg").read_bytes()).hexdigest()
    run("sequence-refuse-overwrite",command,success=False)
    assert hashlib.sha256((seq/"frame_00000001.jpg").read_bytes()).hexdigest()==original
    run("sequence-replace",command+["--overwrite"])
    assert probe(seq/"frame_00000001.jpg")["streams"][0]["width"]==102
    assert not list(folder.glob("*.part*")) and not list(folder.glob("*.backup"))
    results.append({"case":"failure-and-directory-commit","passed":True})
except Exception as e: results.append({"case":"failure-and-directory-commit","passed":False,"error":str(e)})
(folder/"summary.json").write_text(json.dumps(results,indent=2),encoding="utf-8")
print("Report:",folder/"summary.json")
raise SystemExit(0 if all(r["passed"] for r in results) else 1)
