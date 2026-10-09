# FrameScale runtime

`bin/` contains the executables actually used by FrameScale:

- `ffmpeg.exe`, `ffprobe.exe`: decode, filters, encode and validate media;
- `realesrgan-ncnn-vulkan.exe`, `realcugan-ncnn-vulkan.exe`: upscale;
- `rife-ncnn-vulkan.exe`: frame interpolation;
- `mpv.exe`: Anime4K shader processing through the offscreen `gpu` filter;
- `yt-dlp.exe`, `deno.exe`: online video extraction;
- `vcomp140.dll`, `d3dcompiler_43.dll`: Microsoft runtime dependencies of those tools.

Models and shaders are in `../models/`. Download archives and extracted upstream
packages are staging caches, not application dependencies, and are not packaged.

## Integrity checks

From the FrameScale source root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/verify-runtime.ps1
```

To verify a deployed package, use `-AppRoot <installation>/bin`. The check verifies
each binary, model and shader against its SHA-256 and size, rejects missing or
unlisted files, and requires the included license texts. The installer script
runs it against both the source tree and the freshly deployed payload. It also
compares the deployed FrameScale executable with the Release build.

`manifest.json` schema 2 distinguishes package/archive provenance from individual
file hashes. `packages[].sha256` refers to the downloaded archive or executable
at `source`; `artifacts[].sha256` refers to the actual installed file, with paths
relative to the source root or installed `bin/` directory. Legacy archive hashes
are retained as historical records: archives no longer present were not all
downloaded again. File hashes record the audited local snapshot and do not prove
its upstream authenticity. The yt-dlp executable hash was additionally matched
to its official GitHub release digest. Model entries explicitly retain unresolved
archive provenance instead of inventing a download URL.

When deliberately changing a runtime or model, obtain it from its upstream,
check the release digest/signature, update its package record and file hashes,
and rerun the media tests before building an installer. The validator does not
silently accept or regenerate hashes for changed files. FrameScale does not
download or execute an installer at startup.

## Local installer builds

`scripts/build-installer.ps1` creates a unique `payload-<id>` directory below
`-PackageDir` (default `build/installer`) for every invocation. Existing staging
contents cannot enter the new installer. Staging is removed on completion unless
`-KeepStaging` is supplied. A new setup replaces the current `dist/` setup only
after successful compilation; a previous setup and checksum are archived under
`dist/archive/<id>/`. No package is published online by this script.

## Distribution records

The source license texts are `runtime/LICENSE-*.txt`; a deployed package places
them in `bin/runtime/licenses/`. See `THIRD_PARTY_NOTICES.md` for the components,
upstream sources and outstanding corresponding-source/redistribution records.
An integrity check or a successful installer build does not fulfill license
conditions on its own.
