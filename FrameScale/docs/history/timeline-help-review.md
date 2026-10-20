# Timeline and help review

## Help policy

The custom help surface now suppresses text that merely repeats a visible button, label, selector item or menu action. This includes QAction's automatic fallback from tooltip to action text (the source of the General tooltip). Container-format help was removed. Technical field descriptions remain, and menu items receive help only when an explicit item description exists. Icon-only actions and full file paths retain useful help.

## Preview changes

- Comparison thumbnails come from the selected source time range, rather than repeatedly decoding upscaled 8K output. They remain navigation thumbnails, not quality comparisons.
- A shared non-UI frame reader decodes short sequential batches with an independently bounded 64 MiB cache per reader. PPM streaming delivers the requested image without waiting for PNG compression or batch completion. Both forward and backward neighbours are cached.
- Rapid requests supersede obsolete decoders. Frames are presented only for the latest request. The existing paired presentation still waits for both comparison frames.
- Scrubbing is enabled once metadata is available, independently of playback-proxy loading. Metadata arrival reissues the requested comparison seek.
- Decode resolution follows the visible comparison panel and zoom. A ready lossless playback copy is reused when it contains enough pixels for that view; larger zoom requests decode the full processed source. Exported data and reported dimensions are unchanged.
- Filmstrip extraction uses the same batched reader, with letterboxing preserved for non-widescreen media.

## Validation

Release build passed. No UI automation was run, following the user's preference.

`FrameScalePreviewDecodeCheck` uses QCoreApplication, compares returned pixels against an independent FFmpeg `select=eq(n,frame)` decode, and exercises forward/backward cached requests, rapid superseded requests, the last frame, and all 16 filmstrip images.

- Supplied 1080p source: passed; selected first-second thumbnails completed in 784 ms. Log: `build/preview-decode-source-final.log`.
- Generated 7680x4320/24fps source: passed; initial middle-frame request 480 ms, next frame 2 ms, previous cached frame 0 ms. Log: `build/preview-reader-check-8rzd3ie4/final-8k.log`.
- The comparison now generates its strip from the lower-resolution original; the direct 8K strip stress check also completed, in 2189 ms.

These are decoder measurements on this machine, not end-to-end UI timing guarantees. The running Qt Creator Debug executable predates this change; the updated Release executable is deployed to `build/FrameScale_UI/bin/FrameScale.exe`.
