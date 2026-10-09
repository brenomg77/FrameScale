"""Real cancellation regression: preserve a decodable prefix and existing files."""
from pathlib import Path
import os, subprocess, tempfile, json
from test_paths import test_paths
ROOT=Path(__file__).resolve().parents[1]
BUILD,ENV=test_paths(__doc__)
OUT=Path(tempfile.mkdtemp(prefix='partial-export-',dir=ROOT/'build'))
FF=ROOT/'runtime/bin/ffmpeg.exe';PROBE=ROOT/'runtime/bin/ffprobe.exe'
SMOKE=BUILD/'FrameScaleProcessingSmoke.exe'
def run(name,args,expected=0):
 r=subprocess.run(list(map(str,args)),capture_output=True,cwd=ROOT,env=ENV,timeout=180)
 (OUT/(name+'.log')).write_bytes(r.stdout+r.stderr)
 assert r.returncode==expected,(name,r.returncode,r.stderr.decode(errors='replace')[-1500:])
 return r.stdout
source=OUT/'source.mp4'
run('fixture',[FF,'-v','error','-f','lavfi','-i','testsrc2=s=640x360:r=24:d=3','-c:v','libx264','-preset','ultrafast',source])
report={}
for operation in ['interpolate','copy']:
 dest=OUT/(operation+'.mp4');dest.write_bytes(b'existing destination must survive')
 first=OUT/(operation+'_2.mp4');first.write_bytes(b'existing partial must survive')
 run(operation,[SMOKE,source,dest,'video',operation,'esrgan','realesr-animevideov3',2,120,'--preset=slow','--no-audio','--overwrite','--preserve-partial=1','--cancel-at-frame=10'],2)
 assert dest.read_bytes()==b'existing destination must survive'
 assert first.read_bytes()==b'existing partial must survive'
 partial=OUT/(operation+'_3.mp4');assert partial.is_file(),operation
 data=json.loads(run(operation+'-probe',[PROBE,'-v','error','-select_streams','v:0','-count_frames','-show_entries','stream=width,height,nb_read_frames:format=duration','-of','json',partial]))
 count=int(data['streams'][0]['nb_read_frames']);assert 0<count<= (360 if operation=='interpolate' else 72)
 assert float(data['format']['duration'])<=3.05
 run(operation+'-decode',[FF,'-v','error','-xerror','-i',partial,'-f','null','-'])
 report[operation]=data
# With no collision, cancellation keeps the exact requested output name.
fresh=OUT/'chosen-name.mp4'
run('fresh-name',[SMOKE,source,fresh,'video','copy','esrgan','realesr-animevideov3',2,120,'--preset=slow','--no-audio','--preserve-partial=1','--cancel-at-frame=10'],2)
assert fresh.is_file()
assert not list(OUT.glob('*_parcial*'))
run('fresh-name-decode',[FF,'-v','error','-xerror','-i',fresh,'-f','null','-'])
report['requested_name_preserved']=True
(OUT/'report.json').write_text(json.dumps(report,indent=2))
print(OUT)
