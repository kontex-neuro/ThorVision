# Direct digital I/O

Use this route when the user asks to read digital inputs or set a digital output on the device already selected by the FrameLab MCP connection. For example: “Turn on the LED connected to digital OUT2 until I say stop.” This is direct operator-requested I/O, not camera-analysis authoring. Do not create or edit a callback project, read an unrelated project, prepare a package, or activate an analysis to perform it.

Use the existing `get_digital_io` and `set_digital_outputs` tools. The selected DeviceClient owns the API target; the native SPI publisher owns hardware access. Do not use SSH, direct HTTP writes, direct SPI, a second publisher, or another MCP registration as a substitute. Missing tools or an unsupported device need the matching PC/device release from the deployment owner.

## Identify the connection and hardware

Read `get_digital_io` before a requested change. Require fresh `state: "ready"`, supported protocol and valid masks; stale, unavailable or unsupported channels are unknown, not LOW. The current ready contract requires `sample_age_ms < 500`. The write tool also reads a fresh snapshot and uses the publisher instance and output revision to prevent a stale command from being applied.

The current box profile is `cm5-hx8k-cb132-dio8-box-v4`, with TDIO protocol `4` and `valid_input_mask: 255`, `valid_output_mask: 255`. Its exposed box connectors map as follows:

| Box connector | Bit | Mask |
| --- | --- | --- |
| IN1 / OUT1 | 0 | 1 |
| IN2 / OUT2 | 1 | 2 |
| IN3 / OUT3 | 2 | 4 |
| IN4 / OUT4 | 3 | 8 |

This mapping describes the board/profile; it does not establish electrical acceptance of every external connector. For any external LED connection, record confirmed FPGA latch state separately from the operator's observation of the LED. Do not infer acceptance of other channels from that observation.

Internal channels 5–8 exist in the eight-bit protocol but are not physically qualified box connectors. A mask of 255 is an eight-bit protocol capability, not permission to assume eight usable box connectors. Use qualified connector mapping for the selected hardware. If the tool explicitly supports an older TDIO protocol `1` device with masks `15`, label it as a legacy four-channel logical mapping; do not infer this current box's physical wiring from legacy support. For a request naming the printed box OUT2 connector, require the current protocol-4 box mapping or a separately verified operator mapping: OUT2/OUT3 wiring differs from the legacy mapping. Other protocols, masks or unknown physical mappings require clarification before a physical-output change.

Digital OUT2 is the external TTL connector. It is not camera-status RGB LED2 or GPIO20. An LED connected to OUT2 is an external load whose illumination must be observed by the operator.

## Set only the requested output

For an authorized OUT2 action, use these exact masked values:

| Request | `set_digital_outputs` arguments | Effect |
| --- | --- | --- |
| Turn OUT2 on / HIGH | `{"mask": 2, "values": 2}` | Set bit 1; preserve every other output |
| Turn OUT2 off / LOW | `{"mask": 2, "values": 0}` | Clear bit 1; preserve every other output |

Do not use a full-register mask to change one output. `values` may contain only bits selected by `mask`.

Send one write for the requested action. A successful result must contain the tool's matched, confirmed acknowledgment: request identity, publisher instance, next output revision, command sequence and `confirmed_output_bits` consistent with the requested masked change. The tool checks this binding; report any tool error or mismatched result instead of treating a returned HTTP response as success. If a confirmation is returned with stale current status, distinguish the latch acknowledged at confirmation time from its unknown current state; read fresh status before claiming it remains set.

Never automatically replay a HIGH after a timeout, uncertain completion or conflict. Read status to establish what is currently known, report the uncertainty, and obtain a new operator decision before another HIGH attempt. Do not silently fetch a fresh revision and repeat a conflicted command. A later explicit stop request is a new LOW action, not a HIGH retry.

## Hold and stop

“Until I say stop” means leave the requested output latched; do not pulse it, schedule an unrequested timeout, or start an agent polling loop. On the user's stop request, issue the masked LOW action and check its acknowledgment. If communication fails, report that LOW is not confirmed; do not claim the LED is off.

Disconnecting the agent or MCP does not clear output latches while the SPI publisher continues polling. The FPGA's enabled `watchdog_timeout_ms: 2000` clears outputs after loss of SPI polling. It is not an agent-session lease and must not be presented as an automatic two-second shutoff after the chat closes.

Report confirmed FPGA latch state separately from physical observation: “OUT2's latch is confirmed HIGH” is supported by the acknowledgment; “the LED is lit” requires the operator's observation or suitable independent measurement. A digital-input read also does not measure output connector voltage unless an explicitly configured loopback is part of the test.

## Preserve the camera workflow

Direct I/O authorization does not grant callbacks hardware, network or MCP access. Frame callbacks remain isolated. A request to drive outputs from detections needs a separately supported integration; do not add host-device access to `setup()` or `process()`.

Camera Run, Stop, Restore and package deletion remain human-reviewed panel controls. Direct operator-requested digital-output writes use this I/O route and do not require camera activation.

## Later operator test

Do not change outputs merely to inspect or install this guide. When the operator is ready with an LED connected to the current box's OUT2 connector, use this plain request in their agent:

> Turn on the LED connected to digital OUT2 until I say stop.

The agent reads readiness, sends the OUT2 HIGH action once and reports latch confirmation. The operator then checks the external LED. When the operator says “Turn off the LED connected to digital OUT2,” the agent sends the masked LOW action and reports its confirmation. Record the operator's LED observations separately from latch acknowledgments.
