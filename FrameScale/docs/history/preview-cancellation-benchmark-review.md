# Preview, cancellation and GPU benchmark review

## Changes

- SwitchControl hit testing is limited to the painted switch track; keyboard activation remains available.
- Windows dialog caption hit testing includes the top and side layout margins, excluding the close button.
- Paused comparison frames are decoded separately but presented as a pair. The comparison timeline keeps the requested position while decoders catch up and preserves fractional frame steps.
- Comparison playback proxies use lossless H.264 colour encoding. Large video display copies still have a bounded resolution; paused comparison decoding remains bounded at 4096 pixels. The reduced-playback label was removed as requested.
- Preview progress uses dark text over both a light track and a light green fill.
- Model, filter and encoder field explanations were expanded, with larger help typography and spacing. English translations were added.
- Export cancellation can encode and validate a contiguous prefix from the final processing stage, or the completed frame count during a direct encode. It writes a unique `_parcial` file and preserves existing destinations. Cancellation before any frames reach the final requested processing stage has no completed result to save. Preview and benchmark cancellation do not retain files.
- Settings includes an explicit GPU benchmark with six profiles: ESRGAN/CUGAN at 4K60, 4K120 and 8K120. It exports a synthetic 1080p/24fps/0.25-second source and measures the entire job. One-minute times are labelled estimates, not guarantees. GPU names are detected; Vulkan adapter selection stays automatic.

## Validation

- Release build succeeded and the deployed executable SHA-256 matches the build.
- FrameScaleCoreTests and FrameScaleProcessingSmoke: 2/2 passed (27.54 seconds).
- `tests/validate-partial-export.py`: real interpolation and direct-encode cancellations produced decodable prefixes while preserving both an existing destination and an existing partial output. Report: `build/partial-export-blx0p9mf/report.json`.
- The supplied 1920x1080 source has 140 frames at 24000/1001 fps and approximately 5.84 seconds. A selected first two seconds exported at 120fps produced 241 frames and passed full decoding. Result: `build/interpolation-review-qiogxnxj/full120.mp4`.
- Cancellation on the same source recovered 50 complete frames at 1080p/120fps; an interrupted PNG was excluded from the contiguous prefix.
- Real 4K60 synthetic benchmark exports succeeded with both RealESRGAN AnimeVideo v3 and RealCUGAN SE. Other benchmark profiles were implemented but not run in this review.
- RIFE normal versus temporal TTA was compared on a 480x270 diagnostic sample. Original anchor frames were preserved exactly in both. Temporal TTA did not improve the measured edge consistency and was not enabled. Edge statistics are not a perceptual quality score.

## Limits

No UI tests were run, following the user's standing preference. Visual layout and native dragging still need interactive confirmation. Full decoding proves file integrity, not uniform visual quality: variation in generated RIFE frames has not been conclusively resolved. No claim is made that interpolation can preserve every original detail in newly generated frames.

## Subsequent user-requested refinements

The GPU benchmark dialog and Settings entry were removed. Tooltips now use 300-pixel cards, 12-pixel text, smaller padding and shorter explanations. Preview progress is blue again with white text. Cancellation uses the requested output filename without `_parcial`; numeric suffixes are used only to preserve existing files. The partial-export regression also checks the exact requested name when no collision exists.
