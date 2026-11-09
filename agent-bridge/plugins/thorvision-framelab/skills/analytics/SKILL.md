---
name: analytics
description: Build or refine real ThorVision camera analytics using the device SDK and MCP tools. Preparation includes dependency reuse, validation and testing automatically.
---

Use the ThorVision MCP tools in this session. Inspect `get_sdk_contract`, `get_device_capabilities`, `read_project` and saved reports before changing the callback.

The user describes behavior, not infrastructure. Preserve original frame references, per-camera state and stream restart handling. Use real inference and tracking when requested; report missing dependencies instead of simulating detections. Declare dependencies and model checksums in the manifest. Never install or download inside a frame callback.

After the final edit, call `prepare_extension`. It validates the candidate, reuses compatible prepared dependencies, downloads through the temporary PC proxy if required, and tests this exact revision. Do not ask the user to request reuse, validation, testing or SSH login. The developer pre-deploys and maintains the service; all user operations use the device API. If it is unavailable, report the API error for the developer instead of deploying the service or falling back to SSH. Do not repeat failed preparation without addressing its reported cause. Do not delete packages or raise limits to make preparation pass.

Report measured test results and any limitation: fixture startup tests do not establish real-world detection quality. The mod automatically checks preparation after your turn as a fallback.

The human opens `/framelab`, reviews the exact candidate and camera, then clicks **Run on camera**. The panel links to continuous processed video and storage controls. Activation, stopping, rollback and deletion require human controls: do not call those HTTP actions through shell or other tools. Never claim a candidate is running merely because preparation passed.
