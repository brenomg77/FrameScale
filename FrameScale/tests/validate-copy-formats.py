"""Exercise real trimmed clipboard encodes and verify their containers/codecs."""
import json
import pathlib
import subprocess
import sys
import tempfile

build = pathlib.Path(sys.argv[1]).resolve()
ffmpeg = build / 'runtime/bin/ffmpeg.exe'
ffprobe = build / 'runtime/bin/ffprobe.exe'
smoke = build / 'FrameScaleProcessingSmoke.exe'

def run(args):
    result = subprocess.run([str(x) for x in args], capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    return result.stdout

with tempfile.TemporaryDirectory(prefix='framescale-copy-formats-') as directory:
    root = pathlib.Path(directory)
    source = root / 'source.mp4'
    run([ffmpeg, '-v', 'error', '-y', '-f', 'lavfi', '-i',
         'testsrc2=size=96x64:rate=25', '-f', 'lavfi', '-i',
         'sine=frequency=440:sample_rate=48000', '-t', '1',
         '-c:v', 'libx264', '-pix_fmt', 'yuv420p', '-c:a', 'aac',
         '-timecode', '01:00:00:00', source])
    source_info = json.loads(run([ffprobe, '-v', 'error', '-show_streams',
                                  '-of', 'json', source]))
    assert any(s.get('codec_tag_string') == 'tmcd' for s in source_info['streams'])
    cases = {
        'mp4': ('mp4', 'h264', []),
        'mov': ('mov', 'h264', []),
        'mkv': ('matroska', 'h264', []),
        'avi': ('avi', 'mjpeg', ['--codec=mjpeg', '--pixel-format=yuvj420p', '--audio-codec=pcm_s16le']),
        'webm': ('webm', 'vp9', ['--codec=libvpx-vp9', '--audio-codec=libopus', '--audio-rate=48000']),
        'gif': ('gif', 'gif', ['--codec=gif', '--no-audio']),
    }
    for suffix, (container, codec, args) in cases.items():
        output = root / ('selection.' + suffix)
        run([smoke, source, output, 'video', 'copy', 'esrgan',
             'realesr-animevideov3', '--trim-start=0.2', '--trim-end=0.8', *args])
        info = json.loads(run([ffprobe, '-v', 'error', '-show_format',
                               '-show_streams', '-of', 'json', output]))
        assert container in info['format']['format_name'].split(','), info
        videos = [s for s in info['streams'] if s['codec_type'] == 'video']
        audio = [s for s in info['streams'] if s['codec_type'] == 'audio']
        assert len(videos) == 1 and videos[0]['codec_name'] == codec, info
        assert len(audio) == (0 if suffix == 'gif' else 1), info
        assert len(info['streams']) == len(videos) + len(audio), info
        assert abs(float(info['format']['duration']) - 0.6) < 0.09, info
        print(suffix + ': container, codec, audio and cut verified', flush=True)
