# Selected-device digital I/O

The existing FrameLab MCP server adds `get_digital_io()` and `set_digital_outputs(mask, values)`. The tools use the selected `DeviceClient` and the sibling `/digital-io` device API; neither tool accepts another target URL. Four channels occupy bits 0..3: 1 means HIGH, 0 means LOW. Bits 4..31 are reserved zero. Output readback is the FPGA's applied latch, not a connector-voltage measurement.

`get_device_capabilities` includes a `digital_io` status. A legacy device without the endpoint reports unsupported. Stale or unavailable samples expose channel values as `null`, never as LOW. Timestamp, SPI sequence and revision uint64 values remain decimal strings.

`set_digital_outputs` obtains a fresh ready status, then issues exactly one PUT carrying a new request UUID, publisher instance, expected output revision, mask and values. The values may set only bits included in a nonzero mask from 1 to 15. Success requires the device's matching acknowledgment and applied latch snapshot. A cached confirmed receipt includes `confirmed_output_bits` for that original command; stale current channel values remain unknown. A conflict (409), exhausted capacity (429), unavailable device (503), or uncertain completion (504) is returned without retry against a newer revision.

The confirmation must carry both valid masks as `15`, a nonzero `confirmed_at_monotonic_ns` decimal string, and the submitted output revision plus one without uint64 overflow. Invalid confirmations and interrupted HTTP response bodies are uncertain even when the response headers say 200. They never trigger an automatic resend.

The FPGA watchdog clears outputs after two seconds without valid SPI polls or commands. MCP disconnection alone does not clear outputs while the sole SPI publisher continues polling. Restarting that publisher requires capability negotiation and acknowledged startup clearing before writes become available.

The tools preserve the existing native-provider permission route and login ownership. FrameLab callback workers gain no network, device, API or MCP access. Package activation remains in the existing human review controls. No automatic detector-to-TTL behavior is added.

The DIO build is a preparation artifact. Flashing, package installation, release activation and physical acceptance are separate later operations. The planned OUT2->IN2 loopback and OUT1 LED fixture qualify only those paths; the other channels remain physically unqualified.
