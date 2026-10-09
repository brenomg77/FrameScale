"""Shared paths for real-media regression scripts; never pick an old Qt Creator build."""
import argparse
import os
from pathlib import Path


def test_paths(description):
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument('--build', type=Path, default=root / 'build/Release')
    parser.add_argument('--qt-bin', type=Path, help='Qt bin directory if absent from PATH')
    args = parser.parse_args()
    build = args.build.resolve()
    if not (build / 'FrameScaleProcessingSmoke.exe').is_file():
        parser.error(f'Build framescale-tests first; executable not found in {build}')
    env = os.environ.copy()
    if args.qt_bin:
        env['PATH'] = str(args.qt_bin.resolve()) + os.pathsep + env.get('PATH', '')
    (root / 'build').mkdir(exist_ok=True)
    return build, env
