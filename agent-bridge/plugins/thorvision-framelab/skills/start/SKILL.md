---
name: framelab-start
description: Open ThorVision FrameLab in a new Claude conversation before the native /framelab command has registered.
disable-model-invocation: true
---

Call `open_control_panel` on the `framelab` MCP server supplied by
`thorvision-framelab`. Discover the tool if it is deferred. Give the user its
session-local browser link. If the tool is unavailable, report the setup error;
do not request SSH credentials or deploy services.

The new conversation is now started, so `/framelab` opens the native panel from
the next message. The user describes the experiment in the agent chat and
reviews the tested candidate with **Run on camera**. Never execute the panel's
HTTP mutation controls yourself.

For a direct digital-input/output operator request, follow the [digital I/O skill](../digital-io/SKILL.md). It uses the same selected MCP server without preparing or activating a camera analysis.
