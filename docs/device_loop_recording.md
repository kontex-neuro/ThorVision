# Device SSD loop recording (feature build)

This client and the matching Pi backend record up to four cameras to an external SSD independently of the PC. Closing or disconnecting the client does not stop device recording. The SSD panel and each preview distinguish **SSD REC** from **PC REC**. A disconnected client reports device status as **Unknown**.

## Prepare the SSD (temporary user-manual instructions)

1. Back up footage or other files you need from the intended external SSD.
2. Connect it to a PC. On Windows, open File Explorer, identify that external drive carefully, right-click it and choose **Format**. Select **exFAT** and complete formatting. Formatting erases that volume. On macOS use Disk Utility and choose exFAT for the intended external recording volume.
3. Safely eject it from the PC, then connect it to the device's designated recording USB connection.
4. Open ThorVision and wait for the SSD status to become **Ready**. **Port not configured** means the hardware connection has not yet been commissioned; an engineer must complete the matching backend's `recording/README.md` procedure.

No formatting command exists in ThorVision. Keep system storage separate. Desktop mounts must be safely unmounted before the device can claim the recording SSD.

## First use and client controls

Before any client setup, the device selects native H.265 1920x1080 at 30 FPS. It tries native MJPEG at the same workload if H.265 is unavailable. Otherwise that camera shows **Configuration required**. Select an advertised codec/resolution/FPS combination and apply it. No bitrate setting is added.

The first four enrolled cameras are selected for local recording. The capture/display system can retain up to eight cameras; deselect one local destination before enabling a fifth. The selection and workload are saved on the device and survive client closure and device restart. Cameras without serial numbers must return to the same USB connection with the same model.

The SSD block in the top row shows SSD status, which selected cameras are recording, estimated retention, free space and the current issue. **SSD details…** adds the last event, storage breakdown, per-camera state and profile, segment length and the eject, resume and check actions. The default file length is 60 seconds. H.265 cuts follow keyframes, so actual duration can differ. Estimates use measured data growth and available recorder space; they are not a retention guarantee.

Codec and resolution/FPS changes are staged until **Apply camera settings** is confirmed. The warning explains that the source is shared by PC and SSD recording. The client finalizes the affected PC recording, the device finalizes its affected SSD file, then the new source must produce frames before its configuration is saved. A failed source start restores the prior workload. PC recording resumes in a new file once its stream is ready; other cameras continue. Changing the global SSD file length starts new SSD files for all selected cameras.

Device files are MKV with the original encoded camera metadata. Rotation deletes the oldest completed recorder-owned files across selected cameras, preserving at least 10% free space with a 2 GB minimum plus working headroom. Unrelated files, active files and files undergoing recovery are excluded. Five seconds without write progress and rolling recording loss of at least 5% are faults reported independently of preview.

## Disconnects and recovery

- A camera interruption preserves the other streams. Reconnecting its verified identity starts a fresh recording file automatically. A changed model requires explicitly applied source settings.
- An SSD interruption stops local writers while capture/PC streams continue. On return, the designated connection, filesystem, UUID/attachment identity and mount are validated before new writers start. There is no fallback onto system storage.
- A bounded background pass attempts to remux interrupted originals. Original files remain until normal recorder rotation; unwritten frames cannot be restored. No RAM buffer or redundant recording is promised.
- Use **Safely eject SSD** before removal. It finalizes local writers and unmounts the volume. Use **Resume** to continue or reconnect it. **Check SSD** pauses local writers for an unmounted read-only check.
- Limited **Repair SSD** remains disabled until the filesystem-repair POC and physical interruption gates are approved. Repair can change damaged filesystem entries, requires explicit confirmation and rechecks before recording. Use PC repair for unsupported damage.

## Recording policy choices

| Policy | Startup behavior | Tradeoff |
| --- | --- | --- |
| Factory auto-recording (implemented) | Start the default supported workload when eligible SSD and cameras appear | No PC dependency; installer must verify the workload and SSD connection |
| Saved device configuration (implemented) | Resume device-persisted selections and source profiles | Predictable operation after client disconnect or reboot |
| Client-only arming (alternative) | Wait for the PC application to enable recording | Explicit operator control, but no recording before client setup |

The implemented device-owned policy supports an unattended CCTV-style continuous loop. Compared with a dedicated NVR product, this version concentrates on native USB camera capture and embedded XDAQ metadata. It does not add motion/event recording rules, remote retention services, RAID, redundant media or gap filling. SSD recovery and workload qualification therefore remain explicit acceptance tasks.

## Build matching components

Use feature branches `feat/device-loop-recording` in xvc and libxvc, paired with `feat/pi5-loop-recording` in videoserver. The library package is `libxvc/0.5.0-dev`. Export that package locally with the normal Conan workflow, then install the client dependencies and build normally. No repository push or package publication is needed.

For source development, configure the client with `-DTHORVISION_LIBXVC_SOURCE=<absolute-libxvc-worktree>`. This compiles the matching library directly and needs its private dependencies (including cpr/CURL) available to CMake as well as the normal Qt, GStreamer and xdaqmetadata dependencies. Existing Conan generators can be reused only when their compiler/profile/dependency versions match. This implementation was built with MSVC 19.40, Qt 6.8 and the installed GStreamer MSVC SDK.

`THORVISION_DEVICE_HOST` and `THORVISION_DEVICE_PORT` override the endpoint (defaults 192.168.177.100:8000). For this Pi's LAN, set the host to 192.168.50.206. SRT ports are assigned by the backend. Put the matching GStreamer bin before Qt bin on PATH; duplicate FFmpeg DLL names from Qt can otherwise prevent the GStreamer software decoder from loading. `THORVISION_SOFTWARE_PREVIEW=1` is an optional Windows diagnostic fallback when hardware decoding is unavailable.

The built Windows executable is `build/Release/ThorVision.exe`. The Pi must run the matching feature backend from its worktree with its recording environment and private runtime. Builds and isolated test fixtures do not replace installed services automatically.

## Verification

Build target `recording-client-smoke` and run it against `recording.tools.test_server` on an isolated port to exercise the compiled library/API contract. For the real UI and PC recording integration:

1. On the Pi, run `uvicorn recording.tools.synthetic_server:app --host 0.0.0.0 --port 18803` from the matching backend environment. This uses four synthetic cameras and scratch media, with real native encoder/writer processes and SRT.
2. Build the client with `-DTHORVISION_ENABLE_UI_SMOKE=ON` (default OFF). Set the correct GStreamer/Qt runtime PATH and GST_PLUGIN_PATH.
3. Run `python thorvision/tests/recording_ui_smoke.py build/Release/ThorVision.exe --host 192.168.50.206` with FFmpeg available. It records all four PC streams, changes one source from 30 to 15 FPS through the real controller, checks resumption and decodes all five completed files. Peer cameras must remain in one file each.

The smoke build also accepts `THORVISION_UI_SMOKE=1` to skip update discovery and `THORVISION_UI_SCREENSHOT=<file.png>` for a timed UI capture. These hooks are compiled out of normal builds.

Software verification passed on Windows and Pi: native H.265/MJPEG four-writer segmentation/playback, mixed-codec camera/SSD interruption recovery, metadata round-trip, API revision and repair gates, PC source-change coordination, and existing display tests. Device filesystem repair was exercised only with disposable exFAT images. Hardware acceptance remains pending: actual recording USB connection, real cameras/SSD, 1-4 camera workload, four-hour maximum-load run, power interruptions, full/read-only media and repeated physical replug. The repair gate stays off until those results are accepted.

## UI refresh and memory regression

Background status reads do not disable the recording/source controls. A user operation that arrives during a read is queued and runs after that read, using the refreshed revision or SSD attachment token. Foreground actions and device storage work still disable conflicting controls. Camera rows persist across status updates, and unchanged source profiles do not emit selection-change notifications.

Connection animations have an icon-sized render limit and stop when hidden. In the previous build, large GIFs continued rendering behind the connected state and reproduced multi-gigabyte memory growth even without preview. This was separate from the polling flicker.

For the Windows regression check, build with `THORVISION_ENABLE_UI_SMOKE=ON`, prepare the matching Qt/GStreamer runtime PATH and GST_PLUGIN_PATH, then run:

```powershell
python thorvision/tests/recording_refresh_smoke.py build/Release/ThorVision.exe --name refresh-test --seconds 120 --no-preview --test-operation
python thorvision/tests/recording_refresh_smoke.py build/Release/ThorVision.exe --name live-preview-test --seconds 180 --host 192.168.50.206
```

The isolated fixture checks four persistent camera rows, no background busy toggles, an operation queued behind a slow status request, bounded object counts and process memory. The live variant only reads device state and attaches a preview receiver. Logs and memory samples are saved in the current directory. Short soak checks catch this regression; they do not replace extended workload qualification. Normal release builds keep these test hooks disabled.
