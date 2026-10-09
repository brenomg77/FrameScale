from pathlib import Path
import subprocess, json, os, tempfile, time, argparse
from PIL import Image
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
OUT=Path(tempfile.mkdtemp(prefix='interpolation-review-',dir=ROOT/'build'))
ff=ROOT/'runtime/bin/ffmpeg.exe'
rife=ROOT/'runtime/bin/rife-ncnn-vulkan.exe'
parser=argparse.ArgumentParser(description='Compare RIFE temporal processing on a supplied video; edge metrics are not perceptual quality scores.')
parser.add_argument('source',type=Path)
source=parser.parse_args().source
def run(name,args):
 t=time.monotonic()
 r=subprocess.run(list(map(str,args)),cwd=ROOT,capture_output=True,timeout=600)
 (OUT/(name+'.log')).write_bytes(r.stdout+r.stderr)
 if r.returncode:raise RuntimeError(name+': '+r.stderr.decode(errors='replace')[-1500:])
 return time.monotonic()-t
frames=OUT/'source';frames.mkdir()
run('extract',[ff,'-v','error','-i',source,'-t','2','-vf','scale=480:270,format=rgb24','-frames:v','48',frames/'%08d.png'])
report={}
for mode in ['normal','temporal']:
 dest=OUT/mode;dest.mkdir()
 elapsed=run(mode,[rife,'-i',frames,'-o',dest,'-n','240','-m',ROOT/'models/rife/rife-v4.6','-f','%08d.png']+(['-z'] if mode=='temporal' else []))
 files=sorted(dest.glob('*.png'));assert len(files)==240
 sharp=[]
 for file in files:
  im=np.asarray(Image.open(file).convert('L'),dtype=float)
  sharp.append(float(np.mean(np.abs(im[1:]-im[:-1]))+np.mean(np.abs(im[:,1:]-im[:,:-1]))))
 anchor=[]
 for n,file in enumerate(sorted(frames.glob('*.png'))):
  a=np.asarray(Image.open(file).convert('RGB'),dtype=float)
  b=np.asarray(Image.open(files[n*5]).convert('RGB'),dtype=float)
  anchor.append(float(np.abs(a-b).max()))
 report[mode]={'seconds':elapsed,'frames':len(files),'anchor_max_error':max(anchor),'edge_strength':sharp}
 (OUT/'report.json').write_text(json.dumps(report,indent=2))
print(OUT)
