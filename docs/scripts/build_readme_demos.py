from pathlib import Path
import os, subprocess, json, math, time
from PIL import Image, ImageDraw

ROOT=Path(__file__).resolve().parents[2]
BUILD=ROOT/'FrameScale/build/Desktop_Qt_6_11_2_MinGW_64_bit_Debug'
WORK=BUILD/'readme-demos'
PUBLIC=ROOT/'docs/media'
WORK.mkdir(exist_ok=True);PUBLIC.mkdir(parents=True,exist_ok=True)
FF=BUILD/'runtime/bin/ffmpeg.exe';PROBE=BUILD/'runtime/bin/ffprobe.exe';SMOKE=BUILD/'FrameScaleProcessingSmoke.exe'
ENV=os.environ.copy();ENV['PATH']='C:/Qt/Tools/mingw1310_64/bin;C:/Qt/6.11.2/mingw_64/bin;'+ENV['PATH']
RESULTS=[]

def run(name,args,timeout=300):
    started=time.monotonic()
    p=subprocess.run(list(map(str,args)),env=ENV,capture_output=True,timeout=timeout)
    (WORK/(name+'.log')).write_bytes(p.stdout+b'\n'+p.stderr)
    if p.returncode:raise RuntimeError(f'{name}: {p.returncode}\n'+p.stderr.decode(errors='replace')[-1500:]+p.stdout.decode(errors='replace')[-800:])
    return p.stdout,time.monotonic()-started

def probe(path):
    raw,_=run('probe-'+path.stem,[PROBE,'-v','error','-count_frames','-select_streams','v:0','-show_entries','stream=width,height,avg_frame_rate,nb_read_frames:format=duration','-of','json',path])
    return json.loads(raw)

# Original procedural illustration: no third-party footage or personal files.
frames=WORK/'frames';frames.mkdir(exist_ok=True)
for k in range(24):
    im=Image.new('RGB',(384,216));d=ImageDraw.Draw(im)
    for y in range(216):
        t=y/215;d.line((0,y,384,y),fill=(round(14+34*t),round(28+38*t),round(62+33*t)))
    d.ellipse((270,18,316,64),fill='#ffe8ac')
    for i in range(45):
        x=(i*79+17)%384;y=(i*37+9)%96;d.ellipse((x,y,x+1,y+1),fill='#b5c8f3')
    d.polygon([(0,127),(67,58),(126,123),(203,48),(280,130),(345,76),(384,130),(384,216),(0,216)],fill='#4b677b')
    d.polygon([(176,76),(203,48),(233,88),(211,76),(199,84)],fill='#d7e4eb')
    d.polygon([(33,92),(67,58),(98,92),(72,79),(61,87)],fill='#d7e4eb')
    for i in range(9):
        x=i*48-10;h=24+(i*17)%39;y=154-h
        d.rectangle((x,y,x+35,174),fill=('#283c58','#334c64')[i%2],outline='#94a8b9',width=1)
        d.polygon([(x-3,y),(x+17,y-17),(x+39,y)],fill='#142841',outline='#8eabc3')
        for xx in range(x+5,x+31,9):
            for yy in range(y+6,169,11):d.rectangle((xx,yy,xx+4,yy+5),fill='#eac586')
    d.rectangle((0,176,384,216),fill='#162838')
    for y in [185,199]:d.line((0,y,384,y),fill='#597181',width=1)
    for x in range(0,384,27):d.rectangle((x,207,x+12,209),fill='#edbf7a')
    x=round(25+k*10)
    d.rounded_rectangle((x,148,x+78,193),radius=6,fill='#f7c84b',outline='#1a2a37',width=3)
    d.rectangle((x+8,155,x+67,174),fill='#b3e1eb',outline='#254659',width=2)
    for xx in [x+28,x+47]:d.line((xx,156,xx,173),fill='#254659',width=2)
    for xx in [x+17,x+61]:
        d.ellipse((xx-8,185,xx+8,201),fill='#152337',outline='#8ea4b3',width=2)
        d.line((xx-4,193,xx+4,193),fill='#cad8e4',width=1)
    d.rectangle((x+71,179,x+76,184),fill='#fff1be')
    # Fine wires, branches and masonry show edge/detail behavior.
    d.line((6,90,6,198),fill='#9bafbd',width=2);d.line((378,88,378,198),fill='#9bafbd',width=2)
    for j in range(3):d.arc((0,30+j*5,384,125+j*5),0,180,fill='#b1bfc9',width=1)
    im.save(frames/f'{k:03}.png')

clean=WORK/'clean.mp4';source=WORK/'source.mp4';noisy=WORK/'noisy.mp4';bands=WORK/'bands.mp4'
if not clean.exists():run('clean',[FF,'-v','error','-y','-framerate','8','-i',frames/'%03d.png','-f','lavfi','-i','sine=frequency=440:sample_rate=48000:duration=3','-c:v','libx264','-crf','0','-pix_fmt','yuv444p','-c:a','aac','-shortest',clean])
if not source.exists():run('source',[FF,'-v','error','-y','-i',clean,'-vf','scale=192:108:flags=area','-c:v','libx264','-crf','30','-pix_fmt','yuv420p','-c:a','aac',source])
if not noisy.exists():run('noisy',[FF,'-v','error','-y','-i',source,'-vf','noise=alls=18:allf=t+u:all_seed=42','-c:v','libx264','-crf','20','-pix_fmt','yuv420p','-an',noisy])
if not bands.exists():run('bands',[FF,'-v','error','-y','-i',source,'-vf','lutrgb=r=floor(val/24)*24:g=floor(val/24)*24:b=floor(val/24)*24','-c:v','libx264','-crf','0','-an',bands])

FONT='C\\:/Windows/Fonts/segoeui.ttf'
def compare(name,title,input,output,detail,seconds=3):
    meta=probe(output);v=meta['streams'][0];fps=v['avg_frame_rate']
    # Display-only encoding is separate from the native processing result.
    text=WORK/(name+'-title.txt');text.write_text(title,encoding='utf-8')
    left=WORK/(name+'-left.txt');left.write_text('ENTRADA',encoding='utf-8')
    right=WORK/(name+'-right.txt');right.write_text('RESULTADO',encoding='utf-8')
    subtitle=WORK/(name+'-subtitle.txt');subtitle.write_text(detail,encoding='utf-8')
    def draw(file,x,y,size,color):
        path=str(file).replace('\\','/').replace(':','\\:')
        return f"drawtext=fontfile='{FONT}':textfile='{path}':x={x}:y={y}:fontsize={size}:fontcolor={color}"
    filters=("[0:v]scale=384:216:flags=neighbor,tpad=stop_mode=clone:stop_duration=4,setpts=PTS-STARTPTS[a];"
             "[1:v]scale=384:216:flags=lanczos:force_original_aspect_ratio=decrease,pad=384:216:(ow-iw)/2:(oh-ih)/2:color=black,tpad=stop_mode=clone:stop_duration=4,setpts=PTS-STARTPTS[b];"
             "[a][b]hstack=inputs=2,pad=800:328:16:70:color=0x0d1420,"
             "drawbox=x=399:y=65:w=2:h=232:color=0x007aff:t=fill,"+
             draw(text,20,14,23,'white')+','+draw(left,24,47,12,'0xa8b7c9')+','+draw(right,424,47,12,'0x62acff')+','+
             draw(subtitle,20,302,13,'0xa8b7c9')+'[out]')
    mp4=PUBLIC/(name+'.mp4');gif=PUBLIC/(name+'.gif');poster=PUBLIC/(name+'.png')
    run('demo-'+name,[FF,'-v','error','-y','-stream_loop','-1','-i',input,'-stream_loop','-1','-i',output,'-filter_complex',filters,'-map','[out]','-t',seconds,'-r','16','-c:v','libx264','-preset','fast','-crf','20','-pix_fmt','yuv420p','-movflags','+faststart',mp4])
    run('gif-'+name,[FF,'-v','error','-y','-i',mp4,'-filter_complex','fps=8,scale=640:-1:flags=lanczos,split[a][b];[a]palettegen=max_colors=128[p];[b][p]paletteuse=dither=bayer:bayer_scale=3','-loop','0',gif])
    run('poster-'+name,[FF,'-v','error','-y','-ss','0.75','-i',mp4,'-frames:v','1',poster])
    RESULTS.append({'id':name,'title':title,'detail':detail,'input':input.name,'output':output.name,'output_metadata':meta,'demo_duration':seconds})
    (PUBLIC/'manifest.json').write_text(json.dumps(RESULTS,ensure_ascii=False,indent=2),encoding='utf-8')
    print('Demo ready:',name,flush=True)

cases=[
 ('upscale-esrgan','Ampliação · Real-ESRGAN',source,'upscale','esrgan','realesr-animevideov3',2,16,[],'192 × 108 → 384 × 216 · escala 2×'),
 ('upscale-cugan','Ampliação · Real-CUGAN',source,'upscale','cugan','realcugan-se',2,16,[],'192 × 108 → 384 × 216 · escala 2×'),
 ('upscale-anime4k','Ampliação · Anime4K',source,'upscale','anime4k','anime4k-v4-a',2,16,[],'192 × 108 → 384 × 216 · escala 2×'),
 ('interpolation','Interpolação · RIFE',clean,'interpolate','esrgan','realesr-animevideov3',1,16,[],'8 FPS → 16 FPS · sem alterar a resolução'),
 ('combined','Ampliação + interpolação',source,'both','esrgan','realesr-animevideov3',2,16,[],'192 × 108 / 8 FPS → 384 × 216 / 16 FPS'),
 ('restore-plus','Restauração · Real-ESRGAN Plus',noisy,'copy','esrgan','realesr-animevideov3',1,16,['--enhancement=1','--enhance-model=1'],'Restauração em 1× · modelo Plus · origem com ruído'),
 ('restore-anime','Restauração · Real-ESRGAN Anime',noisy,'copy','esrgan','realesr-animevideov3',1,16,['--enhancement=1','--enhance-model=2'],'Restauração em 1× · modelo Anime · origem com ruído'),
 ('restore-cugan','Restauração · Real-CUGAN SE',noisy,'copy','esrgan','realesr-animevideov3',1,16,['--enhancement=1','--enhance-model=3'],'Restauração em 1× · modelo SE · origem com ruído'),
 ('denoise','Filtro · redução de ruído',noisy,'copy','esrgan','realesr-animevideov3',1,16,['--enhancement=1','--enhance-denoise=65'],'Somente filtros · redução de ruído: 65'),
 ('sharpen','Filtro · nitidez',source,'copy','esrgan','realesr-animevideov3',1,16,['--enhancement=1','--enhance-sharpen=55'],'Somente filtros · nitidez: 55'),
 ('deblock','Filtro · reduzir blocos',source,'copy','esrgan','realesr-animevideov3',1,16,['--enhancement=1','--enhance-deblock=70'],'Somente filtros · reduzir blocos: 70'),
 ('deband','Filtro · reduzir faixas de cor',bands,'copy','esrgan','realesr-animevideov3',1,16,['--enhancement=1','--enhance-deband=70'],'Somente filtros · reduzir banding: 70'),
 ('grain','Filtro · grão',source,'copy','esrgan','realesr-animevideov3',1,16,['--enhancement=1','--enhance-grain=40'],'Somente filtros · grão: 40'),
 ('contrast','Filtro · contraste',source,'copy','esrgan','realesr-animevideov3',1,16,['--enhancement=1','--enhance-contrast=45'],'Somente filtros · contraste: +45'),
 ('vibrance','Filtro · intensidade das cores',source,'copy','esrgan','realesr-animevideov3',1,16,['--enhancement=1','--enhance-vibrance=65'],'Somente filtros · intensidade das cores: +65'),
 ('speed','Velocidade · movimento lento',clean,'copy','esrgan','realesr-animevideov3',1,16,['--speed=0.5'],'Velocidade: 0,5× · saída com 6 segundos'),
 ('fps-reduction','Redução de FPS',clean,'copy','esrgan','realesr-animevideov3',1,16,['--reduce-fps=4'],'8 FPS → 4 FPS · mesma duração'),
 ('trim','Corte na timeline',clean,'copy','esrgan','realesr-animevideov3',1,16,['--trim-start=1','--trim-end=2'],'Intervalo: 1 s → 2 s · saída com 8 frames'),
 ('rotate','Rotação de vídeo',clean,'copy','esrgan','realesr-animevideov3',1,16,['--quarter-turns=1'],'Rotação: 90° · conteúdo ajustado para esta comparação'),
 ('flip','Espelhamento de vídeo',clean,'copy','esrgan','realesr-animevideov3',1,16,['--flip-horizontal=1'],'Espelhamento horizontal'),
]
for name,title,input,op,engine,model,scale,fps,flags,detail in cases:
    output=WORK/(name+'.mp4')
    if not output.exists():
        print('Processing:',name,flush=True)
        run(name,[SMOKE,input,output,'video',op,engine,model,scale,fps,'--codec=libx264','--preset=ultrafast','--crf=0','--pixel-format=yuv444p','--no-audio',*flags])
    compare(name,title,input,output,detail)
print('DONE',len(RESULTS),flush=True)
