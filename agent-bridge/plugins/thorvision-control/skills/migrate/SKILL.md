---
name: migrate
description: Explain the migration from the old ThorVision control plugin to ThorVision FrameLab.
disable-model-invocation: true
---

The workflow is now ThorVision FrameLab. Use the installed
`thorvision-framelab@thorvision-local` plugin and `/framelab`, or its
`framelab-start` startup skill. This compatibility entry does not start a second
MCP server or duplicate the authoring hooks. Provider login and experiments stay
with the existing agent/workspace.
