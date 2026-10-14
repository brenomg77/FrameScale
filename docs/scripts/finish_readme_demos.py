from pathlib import Path
import os, subprocess,json,shutil
ROOT=Path(__file__).resolve().parents[2];B=ROOT/'FrameScale/build/Desktop_Qt_6_11_2_MinGW_64_bit_Debug';W=B/'readme-demos';P=ROOT/'docs/media'
F=B/'runtime/bin/ffmpeg.exe';S=B/'FrameScaleProcessingSmoke.exe';PROBE=B/'runtime/bin/ffprobe.exe'
ENV=os.environ.copy();ENV['PATH']='C:/Qt/Tools/mingw1310_64/bin;C:/Qt/6.11.2/mingw_64/bin;'+ENV['PATH']
def run(name,args):
 r=subprocess.run(list(map(str,args)),env=ENV,capture_output=True,timeout=180);(W/(name+'.log')).write_bytes(r.stdout+b'\n'+r.stderr)
 assert r.returncode==0,(name,r.stderr.decode(errors='replace')[-1200:],r.stdout.decode(errors='replace')[-600:]);return r.stdout
def job(name,path,input=None,flags=()):
 run(name,[S,input or W/'clean.mp4',path,'video','copy','esrgan','realesr-animevideov3','1','16','--preset=ultrafast','--crf=0','--pixel-format=yuv444p','--overwrite',*flags])
def media(name,title,clip,detail):
 text=W/(name+'-title.txt');text.write_text(title,encoding='utf-8');sub=W/(name+'-subtitle.txt');sub.write_text(detail,encoding='utf-8')
 def draw(path,y,size):
  path=str(path).replace('\\','/').replace(':','\\:');return f"drawtext=fontfile='C\\:/Windows/Fonts/segoeui.ttf':textfile='{path}':x=20:y={y}:fontsize={size}:fontcolor=white"
 out=P/(name+'.mp4')
 run(name+'-presentation',[F,'-v','error','-y','-i',clip,'-vf','scale=768:216:force_original_aspect_ratio=decrease,pad=800:328:(ow-iw)/2:70:color=0x0d1420,'+draw(text,16,23)+','+draw(sub,302,13),'-t','3','-c:v','libx264','-crf','20','-pix_fmt','yuv420p','-c:a','aac','-movflags','+faststart',out])
 run(name+'-gif',[F,'-v','error','-y','-i',out,'-filter_complex','fps=8,scale=640:-1,split[a][b];[a]palettegen=max_colors=128[p];[b][p]paletteuse=dither=bayer:bayer_scale=3','-loop','0',P/(name+'.gif')])
 run(name+'-png',[F,'-v','error','-y','-ss','0.25','-i',out,'-frames:v','1',P/(name+'.png')])
 manifest=json.loads((P/'manifest.json').read_text(encoding='utf-8'));manifest=[m for m in manifest if m['id']!=name];manifest.append({'id':name,'title':title,'detail':detail,'input':'clean.mp4','output':clip.name});(P/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
 print('Demo ready:',name,flush=True)

clean=W/'clean.mp4'
overlay=W/'overlay.mp4';run('overlay',[F,'-v','error','-y','-i',clean,'-vf','hflip','-c:v','libx264','-crf','0','-an',overlay])
layers=[{'path':str(overlay),'kind':'video','duration':3,'fps':8,'width':384,'height':216,'in':0,'out':1.5,'start':1.5},
 {'path':str(clean),'kind':'video','duration':3,'fps':8,'width':384,'height':216,'in':0,'out':3,'start':0},
 {'path':str(clean),'kind':'audio','duration':3,'in':0,'out':3,'start':0}]
layerfile=W/'layers-demo.json';layerfile.write_text(json.dumps(layers),encoding='utf-8');layerout=W/'layers-demo.mp4';job('layers-demo',layerout,flags=['--layers='+str(layerfile)]);media('layers','Composição de camadas',layerout,'Vídeo base + camada espelhada a partir de 1,5 s + áudio')
audio=W/'exported.wav';job('audio-export',audio)
wave=W/'audio-wave.mp4';run('audio-wave',[F,'-v','error','-y','-i',audio,'-filter_complex','[0:a]showwaves=s=768x216:mode=line:rate=16:colors=0x007aff,format=yuv420p[v]','-map','[v]','-map','0:a','-c:v','libx264','-c:a','aac',wave]);media('audio','Exportação de áudio',wave,'WAV exportado pelo FrameScale · visualização da onda de áudio')
gif=W/'exported.gif';job('gif-export',gif,flags=['--no-audio']);media('gif','Exportação em GIF',gif,'Animação GIF exportada pelo FrameScale')
sequence=W/'sequence';job('sequence-export',sequence,flags=['--no-audio','--format=.png'])
files=sorted(sequence.glob('*.png'));assert len(files)==24,len(files)
seqclip=W/'sequence-demo.mp4';run('sequence-video',[F,'-v','error','-y','-framerate','8','-i',sequence/'frame_%08d.png','-c:v','libx264','-crf','0',seqclip]);media('sequence','Sequência de imagens',seqclip,'24 arquivos PNG exportados pelo FrameScale · 8 FPS')
shutil.copy2(W/'source.mp4',P/'source-192x108-8fps.mp4');shutil.copy2(clean,P/'source-384x216-8fps.mp4')
print('Extra demos done',flush=True)
