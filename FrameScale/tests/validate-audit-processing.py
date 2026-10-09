"""Real FFmpeg regressions for chapter mapping and video-only timing (PAP audit)."""
import argparse
from datetime import datetime
import json
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--qt-bin', type=Path, help='Qt bin directory when not already on PATH')
    parser.add_argument('--output', type=Path, help='Directory for synthetic fixtures and logs')
    args = parser.parse_args()
    build = args.build.resolve()
    project = Path(__file__).resolve().parents[1]
    output = (args.output or build / 'audit-processing' / datetime.now().strftime('%Y%m%d-%H%M%S')).resolve()
    output.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    if args.qt_bin:
        env['PATH'] = str(args.qt_bin.resolve()) + os.pathsep + env.get('PATH', '')
    suffix = '.exe' if os.name == 'nt' else ''
    smoke = build / ('FrameScaleProcessingSmoke' + suffix)
    runtime = build / 'runtime/bin'
    if not (runtime / ('ffmpeg' + suffix)).is_file():
        runtime = project / 'runtime/bin'
    ffmpeg, probe = [runtime / (name + suffix) for name in ('ffmpeg', 'ffprobe')]
    results = []

    def run(name, *command):
        result = subprocess.run(list(map(str, command)), capture_output=True, env=env,
                                cwd=project, timeout=120)
        (output / (name + '.stdout')).write_bytes(result.stdout)
        (output / (name + '.stderr')).write_bytes(result.stderr)
        if result.returncode:
            raise AssertionError(f'{name}: exit {result.returncode}\n'
                                 + result.stderr.decode(errors='replace')[-3000:])
        return result.stdout

    def inspect(name, path):
        return json.loads(run(name, probe, '-v', 'error', '-count_frames', '-show_streams',
                              '-show_chapters', '-show_format', '-of', 'json', path))

    def fixture(name, audio_duration, *, video_duration=2, vfr=False):
        path = output / (name + '.mkv')
        command = [ffmpeg, '-y', '-v', 'error', '-f', 'lavfi', '-i',
                   f'testsrc2=size=96x64:rate=4:duration={video_duration}',
                   '-f', 'lavfi', '-i', f'sine=frequency=440:duration={audio_duration}']
        if vfr:
            command += ['-vf', 'setpts=(N+floor(N/2))*0.25/TB', '-fps_mode', 'vfr']
        run('fixture-' + name, *command, '-c:v', 'libx264', '-pix_fmt', 'yuv420p',
            '-c:a', 'aac', path)
        metadata = inspect('source-' + name, path)
        video = next(s for s in metadata['streams'] if s['codec_type'] == 'video')
        assert 'duration' not in video or 'nb_frames' not in video, (
            name + ': fixture no longer exercises incomplete video timing headers')
        return path

    def export(name, source, *, frames=8, duration=2.0, audio=True, extra=()):
        destination = output / (name + '-output.mp4')
        run(name, smoke, source, destination, 'video', 'copy', 'esrgan',
            'realesr-animevideov3', '1', '4', '--overwrite', *extra)
        metadata = inspect(name + '-probe', destination)
        videos = [s for s in metadata['streams'] if s['codec_type'] == 'video']
        audios = [s for s in metadata['streams'] if s['codec_type'] == 'audio']
        assert len(videos) == 1 and len(audios) == int(audio), name + ': wrong stream counts'
        assert len(metadata['streams']) == 1 + int(audio), name + ': unexpected stream'
        assert not metadata.get('chapters'), name + ': chapters must be explicitly omitted'
        assert int(videos[0]['nb_read_frames']) == frames, name + ': incorrect frame count'
        actual_duration = float(videos[0]['duration'])
        assert abs(actual_duration - duration) < 0.01, (name, actual_duration, duration)
        rate_num, rate_den = map(int, videos[0]['avg_frame_rate'].split('/'))
        assert abs(rate_num / rate_den - frames / duration) < 0.001, name + ': incorrect cadence'
        if audios:
            assert 0 < float(audios[0]['duration']) <= duration + 0.06, name + ': audio extends video'
        run(name + '-decode', ffmpeg, '-v', 'error', '-xerror', '-i', destination,
            '-map', '0:v:0', '-map', '0:a?', '-f', 'null', '-')
        results.append({'name': name, 'passed': True, 'frames': frames, 'duration': actual_duration})
        print(name + ': OK', flush=True)

    try:
        long_audio = fixture('long-audio', 3)
        export('long-audio', long_audio)
        export('long-audio-trim', long_audio, frames=4, duration=1,
               extra=('--trim-start=0.5', '--trim-end=1.5'))
        export('long-audio-muted', long_audio, audio=False, extra=('--no-audio',))
        short_audio = fixture('short-audio', 0.75)
        export('short-audio', short_audio)
        one_frame = fixture('single-frame', 1, video_duration=0.25)
        export('single-frame', one_frame, frames=1, duration=0.25)
        vfr = fixture('vfr-long-audio', 4, vfr=True)
        export('vfr-long-audio', vfr, duration=2.75)

        base = output / 'chapter-base.mp4'
        run('fixture-chapter-base', ffmpeg, '-y', '-v', 'error', '-i', long_audio,
            '-map', '0:v:0', '-map', '0:a:0', '-t', '2', '-c', 'copy', base)
        chapters = output / 'chapters.ffmeta'
        chapters.write_text(';FFMETADATA1\n[CHAPTER]\nTIMEBASE=1/1000\nSTART=0\nEND=1000\n'
                            'title=First\n[CHAPTER]\nTIMEBASE=1/1000\nSTART=1000\nEND=2000\n'
                            'title=Second\n', encoding='utf-8')
        source = output / 'chapters.mp4'
        run('fixture-chapters', ffmpeg, '-y', '-v', 'error', '-i', base, '-i', chapters,
            '-map_metadata', '1', '-map_chapters', '1', '-c', 'copy', source)
        assert len(inspect('source-chapters', source)['chapters']) == 2
        export('chapters', source)
        export('chapters-trim', source, frames=4, duration=1,
               extra=('--trim-start=0.5', '--trim-end=1.5'))
        export('chapters-no-metadata', source, extra=('--no-metadata',))
        # Audio-only output follows the same explicit chapter policy.
        audio_path = output / 'chapters-audio.mp3'
        run('chapters-audio', smoke, source, audio_path, 'video', 'copy', 'esrgan',
            'realesr-animevideov3', '1', '4', '--overwrite', '--format=.mp3')
        metadata = inspect('chapters-audio-probe', audio_path)
        assert len(metadata['streams']) == 1 and metadata['streams'][0]['codec_type'] == 'audio'
        assert not metadata.get('chapters')
        results.append({'name': 'chapters-audio', 'passed': True})
        print('chapters-audio: OK', flush=True)
    except Exception as error:
        results.append({'passed': False, 'error': str(error)})
        raise
    finally:
        (output / 'summary.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
        print(f'Evidence: {output}', flush=True)
    print(f'{len(results)}/{len(results)} processing regressions passed.', flush=True)


if __name__ == '__main__':
    main()
