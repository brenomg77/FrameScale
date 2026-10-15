#!/usr/bin/env python3
"""Fetch native Linux engines and their matching models into the build tree."""
import hashlib
import json
import shutil
import stat
import subprocess
import urllib.request
import zipfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]
cache = root / 'build/linux-downloads'
cache.mkdir(parents=True, exist_ok=True)
bin_dir = root / 'runtime/bin'
bin_dir.mkdir(parents=True, exist_ok=True)
records = []
packages = [
    ('realesrgan-ncnn-vulkan', 'xinntao/Real-ESRGAN-ncnn-vulkan', 'v0.2.0', 'realesrgan-ncnn-vulkan-v0.2.0-ubuntu.zip'),
    ('realcugan-ncnn-vulkan', 'nihui/realcugan-ncnn-vulkan', '20220728', 'realcugan-ncnn-vulkan-20220728-ubuntu.zip'),
    ('rife-ncnn-vulkan', 'nihui/rife-ncnn-vulkan', '20221029', 'rife-ncnn-vulkan-20221029-ubuntu.zip'),
]
for engine, repo, tag, filename in packages:
    url = f'https://github.com/{repo}/releases/download/{tag}/{filename}'
    archive = cache / filename
    urllib.request.urlretrieve(url, archive)
    destination = cache / engine
    destination.mkdir(exist_ok=True)
    with zipfile.ZipFile(archive) as z:
        z.extractall(destination)
    executable = next(p for p in destination.rglob(engine) if p.is_file())
    shutil.copy2(executable, bin_dir / engine)
    (bin_dir / engine).chmod(0o755)
    model_root = root / 'models' / {'realesrgan-ncnn-vulkan': 'realesrgan', 'realcugan-ncnn-vulkan': 'realcugan', 'rife-ncnn-vulkan': 'rife'}[engine]
    model_root.mkdir(parents=True, exist_ok=True)
    assets = executable.parent
    expected = 'models' if engine == 'realesrgan-ncnn-vulkan' else 'models-se' if engine == 'realcugan-ncnn-vulkan' else 'rife-v4.6'
    if not (assets / expected).is_dir():
        # The runner release omits weights; the upstream model distribution
        # provides the platform-independent NCNN weights separately.
        model_url = url.replace('-ubuntu.zip', '-windows.zip')
        if engine == 'realesrgan-ncnn-vulkan':
            model_url = 'https://github.com/xinntao/Real-ESRGAN/releases/download/v0.2.5.0/realesrgan-ncnn-vulkan-20220424-ubuntu.zip'
        model_archive = cache / filename.replace('-ubuntu.zip', '-weights.zip')
        urllib.request.urlretrieve(model_url, model_archive)
        model_assets = cache / (engine + '-weights')
        with zipfile.ZipFile(model_archive) as z:
            for entry in z.infolist():
                if entry.filename.endswith(('.param', '.bin')):
                    z.extract(entry, model_assets)
        assets = next(p for p in model_assets.rglob(expected) if p.is_dir()).parent
        records.append(dict(id=engine+'-weights', version='20220424' if engine=='realesrgan-ncnn-vulkan' else tag, source=model_url, sha256=hashlib.sha256(model_archive.read_bytes()).hexdigest(), license='See bundled model licenses'))
    if engine == 'realesrgan-ncnn-vulkan':
        shutil.copytree(assets / 'models', model_root, dirs_exist_ok=True)
    elif engine == 'realcugan-ncnn-vulkan':
        for directory in assets.glob('models-*'):
            shutil.copytree(directory, model_root / directory.name, dirs_exist_ok=True)
    else:
        directory = assets / 'rife-v4.6'
        shutil.copytree(directory, model_root / directory.name, dirs_exist_ok=True)
    records.append(dict(id=engine, version=tag, source=url, sha256=hashlib.sha256(archive.read_bytes()).hexdigest(), license='MIT'))
for name in ['ffmpeg', 'ffprobe', 'mpv']:
    path = shutil.which(name)
    if not path:
        raise RuntimeError(f'Missing Linux dependency: {name}')
    shutil.copy2(path, bin_dir / name)
    records.append(dict(id=name, source='Ubuntu 24.04 distribution packages', version=subprocess.check_output(['dpkg-query', '-W', '-f=${Version}', 'ffmpeg' if name=='ffprobe' else name], text=True), license='See bundled third-party notices and Ubuntu copyright files'))
for name, url, version in [
    ('yt-dlp', 'https://github.com/yt-dlp/yt-dlp/releases/download/2026.08.19/yt-dlp_linux', '2026.08.19'),
    ('deno', 'https://github.com/denoland/deno/releases/download/v2.9.7/deno-x86_64-unknown-linux-gnu.zip', 'v2.9.7'),
]:
    archive = cache / (name + ('.zip' if name == 'deno' else ''))
    urllib.request.urlretrieve(url, archive)
    if name == 'deno':
        with zipfile.ZipFile(archive) as z:
            (bin_dir/name).write_bytes(z.read('deno'))
    else:
        shutil.copy2(archive, bin_dir/name)
    (bin_dir/name).chmod(0o755)
    records.append(dict(id=name, version=version, source=url, sha256=hashlib.sha256(archive.read_bytes()).hexdigest(), license='See bundled third-party notices'))
artifacts = [dict(path=p.relative_to(root).as_posix(), sha256=hashlib.sha256(p.read_bytes()).hexdigest())
             for directory in (bin_dir, root/'models') for p in sorted(directory.rglob('*')) if p.is_file()]
manifest = dict(schemaVersion=2, platform='linux-x86_64', packages=records, artifacts=artifacts)
(root/'runtime/manifest.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
# The shaders are platform independent and tracked alongside the sources.
assert (root/'models/libplacebo/anime4k-v4-a.glsl').is_file()
for binary in bin_dir.iterdir():
    if binary.is_file() and binary.suffix != '.exe':
        binary.chmod(binary.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
