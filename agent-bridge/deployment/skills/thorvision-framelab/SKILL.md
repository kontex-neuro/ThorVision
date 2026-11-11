---
name: thorvision-framelab
description: Build or refine ThorVision FrameLab camera processing, detection, tracking and overlays with the configured MCP tools. Preparation automatically reuses verified dependencies, validates and tests on the device.
---

Use the workspace's FrameLab MCP tools, normally registered as `framelab`; the legacy `thorvision` registration exposes the same tools. Use one existing server, not a second registration. Inspect `get_sdk_contract`, `get_device_capabilities`, `read_project` and saved reports before editing. If tools are missing, read [references/setup.md](references/setup.md). An unavailable device API belongs to the developer deployment owner; do not request device SSH credentials or deploy services as part of camera-processing authoring.

Users describe behavior. Preserve original frame references, camera-specific tracker state, timestamps and stream-restart resets. Use real inference when requested; report missing dependencies instead of simulated detections. Declare required packages and verified model sources/checksums through the advertised SDK contract. Never download or install inside a frame callback. Keep existing `thorvision_sdk` imports and wire/schema identities; FrameLab is the feature name.

After the final edit, call `prepare_extension`. It validates the exact candidate, reuses compatible verified dependencies, downloads through the temporary PC proxy when necessary, and runs the device test. Users need not ask for reuse, validation or testing separately. Inspect failures before retrying. Do not delete packages, raise limits or overwrite passing identities to overcome a preparation failure.

Report the tested package digest, actual results and measurements. Fixture execution does not establish real-world detection accuracy. A prepared package may still be inactive.

Call `open_control_panel` and give its session-local browser URL to the user. The shared FrameLab WebUI provides preview and reviewed Run on camera, Stop, Restore and cleanup controls. Activation and deletion belong to those human controls; do not bypass them with HTTP mutations through another tool. Keep the owning agent/MCP session open while the panel is in use. A new session can return a new URL. This workflow works with the configured native agent; Claude and the ThorVision desktop application are optional.

For explicitly requested digital I/O control, use `get_digital_io` and `set_digital_outputs` on the selected device. Read fresh capabilities and valid masks first. Protocol4 is the box-numbered eight-channel map; protocol1 is the older four-channel profile. Do not assume historical prototype channel maps. Bit0 means channel1, 1 means HIGH and 0 means LOW. Output readback is a latch rather than connector voltage. Writes use one revision-checked request; report conflicts and uncertain completion without automatically retrying. Stale channels are unknown. These tools do not grant callbacks hardware/network access or bypass the human camera-package activation controls.
