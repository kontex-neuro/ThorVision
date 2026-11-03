# Product

<!-- impeccable:product-schema 1 -->

## Platform

web

## Users

Scientists running animal experiments, for example recording a mouse, who need computer-vision analysis such as object tracking or behaviour recognition. They are experts in their research, not in software or computer vision. They work with any supported coding agent (Codex, Claude or Gemini, through the ThorVision desktop app, an IDE or Claude Code) beside a ThorVision camera.

## Product Purpose

The user describes an analysis task in plain language to their agent. The agent builds it as a camera extension through the ThorVision MCP tools, and the bridge tests it on the device. The user then runs it on a real camera from the human controls. Success: at any moment the user knows what to do next and how to ask for it, sees the live processed video, and can get the data out.

## Positioning

The analysis is written by the user's own agent from the scientist's words and runs on the camera device itself, on the live stream, beside recording. No CV programming, SSH or model training workflow is required of the user, and no particular agent is required.

## Operating Context

- Requests are made in the agent's own chat, never in an input box on a ThorVision surface.
- The browser view (`control-ui`, served by the bridge's control server) is the agent-neutral human surface. It opens from the link `open_control_panel` returns, either embedded in a narrow side panel next to the agent's chat (about 360–480 px) or as a full browser tab. Both are primary.
- The Claude Code mod (`plugins/thorvision-control`) is the native surface for Claude Code. The browser view follows its states, wording and order: camera and health, live preview, the next step, quiet camera controls, technical details.
- Outside Claude Code the page cannot type into the agent's message box, so task starters copy an editable example request for the user to paste.
- The device keeps the active and previous extension per camera. Changes are bound to exact revisions and reviewed in a separate confirmation before they run.

## Capabilities and Constraints

- Controls match the mod: Test on camera, Run on camera, Stop analysis, Go back to previous, each through the reviewed confirmation. Running an earlier analysis, storage cleanup and technical details stay behind disclosures.
- The page is served from the bridge's local control server with a bearer token passed in the URL fragment. It uses plain HTML, CSS and JavaScript with no build step.
- The page cannot know which agent is driving; a host may name it with the `agent` query parameter. Otherwise copy says "your agent".
- Device tests use fixture frames; they prove the extension runs, not that detection is accurate.
- Open: the analytics API exposes no recording status and no data export. A warning before a mid-recording switch and a "Get data" action need a device or recorder API that does not exist in this bridge yet.

## Evidence on Hand

- Real camera: KonteX-XC-U3 at the trusted-LAN analytics API.
- Real extensions: `yolo11n-bytetrack-person`, `thorvision-mod-smoke` and an agent-built `arena-mouse-tracker`. Do not show invented examples as results.

## Product Principles

1. Guide the next step. Every state tells the user what to do next and how to ask their agent for it.
2. Show the result, not the machinery. The live video and plain-language status come first; revisions, hashes and paths stay in Technical details.
3. Nothing changes silently. Stopped or degraded processing, stale video and failed tests are detected and stated.
4. The scientist's language. Tasks, animals, behaviours and data, not packages, digests or candidates.
5. Agent-neutral, reviewed changes only. Any agent can drive the work; the camera changes only after the user confirms the exact change.
