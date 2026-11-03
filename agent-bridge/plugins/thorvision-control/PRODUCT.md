# Product

<!-- impeccable:product-schema 1 -->

## Platform

web

## Users

Scientists running animal experiments, for example recording a mouse, who need computer-vision analysis such as object tracking or behaviour recognition. They are experts in their research, not in software or computer vision. They work in Claude Code (desktop app first; the terminal is also supported) beside a ThorVision camera.

## Product Purpose

The user describes an analysis task in plain language in Claude Code. Claude builds it as a camera extension, tests it on the device, and the user runs it on a real camera. Success: at any moment the user knows what to do next and how to ask for it, sees an instant processed preview of the result, and can get the data out.

## Positioning

The analysis is written by Claude from the scientist's own words and runs on the camera device itself, on the live stream, beside recording. No CV programming, SSH or model training workflow is required of the user.

## Operating Context

- The authoring surface is Claude Code's own chat. Requests are made there, never in a separate input box in the panel.
- The `/thorvision` panel sits beside the chat. It explains what to do next, offers task starters that put an editable example request into Claude's message box, shows the live processed preview, and holds the reviewed camera controls.
- The device keeps the active and previous extension per camera; changes are bound to exact revisions and reviewed before they run.

## Capabilities and Constraints

- Panel surfaces: Claude Code desktop (native Box/Text/Button/Link/Svg elements; images only through Svg up to 131072 characters) and the terminal (text; no inline video).
- `$.prompt.fill` can place a task starter in the message box; `$.http.fetch` returns text only, so an in-panel preview needs the bridge to serve frames as text (base64).
- Device tests use fixture frames; they prove the extension runs, not that detection is accurate.
- Open: the analytics API exposes no recording status and no data export. Warning before a mid-recording switch and a "get data" action need a device or recorder API that does not exist in this bridge yet.

## Evidence on Hand

- Real camera: KonteX-XC-U3 at the trusted-LAN analytics API.
- Real extensions: `yolo11n-bytetrack-person` (person tracking) and `thorvision-mod-smoke`. No mouse tracking or behaviour model exists yet; do not show invented examples as results.

## Product Principles

1. Guide the next step. Every state tells the user what to do next and how to ask for it in Claude.
2. Show the result, not the machinery. The preview and plain-language status come first; revisions, hashes and paths stay in Technical details.
3. Nothing changes silently. Mid-recording switches, lost versions, stopped or degraded processing and stale video are detected and stated.
4. The scientist's language. Tasks, animals, behaviours and data, not packages, digests or candidates.
5. Reviewed changes only. The camera changes only after the user confirms the exact change.
