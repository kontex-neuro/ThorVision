# Selected-device digital I/O

The existing FrameLab MCP server adds `get_digital_io()` and `set_digital_outputs(mask, values)`. The tools use the selected `DeviceClient` and the sibling `/digital-io` device API; neither tool accepts another target URL. Protocol4 identifies the box-numbered eight-channel profile: bits0..7 are channels1..8, 1 means HIGH and 0 means LOW, and bits8..31 are reserved zero. The older four-channel protocol1 remains supported with mask15. Historical prototype protocols2/3 are rejected because their channel maps differ. Output readback is the FPGA's applied latch, not a connector-voltage measurement.

`get_device_capabilities` includes a `digital_io` status. A legacy device without the endpoint reports unsupported. Stale or unavailable samples expose channel values as `null`, never as LOW. Timestamp, SPI sequence and revision uint64 values remain decimal strings.

`set_digital_outputs` obtains a fresh ready status, then issues exactly one PUT carrying a new request UUID, publisher instance, expected output revision, mask and values. Mask1..255 and values must stay inside the selected profile's valid mask. An out-of-profile mask is rejected before PUT. Success requires the device's matching acknowledgment and applied latch snapshot. A cached confirmed receipt includes `confirmed_output_bits` for that original command; stale current channel values remain unknown. A conflict (409), exhausted capacity (429), unavailable device (503), or uncertain completion (504) is returned without retry against a newer revision.

The confirmation must carry the same protocol and valid masks as the fresh status (protocol4/mask255 or protocol1/mask15), a nonzero `confirmed_at_monotonic_ns` decimal string, and the submitted output revision plus one without uint64 overflow. Invalid confirmations and interrupted HTTP response bodies are uncertain even when the response headers say200. They never trigger an automatic resend.

The FPGA watchdog clears outputs after two seconds without valid SPI polls or commands. MCP disconnection alone does not clear outputs while the sole SPI publisher continues polling. Restarting that publisher requires capability negotiation and acknowledged startup clearing before writes become available.

The tools preserve the existing native-provider permission route and login ownership. FrameLab callback workers gain no network, device, API or MCP access. Package activation remains in the existing human review controls. No automatic detector-to-TTL behavior is added.

For protocol4 on the qualified CM5 box, IN1..4 use K14/P8/N14/P9 and OUT1..4 use P2/P4/P3/P5. Thus bit0 is box1, bit1 box2, bit2 box3 and bit3 box4. Channels5..8 retain inputs A5/D5/C5/A4 and outputs L5/L4/M3/M4. The current enclosure exposes only ports1..4; other channels remain physically unqualified.

Protocol1/mask15 compatibility establishes logical bits, not the printed connector map: OUT2/OUT3 wiring changed. Requests for printed box connectors on legacy protocol1 require a separately verified operator mapping; do not infer current box wiring.

For an operator-authorized LED on digital OUT2 of the current protocol4 box, HIGH is `mask: 2, values: 2` and LOW is `mask: 2, values: 0`. These masked writes preserve all other outputs. Digital OUT2 is a TTL connector, separate from camera-status RGB LED2/GPIO20. A confirmed latch readback does not establish that the external LED illuminated; that needs the operator's observation.

Direct operator-requested DIO control requires no callback package, dependency preparation or camera activation. Read status and perform only the requested output change through the existing tools and permission route. When asked to stop, explicitly clear the requested output; closing the MCP connection alone does not do so. Do not add automatic detector-to-output behavior or retry an uncertain HIGH write.

Building, installation, firmware programming, device API availability and discovery by the agent host are separate checks. After an MCP registration change, reload its host before claiming tools are available in that host. Native provider login stays owned by the native agent. No callback hardware/network permission or camera package activation control changes are added.
