from pathlib import Path
import os,subprocess,json,time
ROOT=Path(__file__).resolve().parents[2];B=ROOT/'FrameScale/build/Desktop_Qt_6_11_2_MinGW_64_bit_Debug';W=B/'readme-demos-4k';P=ROOT/'docs/media';F=B/'runtime/bin/ffmpeg.exe';PROBE=B/'runtime/bin/ffprobe.exe';S=B/'FrameScaleProcessingSmoke.exe';ENV=os.environ.copy();ENV['PATH']='C:/Qt/Tools/mingw1310_64/bin;C:/Qt/6.11.2/mingw_64/bin;'+ENV['PATH']
def run(name,args):
 p=subprocess.run(list(map(str,args)),env=ENV,capture_output=True,timeout=600);(W/(name+'.log')).write_bytes(p.stdout+b'\n'+p.stderr);assert p.returncode==0,(name,p.stderr.decode(errors='replace')[-900:]);print(name,'done',flush=True);return p.stdout
input=P/'combined-full.mp4';output=P/'fps-reduction-full.mp4';assert input.exists()
if not output.exists():run('4k60',[S,input,output,'video','copy','esrgan','realesr-animevideov3','1','120','--reduce-fps=60','--codec=libx264','--preset=fast','--crf=18','--pixel-format=yuv420p','--no-audio'])
info=json.loads(run('probe-4k60',[PROBE,'-v','error','-count_frames','-select_streams','v:0','-show_entries','stream=width,height,avg_frame_rate,nb_read_frames:format=duration','-of','json',output]));v=info['streams'][0];assert (v['width'],v['height'],v['avg_frame_rate'],v['nb_read_frames'])==(3840,2160,'60/1','90')
title='Redução de FPS · 4K a 120 FPS → 4K a 60 FPS';detail='3840 × 2160 preservado · 120 FPS → 60 FPS · mesma duração'
t=W/'4k60-title.txt';t.write_text(title,encoding='utf-8');d=W/'4k60-detail.txt';d.write_text(detail,encoding='utf-8')
def label(p,y,size):
 p=str(p).replace('\\','/').replace(':','\\:');return f"drawtext=fontfile='C\\:/Windows/Fonts/segoeui.ttf':textfile='{p}':x=20:y={y}:fontsize={size}:fontcolor=white"
graph='[0:v]scale=640:360,setpts=PTS-STARTPTS[a];[1:v]scale=640:360,setpts=PTS-STARTPTS[b];[a][b]hstack,pad=1312:470:16:65:color=0x0d1420,'+label(t,14,25)+','+label(d,436,17)+'[out]'
run('4k60-comparison',[F,'-v','error','-y','-stream_loop','1','-i',input,'-stream_loop','1','-i',output,'-filter_complex',graph,'-map','[out]','-t','3','-r','120','-c:v','libx264','-preset','fast','-crf','19','-pix_fmt','yuv420p','-movflags','+faststart',P/'fps-reduction.mp4'])
run('4k60-gif',[F,'-v','error','-y','-i',P/'fps-reduction.mp4','-filter_complex','fps=12,scale=800:-1,split[a][b];[a]palettegen=max_colors=128[p];[b][p]paletteuse=dither=bayer:bayer_scale=3','-loop','0',P/'fps-reduction.gif'])
run('4k60-poster',[F,'-v','error','-y','-ss','0.25','-i',P/'fps-reduction.mp4','-frames:v','1',P/'fps-reduction.png'])
manifest=json.loads((P/'manifest.json').read_text(encoding='utf-8'));manifest=[m for m in manifest if m['id']!='fps-reduction'];manifest.append({'id':'fps-reduction','title':title,'detail':detail,'input':input.name,'output':output.name,'output_metadata':info,'demo_duration':3});(P/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
