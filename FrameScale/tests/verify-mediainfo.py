"""Check actual encoder settings using the user's MediaInfo library (Windows).

Example: python tests/verify-mediainfo.py output.mp4 --crf 23 --title Example
The DLL is a test dependency only; it is not bundled with FrameScale.
"""
import argparse
import ctypes
import json
from pathlib import Path
import re

parser = argparse.ArgumentParser()
parser.add_argument("video", type=Path)
parser.add_argument("--library", type=Path, default=Path("C:/Program Files/MediaInfo/MediaInfo.dll"))
parser.add_argument("--crf", type=int)
parser.add_argument("--no-crf", action="store_true")
parser.add_argument("--title")
parser.add_argument("--comment")
parser.add_argument("--report", type=Path)
args = parser.parse_args()
lib = ctypes.WinDLL(str(args.library.resolve()))
lib.MediaInfo_New.restype = ctypes.c_void_p
lib.MediaInfo_Open.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p]
lib.MediaInfo_Open.restype = ctypes.c_size_t
lib.MediaInfo_Option.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_wchar_p]
lib.MediaInfo_Option.restype = ctypes.c_wchar_p
lib.MediaInfo_Inform.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
lib.MediaInfo_Inform.restype = ctypes.c_wchar_p
lib.MediaInfo_Delete.argtypes = [ctypes.c_void_p]
handle = lib.MediaInfo_New()
try:
    lib.MediaInfo_Option(handle, "Output", "JSON")
    if not lib.MediaInfo_Open(handle, str(args.video.resolve())):
        raise SystemExit("MediaInfo could not open the output")
    report = json.loads(lib.MediaInfo_Inform(handle, 0))
finally:
    lib.MediaInfo_Delete(handle)
tracks = report["media"]["track"]
general = next(t for t in tracks if t["@type"] == "General")
video = next(t for t in tracks if t["@type"] == "Video")
tags = {k.lower(): v for k, v in {**general, **general.get("extra", {})}.items()}
settings = video.get("Encoded_Library_Settings", "")
errors = []
if args.crf is not None:
    # A custom container tag alone is not enough: verify the codec's own SEI.
    if not re.search(r"(?:^| / )crf=" + str(args.crf) + r"(?:\.0+)?(?: / |$)", settings):
        errors.append("CRF missing/wrong in MediaInfo Encoding settings: " + settings)
    if str(tags.get("encoding_crf")) != str(args.crf):
        errors.append("New CRF missing/wrong in container metadata")
if args.no_crf and (tags.get("crf") or tags.get("encoding_crf") or "crf=" in settings):
    errors.append("Codec without CRF inherited stale quality metadata")
for field in ("title", "comment"):
    expected = getattr(args, field)
    if expected is not None and tags.get(field) != expected:
        errors.append(f"Source {field} was not preserved")
if args.report:
    args.report.write_text(json.dumps({"passed": not errors, "errors": errors, "mediainfo": report}, indent=2), encoding="utf-8")
if errors:
    raise SystemExit("\n".join(errors))
print(f"PASS {args.video.name}: MediaInfo settings and source metadata verified")
