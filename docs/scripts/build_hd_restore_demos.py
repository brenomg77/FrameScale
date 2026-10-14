from pathlib import Path
import os,subprocess,json,time
ROOT=Path(__file__).resolve().parents[2];B=ROOT/'FrameScale/build/Desktop_Qt_6_11_2_MinGW_64_bit_Debug';W=B/'readme-demos-hd';W.mkdir(exist_ok=True);P=ROOT/'docs/media'
F=B/'runtime/bin/ffmpeg.exe';PROBE=B/'runtime/bin/ffprobe.exe';S=B/'FrameScaleProcessingSmoke.exe';ENV=os.environ.copy();ENV['PATH']='C:/Qt/Tools/mingw1310_64/bin;C:/Qt/6.11.2/mingw_64/bin;'+ENV['PATH']
def run(name,args):
 start=time.monotonic();r=subprocess.run(list(map(str,args)),env=ENV,capture_output=True,timeout=900);(W/(name+'.log')).write_bytes(r.stdout+b'\n'+r.stderr)
 assert r.returncode==0,(name,r.stderr.decode(errors='replace')[-1200:],r.stdout.decode(errors='replace')[-700:]);print(name,'done',round(time.monotonic()-start,1),'s',flush=True);return r.stdout
input=P/'source-hd-noisy.mp4'
if not input.exists():run('source-hd-noisy',[F,'-v','error','-y','-i',P/'source-fullhd-24fps.mp4','-t','1','-vf','scale=1280:720,noise=alls=8:allf=t+u:all_seed=23','-an','-c:v','libx264','-preset','fast','-crf','24','-pix_fmt','yuv420p','-movflags','+faststart',input])
for name,title,model in [('restore-plus','Real-ESRGAN Plus · restauração HD',1),('restore-anime','Real-ESRGAN Anime · restauração HD',2),('restore-cugan','Real-CUGAN SE · restauração HD',3)]:
 output=P/(name+'-full.mp4')
 if not output.exists():run(name,[S,input,output,'video','copy','esrgan','realesr-animevideov3','1','24','--enhancement=1','--enhance-model='+str(model),'--codec=libx264','--preset=fast','--crf=18','--pixel-format=yuv420p','--no-audio'])
 info=json.loads(run('probe-'+name,[PROBE,'-v','error','-count_frames','-select_streams','v:0','-show_entries','stream=width,height,avg_frame_rate,nb_read_frames:format=duration','-of','json',output]));v=info['streams'][0];assert (v['width'],v['height'],v['nb_read_frames'],v['avg_frame_rate'])==(1280,720,'24','24/1')
 detail='1280 × 720 · 24 FPS · restauração em resolução nativa · ruído acrescentado na entrada'
 t=W/(name+'-title.txt');t.write_text(title,encoding='utf-8');d=W/(name+'-detail.txt');d.write_text(detail,encoding='utf-8')
 def label(p,y,size):
  p=str(p).replace('\\','/').replace(':','\\:');return f"drawtext=fontfile='C\\:/Windows/Fonts/segoeui.ttf':textfile='{p}':x=20:y={y}:fontsize={size}:fontcolor=white"
 graph='[0:v]scale=640:360,setpts=PTS-STARTPTS[a];[1:v]scale=640:360,setpts=PTS-STARTPTS[b];[a][b]hstack,pad=1312:470:16:65:color=0x0d1420,'+label(t,14,27)+','+label(d,436,17)+",drawtext=fontfile='C\\:/Windows/Fonts/segoeui.ttf':text=ENTRADA:x=20:y=44:fontsize=15:fontcolor=0xa8b7c9,drawtext=fontfile='C\\:/Windows/Fonts/segoeui.ttf':text=RESULTADO:x=676:y=44:fontsize=15:fontcolor=0x62acff[out]"
 run(name+'-comparison',[F,'-v','error','-y','-stream_loop','2','-i',input,'-stream_loop','2','-i',output,'-filter_complex',graph,'-map','[out]','-t','3','-c:v','libx264','-preset','fast','-crf','19','-pix_fmt','yuv420p','-movflags','+faststart',P/(name+'.mp4')])
 run(name+'-gif',[F,'-v','error','-y','-i',P/(name+'.mp4'),'-filter_complex','fps=12,scale=800:-1,split[a][b];[a]palettegen=max_colors=128[p];[b][p]paletteuse=dither=bayer:bayer_scale=3','-loop','0',P/(name+'.gif')])
 run(name+'-poster',[F,'-v','error','-y','-ss','0.25','-i',P/(name+'.mp4'),'-frames:v','1',P/(name+'.png')])
 manifest=json.loads((P/'manifest.json').read_text(encoding='utf-8'));manifest=[m for m in manifest if m['id']!=name];manifest.append({'id':name,'title':title,'detail':detail,'input':input.name,'output':output.name,'output_metadata':info,'demo_duration':3,'source':'Trecho fornecido pelo utilizador; ruído e compressão acrescentados para demonstrar restauração.'});(P/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
 print('PUBLISHED ASSET READY:',name,flush=True)
