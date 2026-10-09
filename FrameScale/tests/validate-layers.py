"""Nonvisual real FFmpeg regressions: speed frame counts and independent layers."""
import argparse
import array
import json
import math
import os
from pathlib import Path
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument('--build', type=Path, default=Path('build/Desktop_Qt_6_11_2_MinGW_64_bit_Debug'))
args = p.parse_args()
build = args.build.resolve()
folder = Path(tempfile.mkdtemp(prefix='layers-check-', dir=build))
env = os.environ.copy()
env['PATH'] = 'C:/Qt/6.11.2/mingw_64/bin;C:/Qt/Tools/mingw1310_64/bin;' + env.get('PATH', '')
ffmpeg = build/'runtime/bin/ffmpeg.exe'
probe = build/'runtime/bin/ffprobe.exe'
smoke = build/'FrameScaleProcessingSmoke.exe'

def run(*cmd, success=True):
    result = subprocess.run(list(map(str, cmd)), env=env, capture_output=True, timeout=120)
    if (result.returncode == 0) != success:
        raise AssertionError(result.stderr.decode(errors='replace') + result.stdout.decode(errors='replace'))
    return result.stdout

def streams(path):
    return json.loads(run(probe, '-v', 'error', '-count_frames', '-show_streams', '-of', 'json', path))['streams']

def export(name, source, flags, layers=None, suffix='.mp4'):
    output = folder/(name + suffix)
    if layers is not None:
        manifest = folder/(name + '.json')
        manifest.write_text(json.dumps(layers), encoding='utf-8')
        flags = ['--layers=' + str(manifest), *flags]
    run(smoke, source, output, 'video', 'copy', 'esrgan', 'realesr-animevideov3',
        '1', '60', '--preset=ultrafast', '--crf=0', *flags)
    return output

source = folder/'source.mp4'
run(ffmpeg, '-v', 'error', '-f', 'lavfi', '-i', 'testsrc2=size=96x64:rate=60',
    '-f', 'lavfi', '-i', 'sine=frequency=440:sample_rate=48000', '-t', str(2300/60),
    '-c:v', 'libx264', '-preset', 'ultrafast', '-crf', '0', '-c:a', 'aac', source)
for speed in (2, 4, .5):
    output = export('speed-' + str(speed), source, ['--speed=' + str(speed)])
    video = next(s for s in streams(output) if s['codec_type'] == 'video')
    assert int(video['nb_read_frames']) == math.ceil(2300/speed), video
    print('legacy speed', speed, ': exact frame count OK', flush=True)

fractional = folder/'fractional.mp4'
run(ffmpeg, '-v', 'error', '-f', 'lavfi', '-i', 'testsrc2=size=96x64:rate=60000/1001',
    '-frames:v', '2299', '-c:v', 'libx264', '-preset', 'ultrafast', fractional)
output = export('fractional-speed-2', fractional, ['--speed=2'])
assert int(streams(output)[0]['nb_read_frames']) == 1150
print('fractional cadence, odd source frame count at 2x: 1150 frames OK', flush=True)
output = export('fractional-layer-speed-2', fractional, [], [dict(path=str(fractional), kind='video',
    duration=2299*1001/60000, fps=60000/1001, width=96, height=64, speed=2)])
assert int(streams(output)[0]['nb_read_frames']) == 1150
print('fractional layer at 2x: 1150 frames OK', flush=True)

red = folder/'red.mp4'
blue = folder/'blue.mp4'
tone = folder/'tone.wav'
for path, color in [(red, 'red'), (blue, 'blue')]:
    run(ffmpeg, '-v', 'error', '-f', 'lavfi', '-i', 'color=c=' + color + ':s=96x64:r=30:d=2',
        '-c:v', 'libx264', '-preset', 'ultrafast', '-crf', '0', path)
run(ffmpeg, '-v', 'error', '-f', 'lavfi', '-i', 'sine=frequency=440:sample_rate=48000:duration=2', tone)

def layer(path, kind, **kwargs):
    return dict(path=str(path), kind=kind, duration=2, fps=30, width=96, height=64, **kwargs)

def color_at(path, seconds):
    raw = run(ffmpeg, '-v', 'error', '-ss', str(seconds), '-i', path, '-frames:v', '1',
              '-vf', 'scale=1:1', '-pix_fmt', 'rgb24', '-f', 'rawvideo', '-')
    return tuple(raw[:3])

def audio_rms(path, seconds):
    raw = run(ffmpeg, '-v', 'error', '-ss', str(seconds), '-i', path, '-t', '0.15',
              '-vn', '-ac', '1', '-ar', '48000', '-f', 'f32le', '-')
    values = array.array('f', raw)
    return math.sqrt(sum(v*v for v in values)/max(1,len(values)))

output = export('video-only-speed', red, [], [layer(red,'video',speed=2),layer(tone,'audio')])
assert color_at(output,.5)[0] > 200
assert max(color_at(output,1.5)) < 8
assert audio_rms(output,1.5) > .04, 'video speed incorrectly sped audio'
assert int(next(s for s in streams(output) if s['codec_type']=='video')['nb_read_frames']) == 60
print('video speed affects only video; longer audio retained: OK', flush=True)
output = export('audio-only-speed', red, [], [layer(red,'video'),layer(tone,'audio',speed=2)])
assert color_at(output,1.5)[0] > 200
assert audio_rms(output,.5) > .04
assert audio_rms(output,1.5) < .005
print('audio speed affects only audio; full video retained: OK', flush=True)
output = export('stack-and-trim', red, [], [layer(blue,'video',start=.5,**{'in':.5,'out':1.5}),layer(red,'video'),layer(tone,'audio',start=.5,**{'in':.5,'out':1.5})])
assert color_at(output,.2)[0] > 200
assert color_at(output,.8)[2] > 200
assert color_at(output,1.8)[0] > 200
assert audio_rms(output,.1) < .005 and audio_rms(output,.8) > .04 and audio_rms(output,1.8) < .005
print('video stacking, independent trim and delayed audio: OK', flush=True)
output = export('deleted-audio', red, [], [layer(red,'video')])
assert not any(s['codec_type']=='audio' for s in streams(output))
output = export('deleted-video', red, [], [layer(tone,'audio')])
assert max(color_at(output,.5)) < 8 and audio_rms(output,.5) > .04
print('deleted layers absent from export: OK', flush=True)
empty = folder/'empty.json'; empty.write_text('[]')
run(smoke, red, folder/'empty.mp4', 'video', 'copy', 'esrgan', 'realesr-animevideov3', '--layers='+str(empty), success=False)
print('empty composition rejected: OK', flush=True)
output = export('layers-gif', red, [], [layer(red,'video',speed=2)], suffix='.gif')
assert int(streams(output)[0]['nb_read_frames']) == 30
output = export('layers-audio', red, ['--format=.wav'], [layer(tone,'audio',speed=2)], suffix='.wav')
audio = streams(output)
assert len(audio) == 1 and audio[0]['codec_type'] == 'audio'
assert abs(float(audio[0]['duration'])-1) < .03
output = export('layers-png', red, ['--format=.png'], [layer(red,'video',speed=2)], suffix='')
assert len(list(output.glob('frame_*.png'))) == 30
print('layer composition GIF, audio-only and PNG sequence exports: OK', flush=True)
sparse = folder/'sparse.mka'
run(ffmpeg, '-v', 'error', '-f', 'lavfi', '-i', 'sine=frequency=440:sample_rate=48000:duration=2',
    '-af', 'asetpts=2*PTS', '-c:a', 'pcm_s16le', sparse)
output = export('sparse-audio', red, [], [dict(path=str(sparse),kind='audio',duration=4,audioTimeScale=2,fps=30,width=96,height=64)])
assert all(audio_rms(output,t) > .04 for t in (.5,1.5,2.5,3.5)), 'sparse audio timestamps collapsed and silenced the second half'
print('sparse audio timestamps preserve sound near the end: OK', flush=True)
output = export('video-gaps', red, [], [layer(red,'video',start=.5,**{'in':0,'out':1}),layer(tone,'audio')])
assert max(color_at(output,.2)) < 8 and color_at(output,.8)[0] > 200 and max(color_at(output,1.8)) < 8
print('empty timeline intervals contain no retained video frame: OK', flush=True)
output = export('work-area', red, ['--trim-start=.5','--trim-end=1.5'], [layer(red,'video'),layer(tone,'audio')])
v=next(s for s in streams(output) if s['codec_type']=='video')
assert int(v['nb_read_frames'])==30, v
assert audio_rms(output,.5)>.04
print('export work area: exactly one second of video and audio OK',flush=True)
output=export('hidden-video',red,[],[layer(red,'video',enabled=False),layer(tone,'audio')])
assert max(color_at(output,.5))<8 and audio_rms(output,.5)>.04
output=export('muted-audio',red,[],[layer(red,'video'),layer(tone,'audio',enabled=False)])
assert color_at(output,.5)[0]>200 and not any(s['codec_type']=='audio' for s in streams(output))
print('per-layer eye and mute preserve the other track: OK',flush=True)
print('Artifacts:', folder)
