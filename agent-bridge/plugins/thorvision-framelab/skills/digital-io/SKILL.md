---
name: digital-io
description: Read ThorVision digital inputs or perform direct operator-requested masked digital-output actions, including holding and stopping an external OUT2 LED, independently of camera callback authoring.
---

Follow the matching [direct digital I/O guide](references/digital-io.md). Use `get_digital_io` and `set_digital_outputs` from the existing selected FrameLab MCP connection. No SSH, direct SPI, second publisher or second MCP registration.

This is an operator control route: do not create/edit a camera project, call `prepare_extension`, or activate an analysis. Keep the user's request natural. Read authoritative fresh compatibility/status before a write, preserve other outputs and require matched acknowledgment. Never replay an uncertain HIGH automatically. A later explicit Stop is a masked LOW action for the confirmed prior operator target, not a camera stop or a HIGH retry.

Digital OUT2 is the external TTL connector, distinct from camera-status RGB LED2/GPIO20. Confirm the current box mapping and report FPGA latch confirmation separately from the operator's physical LED observation. Camera callbacks remain sandboxed; clarify conditional detection-to-output automation instead of adding hardware/MCP access to a callback.
