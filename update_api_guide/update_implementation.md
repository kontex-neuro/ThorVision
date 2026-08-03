# Device-Server Update — Implementation Notes

How the update feature is wired into the Qt/QML application, why it is shaped this way, and
which constraints will silently reintroduce bugs if broken.

Scope: **device-server updates only.** Client self-update is not implemented. Offline/local
artifact selection is not implemented — a failed download is terminal.

Companion documents: `update_integration_guide.md` (the libxvc API contract) and
`ui_ux_design_plan.md` (the original UI design). Where this document and the plan disagree,
this one reflects what was actually built and field-tested.

---

## 1. Components

| File | Role |
| --- | --- |
| `src/UpdateController.{h,cc}` | State machine, worker thread, libxvc calls, error mapping |
| `ui/UpdateDialog.qml` | State-driven dialog: offer / progress / success / failure |
| `ui/UpdateProgress.qml` | Phase label, progress bar, elapsed timer, details disclosure |
| `ui/UpdateLogView.qml` | Capped auto-scrolling device log pane |
| `ui/UpdateBadge.qml` | Pending-update indicator on `XDAQStatus` |

Modified: `src/main.cc` (sequencing), `src/Server.{h,cc}` (removed the old version check),
`src/WebSocketClient.{h,cc}` (reconnect support), `ui/AlertDialog.qml` (multi-button footer),
`ui/main.qml`, `ui/Menu.qml`, `ui/VideoLayout.qml`, `ui/XDAQStatus.qml`, `ui/Colour.qml`.

`UpdateController` is separate from `Server` deliberately: `Server` is a connection monitor,
while an update owns a worker thread, a cancellation flag and a multi-step lifecycle.

---

## 2. Configuration

All in the anonymous namespace at the top of `UpdateController.cc`:

| Constant | Value | Notes |
| --- | --- | --- |
| `CLOUD_BASE_URL` | `dl-thorvision-firmware.kontex.io` | Update artifact host |
| `REQUIRED_SERIES` | `{0, 1}` | The device-server `major.minor` this client speaks |
| `DEVICE_HOST` | `192.168.177.100` | Ports default to 8000 (server) / 8001 (update) |
| `MAX_LOG_LINES` | 500 | Ring buffer for the device log pane |
| `RESTART_TIMEOUT` | 180 s | Give-up point waiting for the device after an update |
| `RESTART_POLL_INTERVAL` | 2 s | Version poll cadence during restart |

**`REQUIRED_SERIES` is a release-checklist item.** It is not derivable from libxvc's version
(an independent line) and nothing will warn you when it goes stale.

---

## 3. State machine

```
Idle ──► Checking ──┬─► UpToDate
                    ├─► UpdateAvailable ──┬─► Dismissed          (Ignore)
                    │                     └─► Downloading ──► Handshaking
                    │                              │               │
                    │                              └── Uploading ──┴─► Restarting
                    │                                       │
                    │                                       ├─► Succeeded
                    │                                       └─► Failed
                    └─► Failed                             (check failed)
```

Two predicates drive almost everything, and **conflating them is a bug that has already
happened once**:

- **`streams_blocked()`** — true for `Idle`, `Checking`, `UpdateAvailable` and all
  in-progress states. Gates *camera bring-up*.
- **`device_restarting()`** — true only for in-progress states. Gates *WebSocket hotplug
  events*.

Using `streams_blocked()` for hotplug events froze the camera list permanently: after a
disconnect the state returns to `Idle`, where it is `true`, so every add/remove was silently
discarded.

---

## 4. Startup sequencing — the load-bearing part

On `Server::status_change(true)` the app does **one** thing: ask `UpdateController` to check.
No camera enumeration, no config load, no pipelines.

```
status_change(true) ──► UpdateController::on_device_connected()
                                    │
                          (question resolves, any outcome)
                                    ▼
                        streams_released() ──► reconnect WebSocket (if device restarted)
                                           ──► clear + re-enumerate cameras
                                           ──► load default config (starts pipelines)
```

**Why:** applying the default config sets `cap`/`codec` on each camera, and those setters
start a live GStreamer pipeline as a side effect (`CameraItem::set_cap` / `set_codec`). A
successful update restarts the device server, which would strand any pipeline built against
the old one — the original symptom was an orphaned pipeline retrying SRT forever.

**A failed check still releases streams.** An unreachable update service must never brick a
local recording session.

`release_streams()` is latched by `_released` so it fires once per resolution.
`arm_restart_after_update()` clears the latch and **must be called before the state
transition** — `set_state()` releases as soon as the state stops being blocked, so re-arming
afterwards fires a second time and builds the camera list twice, once against a
still-restarting device.

---

## 5. Phases: why there are three, not four

`transfer_file_and_apply_update()` uploads, applies **and reboots the device** inside one
blocking libxvc call. The client cannot observe the boundaries.

Measured on real hardware:

```
Uploading      47 s   (the whole opaque call)
Restarting     <1 s   (version check + camera-list settle, after the call returned)
```

`Uploading` and `Restarting` therefore share **step 3 of 3**. Numbering them separately
advertised a transition that could not be reported: step 3 ran for most of a minute and step
4 flashed past in under a second, which read as "stuck on 3/4".

Step 3's label is *"Installing on device and restarting"* — during most of that minute
nothing is uploading. `UpdateProgress.qml` shows a per-phase elapsed timer because an
indeterminate bar sitting still for 47 s reads as a hang.

Genuine per-phase progress would require a libxvc API change; its progress callback covers
only the transfer portion.

---

## 6. Cancellation

Allowed through `Downloading` and `Handshaking`; **not** during `Uploading` — aborting once
bytes are landing on the device risks a half-written image.

The flag is `std::atomic<bool>` (GUI writes, worker reads). Returning `false` from the
progress callback cancels the transfer, but `perform_handshake()` and `prepare_transfer()`
take no callback, so the worker checks `_cancelled` explicitly between those steps —
otherwise a cancel during `Handshaking` sat unhonoured until upload began, exactly when it is
no longer permitted.

The Cancel button stays *enabled* and dims to 0.45 opacity when unavailable: a disabled Qt
`Button` does not report `hovered`, so the explanatory tooltip could never appear. The click
is gated in the handler instead.

`ErrorCode::Cancelled` shows **no** error dialog.

---

## 7. Threading

Every libxvc callback fires on a worker thread and is marshalled back with
`QMetaObject::invokeMethod(..., Qt::QueuedConnection)`.

- `total == 0` (no `Content-Length`) ⇒ indeterminate bar. Never divide by it.
- "Finished" comes from the **return value**, never the last progress tick — queued delivery
  means a tick can arrive after the call returns.
- `Camera::cameras()` is a blocking HTTP call. `wait_for_stable_camera_list()` runs it on the
  worker only.

Because state transitions are queued, a log timestamp for a state change is when the **GUI
thread** processed it, not when the worker requested it. This matters when reading logs: an
apparent phase gap is usually GUI-thread latency, not a stalled worker.

---

## 8. Device restart consequences

A restart invalidates more than the version number.

**Camera IDs are reassigned.** Removal looks up by id (`CameraModel::index_of_camera_id`), so
a stale id makes every hotplug event miss. When a removal arrives for an unknown id the
handler clears the model and re-enumerates rather than logging and giving up.

**The WebSocket connection dies and does not recover.** `xvc::ws_client` connects once at
construction and exposes no reconnect handle, so `WebSocketClient::reconnect()` destroys and
recreates it. Without this, *no* hotplug event arrives after an update until the app is
restarted. Triggered only when `device_did_restart()`.

**`/cameras` can 404 immediately after a restart**, so the post-release enumeration retries
5 × 200 ms before accepting an empty list.

**The disconnect dialogs must stay suppressed.** `main.qml` gates its `Server` handlers on
`Update.in_progress`; without it the reboot trips *"The connection to XDAQ has been lost"* on
top of a normal update. `ApplicationWindow.onClosing` also refuses to quit mid-update.

---

## 9. Errors

Branch on `error.code`, never on `error.message`.

| Recovery | Codes | Buttons |
| --- | --- | --- |
| `Retryable` | `NetworkUnreachable`, `HttpError`, `SizeMismatch`, `HandshakeRejected` | Retry, Close |
| `Rollback` | `DeviceUpdateFailed` | Retry, Close |
| `Terminal` | `HashMismatch`, `MalformedResponse`, `PlatformNotFound`, `VersionNotFound`, `ReleaseDeprecated`, `DowngradeRejected`, `FilesystemError`, `TransferRejected` | Close + support links |
| silent | `Cancelled` | *no dialog* |

`HashMismatch` is **never auto-retried** — on a correctly-sized file it indicates corruption
or tampering, deliberately distinct from `SizeMismatch`. `PlatformNotFound` and
`VersionNotFound` are logged at `critical`: they are deployment bugs and must not be
presented as "up to date".

There is no `Qt.quit()` anywhere in the update UI. Closing the app is the user's decision
even after a rollback; the badge remains as the standing reminder.

---

## 10. Ignore

Always available, including for `required` updates, per an explicit product decision. This
overrides the libxvc guide's statement that `required == true` is blocking.

Consequences:
- Camera bring-up cannot be gated on the version check (§4).
- `_dismissed` latches for the session so a flapping connection cannot re-prompt.
- A persistent badge on `XDAQStatus` remains, and Help ▸ *Check for Device Updates…* is the
  way back.
- The app may run against a device whose protocol it does not support. Downstream failures
  will be confusing; the badge is the only on-screen explanation.

An update offer is also skipped entirely while `Recorder.recording` is true. The prompt is
**not** startup-only: `Server` polls and re-emits `status_change` on every reconnect, so an
offer can arrive mid-session.

---

## 11. Known issues, not fixed here

- **`device_id` missing from WebSocket camera payloads.** `Camera::parse()` requires it and
  returns null; `CameraModel::add_camera()` dereferences without checking, which was
  undefined behaviour. The handler now null-checks and logs, but the camera still cannot be
  added — this is a device-server/libxvc contract problem, present before this work.
- **`/cameras` returning 404** has been observed on a settled device, not only during
  restart. The retry masks it; the endpoint is worth verifying against the 0.1.2 server.
- **No file log sink.** Every bug in this feature was diagnosed from a console attached via
  `WIN32_EXECUTABLE FALSE`. Release builds discard that output entirely.
- **Untested paths:** rollback (`DeviceUpdateFailed`), cancellation, the restart timeout, and
  the `HashMismatch` branch have never executed against hardware.
