# ThorVision analytics SDK

Canonical source of the callback contract for prompt-driven analytics
extensions (`thorvision_sdk`, version 0.1.0, protocol 1). It has no
third-party runtime dependencies so it can run in the device supervisor,
isolated workers and PC tools alike. `ffmpeg`/`ffprobe` are needed only for
fixture bundles and replay.

| Module | Contents |
| --- | --- |
| `contract` | `FrameRef`, `Frame`, `Transform`, `FrameResult`, detections, track points and drawing primitives |
| `validate` | Strict result validation: 256 KiB, 256 objects, 4,096 drawing vertices, finite in-bounds coordinates |
| `render` | Renderer version 1 with the pinned 5x7 font in `font`; shared by live preview and replay |
| `tracking` | Reference IoU-association tracker with run-scoped IDs and time-bounded trails |
| `package` | Package manifest validation, content/package digests and SDK requirement checks |
| `bundle` | Recording bundle layout, analytics records, coverage states and tolerant JSONL reading |
| `replay` | `python -m thorvision_sdk.replay --bundle <dir> --output <video.mp4>` |
| `fixtures` | Deterministic moving numbered boxes and `python -m thorvision_sdk.fixtures --output <dir>` |
| `mkv` | Frame index (FrameRef, PTS, presentation order) from an actual Matroska recording, tolerant of truncation |
| `frameref` | Reads frame references embedded by the encoders (H.265 SEI, JPEG APP11); `python -m thorvision_sdk.frameref <file>` |
| `harness` | In-process runner for SDK tests and authoring checks (not an isolation boundary) |
| `schemas/` | JSON Schemas for TypeScript/C++ consumers; `schema_digest()` identifies a snapshot |

A callback package contains `manifest.json` (see `schemas/package-manifest.schema.json`),
its entrypoint module, optional configuration schema, `requirements.in`, model
declarations and fixtures. `package.seal(<dir>)` records the content digest;
any edit changes the package digest, which is what Apply binds to.

See `examples/moving_green_box` (drawing contract) and `examples/hog_person`
(OpenCV HOG people detector with the IoU tracker; its wheels are staged by
package preparation, not installed into the backend environment).

Run the tests from this directory:

```sh
python3 -m unittest discover -s tests -t .
```
