# ThorVision agent bridge

Node.js/TypeScript companion that ThorVision launches with `QProcess`
(`AgentBridgeClient`). It speaks one versioned JSON-RPC protocol over stdio to
the client and drives Codex, Claude and Gemini through each provider's own
runtime. Diagnostics go to stderr; stdout carries protocol messages only.

```sh
npm ci
npm run build    # regenerates the Codex protocol types from the pinned codex, then tsc
npm test
```

## Version lock

| Agent | Integration | Pinned |
| --- | --- | --- |
| Codex | native app-server protocol over stdio (`codex app-server`) | `@openai/codex` 0.165.0 |
| Claude | official TypeScript Agent SDK with the local Claude Code login | `@anthropic-ai/claude-agent-sdk` 0.3.312 |
| Gemini | native ACP mode (`gemini --acp`) | `@google/gemini-cli` 0.66.0, `@agentclientprotocol/sdk` 2.0.2 |
| Tools | MCP stdio server | `@modelcontextprotocol/sdk` 1.32.2 |

The runtimes are project dependencies launched with this Node; global installs
(for example a global Codex 0.34.0) are neither used nor upgraded.
`src/protocol/codex` is generated from the pinned Codex and not committed.
`sdk-snapshot/` is a byte-identical copy of the device SDK; `SNAPSHOT.json`
records its version, schema digest and source commit.

## Client protocol (version 1)

Requests: `bridge.initialize {protocolVersion}`, `agents.discover`,
`agent.connect|accountStatus|login|disconnect {agent}`,
`session.create {agent, camera, workspace, extensionsUrl, approval}`,
`session.prompt {sessionId, text}`, `session.cancel`, `session.close`,
`permission.respond {requestId, allow}`.
Notifications: `session.event {sessionId, agent, type, ...}` with `type` one of
`message`, `progress`, `toolStarted`, `toolFinished`, `permissionRequested`,
`artifactChanged`, `completed`, `failed`; and `agent.status` after a sign-in.

Account states are `connected`, `signin_required`, `quota_exhausted`,
`unsupported`, `unavailable`, `unknown` and `error`; the client shows them as
connection states, never as callback failures. Billing is what the provider
reports (ChatGPT plan, Claude subscription) or `Unknown`.

## Tools and restrictions

Every agent gets the same MCP tools (`get_sdk_contract`,
`get_device_capabilities`, `get_sample_frames`, `read_project`,
`write_project`, `validate_extension`, `prepare_dependencies`,
`test_candidate`, `get_validation_report`). Files are confined to
`<workspace>/package` (no `..`, absolute paths or links); reports and history go
to `<workspace>/history`. One workspace per camera is shared by all agents.
Activation is not a tool: only the client's Apply activates a tested digest.

| Agent | Native tools off |
| --- | --- |
| Codex | `--disable` shell_tool, unified_exec, apps, plugins, browser_use, computer_use, in_app_browser, image_generation; `web_search="disabled"`; the whole `mcp_servers` table is replaced, so servers from the user's Codex config do not load; read-only sandbox; command and file-change approvals are declined |
| Claude | `tools: []`, `settingSources: []`, `strictMcpConfig`, only the `thorvision` MCP server; `canUseTool` routes every call |
| Gemini | system settings (`GEMINI_CLI_SYSTEM_SETTINGS_PATH`, highest precedence) exclude every built-in tool, allow only the `thorvision` MCP server and enforce Google sign-in; no client fs/terminal capabilities |

Permission policy (bridge): ThorVision tools are allowed (or asked, when the
client sets `approval: "ask"`); anything else is denied; unanswered prompts deny
after five minutes.

## Authentication

Subscription sign-in stays with each provider's runtime; the bridge never reads,
copies or forwards credentials. Child processes run without provider API keys
or endpoint overrides (`OPENAI_API_KEY`, `ANTHROPIC_API_KEY`, `GEMINI_API_KEY`,
...); the user's environment is not changed, and an API-key-only setup reports
`unsupported` instead of falling back to paid API use. Sign-in: Codex returns a
ChatGPT browser URL, Claude opens a terminal with `claude /login`, Gemini starts
its Google sign-in. Disconnecting stops this bridge's processes and never signs
the user out. Provider eligibility for third-party subscription use is an
external prerequisite; record the authorized route for each live test.

## Verification on 2026-10-28

- `npm test`: 21 tests, including protocol fixtures for each adapter (sign-in
  required, streamed messages, tool calls, permission routing, cancellation,
  quota exhaustion, process exit), bridge policy, package digests and tools.
- Package digests match the Python SDK byte for byte; a candidate written and
  tested through the tools against the device service passed in its sandbox.
- Live account status (no prompts): Codex connected (ChatGPT plan), Claude
  connected (Claude Pro), Gemini sign-in required.
- No live agent prompt has been run yet; mocked sessions do not count as a
  provider scenario.
