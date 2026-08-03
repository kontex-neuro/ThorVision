# Server Update — UI/UX Design Plan

Scope: **device-server updates only.** Client self-update is explicitly out of scope
(flowchart nodes `M`, `N`, `P` are not built). Local/offline artifact selection is deferred;
a failed download is terminal for this iteration.

Authority order: `xdaqvc/update/*.h` headers > `docs/decisions/` ADRs >
`update_integration_guide.md` > this document.

---

## 0. Decisions this plan is built on

| # | Decision | Consequence |
| --- | --- | --- |
| 1 | **Ignore is always available**, even when `plan->required` | Version check must stop gating camera population — see §1 |
| 2 | Download unavailable/failed = **update failure**, no local-file fallback | One error path, no file picker |
| 3 | No client self-update | No background appcast polling, no client-version UI |
| 4 | Device *should* reconnect after update, but failure is handled | Explicit `Restarting…` phase with timeout — see §5 |
| 5 | Expandable progress detail | Collapsed bar + `Show details` disclosure over the log feed |
| 6 | Extend `AlertDialog`, don't fork it | New multi-button footer; `UpdateDialog` composes the shell |

**The load-bearing change is #1.** Everything else is additive UI; #1 alters startup behavior.

---

## 1. Unblocking "Ignore" (behavioral prerequisite)

Today [`main.cc:125-127`](../thorvision/src/main.cc#L125-L127) returns early when the version
check fails, so cameras are never added and the default config is never loaded. Ignoring an
update currently yields a dead application. To honor decision #1:

```
status_change(true)
  ├─ populate cameras + load default config   ← ALWAYS, unconditionally
  └─ evaluate update plan (async)             ← advisory; drives UI only
```

`Server::check_api_version()` is replaced. The exact-equality test against `Version(0,1,1)`
is wrong under the new policy — the API is **series**-based (`0.1.x`) and a downgrade from
`0.2.0` to `0.1.5` is a normal, expected outcome, not an error. Equality would report a
perfectly compatible `0.1.5` device as mismatched.

New surface on `Server` (or a dedicated `UpdateController`, see §7):

| Member | Purpose |
| --- | --- |
| `Q_PROPERTY updateState` | enum, drives all UI (§3) |
| `Q_PROPERTY targetVersion`, `deviceVersion` | display strings |
| `Q_PROPERTY changelog` | markdown, rendered via `Text.MarkdownText` |
| `Q_PROPERTY required` | styles the dialog; does **not** remove Ignore |
| `Q_PROPERTY progress`, `progressIndeterminate` | `total == 0` ⇒ indeterminate |
| `Q_PROPERTY updateDismissed` | set by Ignore; suppresses re-prompt this session |

**Re-prompt policy.** `status_change(true)` fires on every reconnect, so without guarding, a
user who ignored an update gets re-prompted on every network blip. `updateDismissed` latches
for the session; the persistent indicator (§4) remains the standing reminder.

---

## 2. Where the update lives in the existing layout

The top bar already ends with `XDAQStatus` (110px), the natural home for device state.

```
┌────────────┬───────┬─────────┬──────────────┬────────┬─────────────┐
│ CameraList │ Count │ spacer  │ RecordSetting│ Record │ XDAQStatus  │
└────────────┴───────┴─────────┴──────────────┴────────┴─────────────┘
                                                        └─ update badge
```

No new top-level real estate. The update entry point is a **badge on `XDAQStatus`**, which
already owns connected/connecting visuals via its `StackLayout`.

---

## 3. State machine

One enum drives dialog, badge, and menu. Named to match `UpdateAction`/`ErrorCode`.

| State | Trigger | UI |
| --- | --- | --- |
| `Idle` | no device / not yet checked | nothing |
| `Checking` | connected, plan resolving | badge spinner only, **no dialog** |
| `UpToDate` | `action == None` | nothing |
| `UpdateAvailable` | `action == Update` | Offer dialog (§4) |
| `Downloading` | user confirmed | Progress dialog, determinate or indeterminate |
| `Handshaking` | download OK | Progress, indeterminate |
| `Uploading` | transfer running | Progress, determinate |
| `Restarting` | transfer returned OK | Progress, indeterminate + reconnect watch |
| `Succeeded` | reconnect, version now compatible | Success dialog |
| `Failed` | any `ErrorCode` except `Cancelled` | Failure dialog (§6) |
| `Dismissed` | Ignore | badge only |

`Checking` never shows a dialog — a modal that appears unbidden on every launch before the
app knows whether anything is wrong would be worse than the problem it solves.

**`Succeeded` is driven by the return value of `transfer_file_and_apply_update`, never by the
final progress tick** — the guide warns that queued callbacks may arrive after the function
returns.

---

## 4. The screens

### 4a. Offer dialog — `UpdateAvailable`

```
┌─ ⚠ ThorVision Server Update ────────────────────────────┐
│                                                          │
│   A server update is required for this device.           │
│   Current 0.2.0  →  Update to 0.1.5                      │
│                                                          │
│   ┌ What's changed ──────────────────────────────┐ ▲     │
│   │ (markdown changelog, scrollable)             │ █     │
│   │                                              │ ▼     │
│   └──────────────────────────────────────────────┘       │
│   Keep the device connected during the update.           │
│                                                          │
│                      [ Ignore ]      [ Update Now ]      │
└──────────────────────────────────────────────────────────┘
```

- **Version pair is always shown both ways.** Because downgrades are normal, "Update to
  0.1.5" from `0.2.0` looks like an error unless the current version sits next to it.
  Wording stays **"Update"** in both directions — the API models it as one action, and
  exposing "downgrade" invites the user to second-guess a correct decision.
- **Required vs optional** differs by *tone and default*, not by capability:
  - required → warning icon, accent border, `Update Now` is the default/focused button,
    body reads "is required for this device".
  - optional → info icon, neutral border, `Ignore` is default, body reads "is available".
- `Ignore` is always present and Esc-dismissible. Per decision #1.
- Changelog is `Text.MarkdownText` inside a `ScrollView`; the pane is **fixed-height and
  scrolls**, so a long changelog can never push the buttons off the 534px shell.

### 4b. Progress dialog — `Downloading` → `Restarting`

```
┌─ ThorVision Server Update ──────────────────────────────┐
│                                                          │
│   Installing update to 0.1.5                             │
│   Step 3 of 4 — Uploading to device                      │
│                                                          │
│   ████████████████████░░░░░░░░░░░░  62%                  │
│                                                          │
│   ⚠ Do not disconnect the device or close this window.   │
│                                                          │
│   ▸ Show details                                         │
│                      [ Cancel ]                          │
└──────────────────────────────────────────────────────────┘
```

Expanded, `Show details` reveals a monospace log pane (`stream_update_logs`), auto-scrolling
to the tail, using `popup_scroll_text`. The pane is capped (~500 lines, ring buffer) so a
chatty device can't grow the model unbounded.

- **`progressIndeterminate` binds to `total === 0`** — never divide by it.
- **Cancel is only enabled during `Downloading`.** Once bytes are landing on the device,
  cancelling risks a half-written image; the button becomes disabled with hint text
  "Cannot cancel while installing." Cancel sets the `std::atomic<bool>` flag; the resulting
  `Cancelled` error **shows no error dialog** — the user already knows.
- **The dialog is non-closable here.** `closePolicy: Popup.NoAutoClose`, and
  `ApplicationWindow.onClosing` must also refuse to quit mid-update, exactly as it already
  refuses to quit mid-recording.

### 4c. Success

Auto-dismisses after confirming reconnect + compatible version. Brief confirmation with the
now-current version; single `Done` button.

---

## 5. The reconnect problem (decision #4)

The device restarts mid-update. `Server::status_change(false)` will fire and, as written,
trip the *"The connection to XDAQ has been lost"* dialog at
[`main.qml:196-207`](../thorvision/ui/main.qml#L196-L207) — stacking a scary, wrong error on
top of a normal update.

**Both existing `Connections` blocks in `main.qml` must be gated** on
`Server.updateState !== Restarting` (and the in-flight states generally). This is a real
regression risk and is called out as its own task in §8.

`Restarting` runs a bounded watch:

- reconnect within timeout, version compatible → `Succeeded`
- reconnect, version *still* incompatible → `Failed`, "The update did not apply."
- **no reconnect before timeout → `Failed`**, with recovery text: the device may still be
  booting; power-cycle it and restart the app. This is the failure case decision #4 asks for,
  and it must not hang forever on a spinner.

---

## 6. Errors

Branch on `error.code`; **never parse `error.message`.** Per the guide's table, mapped to
three response shapes:

| Shape | Codes | Buttons |
| --- | --- | --- |
| Retryable | `NetworkUnreachable`, `HttpError`, `SizeMismatch`, `HandshakeRejected` | `Retry`, `Ignore` |
| Not auto-retryable | `HashMismatch` | `Close` + "try again later; if it persists, contact support" |
| Escalate | `VersionNotFound`, `PlatformNotFound`, `MalformedResponse`, `TransferRejected` | `Close` + support link |
| Device rollback | `DeviceUpdateFailed` | `Retry`, `Quit` — device self-restored |
| Silent | `Cancelled` | *no dialog* |

**`HashMismatch` must never auto-retry** — it is security-relevant (corruption or tampering),
and it is deliberately distinct from `SizeMismatch`, which is a transport blip worth retrying.
`PlatformNotFound` must be logged loudly and never rendered as "up to date"; it is a
deployment bug masquerading as a benign state.

Reuse the existing support-contact treatment (mail + ticket links) already written for the
mismatch dialog — that content is good and should survive the rewrite.

---

## 7. Component work

**New**

| File | Role |
| --- | --- |
| `ui/UpdateDialog.qml` | State-driven shell; composes `AlertDialog` |
| `ui/UpdateProgress.qml` | Bar + phase label + disclosure |
| `ui/UpdateLogView.qml` | Capped auto-scrolling log pane |
| `ui/UpdateBadge.qml` | Overlay for `XDAQStatus` |
| `src/UpdateController.{h,cc}` | Owns libxvc calls, threading, state |

**Modified**

| File | Change |
| --- | --- |
| `ui/AlertDialog.qml` | Footer becomes a multi-button slot (§6 needs two, current shell allows one) |
| `ui/XDAQStatus.qml` | Host the badge |
| `ui/Menu.qml` | "Check for Device Updates…" — the manual path back after Ignore |
| `ui/main.qml` | Gate the two `Connections` blocks; drop the dead mismatch dialog |
| `src/Server.{h,cc}` | Remove `check_api_version` equality test |
| `src/main.cc` | Decouple camera population from version check |
| `ui/Colour.qml` | Add `progress_track`, `progress_fill`, `success` — none exist today |

`UpdateController` is a separate `QObject` rather than more surface on `Server`: `Server` is
a connection monitor, and updates carry their own thread, cancellation flag, and multi-step
state. **Every libxvc callback fires on a worker thread** and must marshal via
`QMetaObject::invokeMethod(..., Qt::QueuedConnection)`; the cancel flag is
`std::atomic<bool>`. Touching QML-visible properties from a worker "usually appears to work
under test and fails in the field."

---

## 8. Build order

1. **Decouple version check from camera population** (§1) — unblocks Ignore. Behavioral.
2. **Gate the existing disconnect/mismatch dialogs** (§5) — prevents the wrong-error regression.
3. `UpdateController` skeleton + state enum, fake data source — lets all UI be built and
   demoed without hardware.
4. `AlertDialog` multi-button footer.
5. Offer dialog + badge + menu entry.
6. Progress dialog, log view, cancel.
7. Error mapping (§6).
8. Wire to real libxvc; exercise reconnect and timeout paths.

Steps 3–7 are hardware-independent by design. The guide flags the device HTTP paths as
**unverified against real hardware** and the macOS SHA-256 branch as **never built** — step 8
is where that risk lands, and the expandable log pane (§4b) exists largely to make it
debuggable.

---

## 9. Open items

- **Where does the cloud base URL come from?** Hardcoded, `Config`, or user-visible setting?
  Affects whether Settings needs a field.
- **`REQUIRED_SERIES` is a compile-time constant** the app declares. Confirm `{0, 1}` and
  that bumping it is a release-checklist item — it is not derivable from libxvc's version.
- **Re-check cadence** — this plan checks on connect only, plus manual menu invocation.
