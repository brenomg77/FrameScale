"""Verify real exports against independent pixel permutations, including audio and trim."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--build', type=Path, default=Path(__file__).resolve().parents[1] / 'build/Release')
parser.add_argument('--qt-bin', default='')
args = parser.parse_args()
build = args.build.resolve()
folder = Path(tempfile.mkdtemp(prefix='orientation-', dir=build))
env = os.environ.copy()
if args.qt_bin:
    env['PATH'] = args.qt_bin + os.pathsep + env.get('PATH', '')
ffmpeg = build / 'runtime/bin/ffmpeg.exe'
probe = build / 'runtime/bin/ffprobe.exe'
smoke = build / 'FrameScaleProcessingSmoke.exe'

def run(*command):
    result = subprocess.run(list(map(str, command)), env=env, capture_output=True, timeout=120)
    if result.returncode:
        raise AssertionError(result.stderr.decode(errors='replace') + result.stdout.decode(errors='replace'))
    return result.stdout

source = folder / 'source.mp4'
run(ffmpeg, '-v', 'error', '-f', 'lavfi', '-i', 'testsrc2=size=96x64:rate=4:duration=2',
    '-f', 'lavfi', '-i', 'sine=frequency=440:duration=2', '-c:v', 'libx264', '-crf', '0',
    '-pix_fmt', 'yuv444p', '-c:a', 'aac', '-shortest', source)

def pixels(path):
    return run(ffmpeg, '-v', 'error', '-i', path, '-frames:v', '1', '-pix_fmt', 'yuv444p', '-f', 'rawvideo', '-')

original = pixels(source)
for name, turns, horizontal, vertical in [('left', 3, 0, 0), ('right', 1, 0, 0),
        ('horizontal', 0, 1, 0), ('vertical', 0, 0, 1), ('combined', 1, 1, 1), ('half', 2, 0, 0)]:
    output = folder / (name + '.mp4')
    run(smoke, source, output, 'video', 'copy', 'esrgan', 'realesr-animevideov3', '1', '4',
        '--pixel-format=yuv444p', '--crf=0', '--quarter-turns=' + str(turns),
        '--flip-horizontal=' + str(horizontal), '--flip-vertical=' + str(vertical))
    width, height = (64, 96) if turns % 2 else (96, 64)
    expected = bytearray(len(original))
    for plane in range(3):
        offset = plane * 96 * 64
        for y in range(64):
            for x in range(96):
                tx, ty = x, y
                w, h = 96, 64
                for _ in range(turns):
                    tx, ty = h - 1 - ty, tx
                    w, h = h, w
                if horizontal:
                    tx = w - 1 - tx
                if vertical:
                    ty = h - 1 - ty
                expected[offset + ty * width + tx] = original[offset + y * 96 + x]
    assert pixels(output) == expected, name + ': exported pixels differ'
    streams = json.loads(run(probe, '-v', 'error', '-count_frames', '-show_streams', '-of', 'json', output))['streams']
    video = next(s for s in streams if s['codec_type'] == 'video')
    assert (video['width'], video['height'], int(video['nb_read_frames'])) == (width, height, 8)
    assert any(s['codec_type'] == 'audio' for s in streams)
    print(name + ': pixels, dimensions, frames and audio OK', flush=True)

tagged = folder / 'tagged.mp4'
run(ffmpeg, '-v', 'error', '-display_rotation:v:0', '90', '-i', source, '-c', 'copy', tagged)
output = folder / 'tagged-right.mp4'
run(smoke, tagged, output, 'video', 'copy', 'esrgan', 'realesr-animevideov3', '1', '4',
    '--pixel-format=yuv444p', '--crf=0', '--quarter-turns=1')
assert pixels(output) == original, 'source display rotation applied incorrectly'
print('source display rotation: OK', flush=True)

for suffix, extra in [('.mp4', []), ('.gif', []), ('', ['--format=.png'])]:
    output = folder / ('trimmed' + (suffix or '-sequence'))
    run(smoke, source, output, 'video', 'copy', 'esrgan', 'realesr-animevideov3', '1', '4',
        '--quarter-turns=1', '--trim-start=0.5', '--trim-end=1.5', *extra)
    target = output if suffix else sorted(output.glob('frame_*'))[0]
    streams = json.loads(run(probe, '-v', 'error', '-count_frames', '-show_streams', '-of', 'json', target))['streams']
    video = next(s for s in streams if s['codec_type'] == 'video')
    assert (video['width'], video['height']) == (64, 96)
    assert int(video['nb_read_frames']) == (4 if suffix else 1)
    if not suffix:
        assert len(list(output.glob('frame_*'))) == 4
    print('trimmed ' + (suffix or 'PNG sequence') + ': OK', flush=True)
print('Artifacts:', folder)
