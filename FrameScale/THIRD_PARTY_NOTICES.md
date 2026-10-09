# Third-party notices

FrameScale 0.2.0 uses the following third-party components. The versions below
describe the bundled files, not the latest upstream versions. The source tree
keeps license texts as `runtime/LICENSE-*.txt`; deployed packages put these in
`bin/runtime/licenses/`. Component licenses do not assign a license to FrameScale's
own source code or replace the terms for bundled dependencies.

| Component | Bundled version / identification | License and upstream |
| --- | --- | --- |
| Qt | 6.11.2, dynamically linked modules | LGPL-3.0 / applicable module terms; [Qt obligations](https://www.qt.io/development/open-source-lgpl-obligations), [Qt 6.11.2 source archive](https://download.qt.io/archive/qt/6.11/6.11.2/) |
| FFmpeg / ffprobe | `n8.1.2-51-g7ba069f4f1-20260909`, BtbN Windows GPL build | GPL-3.0-or-later for this build (`--enable-gpl --enable-version3`); [FFmpeg legal page](https://ffmpeg.org/legal.html), [build scripts](https://github.com/BtbN/FFmpeg-Builds), [FFmpeg source](https://github.com/FFmpeg/FFmpeg/tree/7ba069f4f1) |
| Real-ESRGAN ncnn Vulkan | 0.2.0 | MIT; [source/release](https://github.com/xinntao/Real-ESRGAN-ncnn-vulkan/tree/v0.2.0) |
| Real-CUGAN ncnn Vulkan | 20220728 | MIT; [source/release](https://github.com/nihui/realcugan-ncnn-vulkan/tree/20220728) |
| RIFE ncnn Vulkan | 20221029 | MIT; [source/release](https://github.com/nihui/rife-ncnn-vulkan/tree/20221029) |
| ncnn | Used by the three inference executables above | BSD-3-Clause and included third-party notices; [source](https://github.com/Tencent/ncnn) |
| mpv | `v0.41.0-1023-g69e63f425`, shinchiro build dated 2026-09-03 | GPL-2.0-or-later; [source](https://github.com/mpv-player/mpv/tree/69e63f425a), [build scripts](https://github.com/shinchiro/mpv-winbuild-cmake), [binary release](https://github.com/shinchiro/mpv-winbuild-cmake/releases/tag/20260903) |
| libplacebo | mpv reports `v7.371.0 (v7.360.0-120-g86bbd5d-dirty)` | LGPL-2.1-or-later; [source](https://code.videolan.org/videolan/libplacebo) |
| yt-dlp | 2026.08.19, Windows x64 PyInstaller executable | The project is Unlicense; the bundled executable is GPL-3.0-or-later with third-party components; [release and source](https://github.com/yt-dlp/yt-dlp/releases/tag/2026.08.19), [licensing explanation](https://github.com/yt-dlp/yt-dlp/tree/2026.08.19#licensing) |
| Deno | 2.9.7 x86_64-pc-windows-msvc | MIT for Deno; its dependencies retain their own licenses; [source/release](https://github.com/denoland/deno/releases/tag/v2.9.7) |
| Anime4K shaders | Bundled V4 / V4.1 shader variants | MIT, bloc97 and contributors; license retained in every shader; [source](https://github.com/bloc97/Anime4K) |
| RIFE models | Bundled `rife-v4.6` | MIT, Megvii Inc.; [upstream model project](https://github.com/hzwer/ECCV2022-RIFE) |
| Real-ESRGAN models | Bundled image and anime-video models | BSD-3-Clause, Xintao Wang and contributors; [upstream](https://github.com/xinntao/Real-ESRGAN) |
| Real-CUGAN models | Bundled SE, PRO and NOSE models | MIT, bilibili; [upstream](https://github.com/bilibili/ailab/tree/main/Real-CUGAN) |
| `vcomp140.dll` | 14.31.31103.0, Microsoft Corporation | Microsoft software terms, **not** the inference tools' MIT license; [redistribution documentation](https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files?view=msvc-170) |
| `d3dcompiler_43.dll` | File version 10.0.18362.1, Microsoft Corporation, supplied with mpv | Microsoft software terms, **not** mpv's GPL license; [DirectX SDK/runtime documentation](https://learn.microsoft.com/en-us/windows/win32/directx-sdk--august-2009-) |

`runtime/manifest.json` identifies the packages, known archive digests, local
binary/model hashes and provenance limits. Its former `FFmpeg 8.1` label has been
replaced by the precise version printed by the binary. The original FFmpeg
download uses a mutable `latest` URL; it is a historical origin record, not a
promise that downloading that URL today yields the same build.

## Corresponding source and distribution status

The included notices and binary hashes support identification and integrity.
This source snapshot does **not** contain a complete corresponding-source bundle
for every GPL/LGPL binary dependency, and does **not** make a written source offer
on behalf of the distributor. Upstream repository URLs alone do not establish
that obligation has been fulfilled.

Before redistributing a compiled package, assemble the exact source, patches,
dependency revisions and build/install instructions required by each applicable
license, and provide them using the distribution mechanism that license permits.
For FFmpeg and mpv this includes their enabled bundled libraries and the actual
build scripts/revisions. The Qt DLLs remain separately replaceable; include the
corresponding Qt source and applicable notices/replacement rights. For the
yt-dlp executable include its bundled dependencies, not only the Unlicense
application source. The local `LICENSE-yt-dlp-third-party.txt` records upstream's
component notices and source contact; it is not a new offer from FrameScale.

The exact source/build correspondence of the historical FFmpeg/mpv distributions,
Deno's complete dependency notices and the applicable Microsoft redistribution
terms/entitlement still need to be recorded for a public release. The model
inventory records local hashes and upstream project references; an original
archive/revision was not established for every model/shader during this repair.
These are outstanding release records, not hidden behind a successful build or
an invented license certificate.
