from pathlib import Path
import os,subprocess,json,time
ROOT=Path(__file__).resolve().parents[2];B=ROOT/'FrameScale/build/Desktop_Qt_6_11_2_MinGW_64_bit_Debug';W=B/'readme-demos-4k';W.mkdir(exist_ok=True)
P=ROOT/'docs/media';F=B/'runtime/bin/ffmpeg.exe';PROBE=B/'runtime/bin/ffprobe.exe';S=B/'FrameScaleProcessingSmoke.exe'
ENV=os.environ.copy();ENV['PATH']='C:/Qt/Tools/mingw1310_64/bin;C:/Qt/6.11.2/mingw_64/bin;'+ENV['PATH']
def run(name,args,timeout=1800):
 start=time.monotonic();p=subprocess.run(list(map(str,args)),env=ENV,capture_output=True,timeout=timeout);(W/(name+'.log')).write_bytes(p.stdout+b'\n'+p.stderr)
 if p.returncode:raise RuntimeError(name+' '+str(p.returncode)+'\n'+p.stderr.decode(errors='replace')[-1000:]+p.stdout.decode(errors='replace')[-1000:])
 print(name,'done',round(time.monotonic()-start,1),'s',flush=True);return p.stdout
def probe(path):return json.loads(run('probe-'+path.stem,[PROBE,'-v','error','-count_frames','-select_streams','v:0','-show_entries','stream=width,height,avg_frame_rate,nb_read_frames:format=duration','-of','json',path]))
input=P/'source-fullhd-24fps.mp4'
if not input.exists():run('source',[F,'-v','error','-y','-ss','0.5','-i',os.environ.get('FRAMESCALE_DEMO_SOURCE','demo-source.mp4'),'-t','1.5','-vf','fps=24,setpts=PTS-STARTPTS','-an','-c:v','libx264','-preset','fast','-crf','16','-pix_fmt','yuv420p','-movflags','+faststart',input])
cases=[
 ('upscale-esrgan','Real-ESRGAN · Full HD → 4K','upscale','esrgan','realesr-animevideov3',2,24,'1920 × 1080 → 3840 × 2160 · 24 FPS'),
 ('upscale-cugan','Real-CUGAN · Full HD → 4K','upscale','cugan','realcugan-se',2,24,'1920 × 1080 → 3840 × 2160 · 24 FPS'),
 ('upscale-anime4k','Anime4K · Full HD → 4K','upscale','anime4k','anime4k-v4-a',2,24,'1920 × 1080 → 3840 × 2160 · 24 FPS'),
 ('interpolation-60','RIFE · Full HD a 60 FPS','interpolate','esrgan','realesr-animevideov3',1,60,'1920 × 1080 · 24 FPS → 60 FPS'),
 ('interpolation','RIFE · Full HD a 120 FPS','interpolate','esrgan','realesr-animevideov3',1,120,'1920 × 1080 · 24 FPS → 120 FPS'),
 ('combined','Real-ESRGAN + RIFE · 4K a 120 FPS','both','esrgan','realesr-animevideov3',2,120,'1080p / 24 FPS → 4K / 120 FPS'),
]
for name,title,operation,engine,model,scale,fps,detail in cases:
 output=P/(name+'-full.mp4')
 if not output.exists():run(name,[S,input,output,'video',operation,engine,model,scale,fps,'--codec=libx264','--preset=fast','--crf=18','--pixel-format=yuv420p','--no-audio'])
 info=probe(output);v=info['streams'][0];a,b=map(int,v['avg_frame_rate'].split('/'))
 assert (v['width'],v['height'])==((3840,2160) if scale==2 else (1920,1080))
 assert abs(a/b-fps)<.001,(name,info)
 assert int(v['nb_read_frames'])==round(1.5*fps),(name,info)
 text=W/(name+'-title.txt');text.write_text(title,encoding='utf-8');sub=W/(name+'-detail.txt');sub.write_text(detail,encoding='utf-8')
 def label(file,x,y,size,color):
  path=str(file).replace('\\','/').replace(':','\\:');return f"drawtext=fontfile='C\\:/Windows/Fonts/segoeui.ttf':textfile='{path}':x={x}:y={y}:fontsize={size}:fontcolor={color}"
 graph="[0:v]scale=640:360,setpts=PTS-STARTPTS[a];[1:v]scale=640:360,setpts=PTS-STARTPTS[b];[a][b]hstack,pad=1312:470:16:65:color=0x0d1420,"+label(text,20,14,27,'white')+','+label(sub,20,436,18,'0xa8b7c9')+",drawbox=x=655:y=65:w=2:h=360:color=0x007aff:t=fill[out]"
 run(name+'-comparison',[F,'-v','error','-y','-stream_loop','1','-i',input,'-stream_loop','1','-i',output,'-filter_complex',graph,'-map','[out]','-t','3','-r',fps,'-c:v','libx264','-preset','fast','-crf','19','-pix_fmt','yuv420p','-movflags','+faststart',P/(name+'.mp4')])
 run(name+'-gif',[F,'-v','error','-y','-i',P/(name+'.mp4'),'-filter_complex','fps=12,scale=800:-1,split[a][b];[a]palettegen=max_colors=128[p];[b][p]paletteuse=dither=bayer:bayer_scale=3','-loop','0',P/(name+'.gif')])
 run(name+'-poster',[F,'-v','error','-y','-ss','0.25','-i',P/(name+'.mp4'),'-frames:v','1',P/(name+'.png')])
 manifest=json.loads((P/'manifest.json').read_text(encoding='utf-8'));manifest=[m for m in manifest if m['id']!=name];manifest.append({'id':name,'title':title,'detail':detail,'input':input.name,'output':output.name,'output_metadata':info,'demo_duration':3,'source':'Trecho fornecido pelo utilizador: Demon Slayer - S4E08 (120).mp4','presentation':'Comparação reduzida; saída completa disponível no arquivo -full.mp4'});(P/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
 print('PUBLISHED ASSET READY:',name,flush=True)
