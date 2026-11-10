# ThorVision FrameLab agent bridge

The selected-device [digital I/O tools](DIGITAL-IO.md) read four input/output latch bits and perform one revision-checked output write through the existing device API. Callback sandbox permissions and package activation controls are unchanged.

For reproducible native-agent installation, use the [standalone deployment recipe](deployment/README.md)
and [deployment material map](deployment/MATERIALS.md). It packages MCP, the browser UI, SDK reference
and portable skill independently of the desktop provider adapters. The instructions below also cover
the original desktop bridge/development mode.

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

Gemini runs as a hidden local Node child process, using the installed
`@google/gemini-cli` entry point with `--acp --allowed-mcp-server-names framelab`.
The bridge exchanges ACP messages over stdin/stdout; it does not open a terminal
window or use PowerShell as Gemini's interface. Google sign-in opens the browser
through Gemini's own runtime.

## Client protocol (version 1)

Requests: `bridge.initialize {protocolVersion}`, `agents.discover`,
`agent.connect|accountStatus|login|disconnect {agent}`,
`session.create {agent, camera, workspace, extensionsUrl, approval}`,
`session.prompt {sessionId, text}`, `session.cancel`, `session.close`,
`permission.respond {requestId, allow}`.
Uploaded packages use `package.prepare {digest, extensionsUrl}` and `package.cancel`;
`package.progress {text}` notifications describe validation/preparation/testing without a model call.
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
`write_project`, `validate_extension`, `prepare_extension`, `prepare_dependencies`,
`test_candidate`, `get_validation_report`). Files are confined to
`<workspace>/package` (no `..`, absolute paths or links); reports and history go
to `<workspace>/history`. One workspace per camera is shared by all agents.
Activation is not a tool: only the client's Apply activates a tested digest.

| Agent | Native tools off |
| --- | --- |
| Codex | `--disable` shell_tool, unified_exec, apps, plugins, browser_use, computer_use, in_app_browser, image_generation; `web_search="disabled"`; the whole `mcp_servers` table is replaced, so servers from the user's Codex config do not load; read-only sandbox; command and file-change approvals are declined |
| Claude | `tools: []`, `settingSources: []`, `strictMcpConfig`, only the `framelab` MCP server; `canUseTool` routes every call |
| Gemini | system settings (`GEMINI_CLI_SYSTEM_SETTINGS_PATH`, highest precedence) exclude every built-in tool, allow only the `framelab` MCP server and enforce Google sign-in; no client fs/terminal capabilities |

Permission policy (bridge): ThorVision tools are allowed (or asked, when the
client sets `approval: "ask"`); anything else is denied; unanswered prompts deny
after five minutes.

## Dependency and model preparation

`prepare_extension` validates the extension, uploads its manifest/code and requests
`POST /v1/packages/{digest}/prepare` with `{"download":"proxy"}`. When a download
is necessary, the device returns a single-use job token. The tool connects a
temporary WebSocket relay through the existing device API; the device's own
package manager resolves and downloads Linux/Python-compatible wheels through
the PC internet connection. No Windows internet-sharing setting or extra network
configuration is required. TLS remains end-to-end between the device
and the repository. The relay permits port 443 on PyPI, Python's file host and
the supported GitHub release-asset hosts only; both ends enforce the allowlist,
eight connections, bounded chunks/buffers and timeouts.

The device installs the verified artifacts offline in an immutable private
environment. Callback workers still have no network access. Preparation is
followed automatically by testing as one preparation operation; Apply is never exposed to an agent.
Compatible verified dependency/model environments are reused automatically. The user
only specifies the desired behavior. `prepare_dependencies` remains a compatibility
alias for the full preparation workflow. If an agent edits a project and forgets
preparation, the bridge executes it before completing the turn. It skips a fresh
successful preparation, and reports an already-observed failure without retrying it.
Tests bind to the prepared environment, model hashes, runtime and configuration.
Cached preparations skip the proxy. Disconnecting the relay cancels an unfinished
preparation; retrying is safe. The Assistant displays progress from the device's
sanitized `preparation_jobs` state, and detailed tool reports remain in history.

V1 supports binary PyPI wheels and pinned model files. Requirement files may
contain package names, extras and version constraints, but no URLs, indexes,
source builds or environment markers. Models must have an allowed HTTPS source,
SHA-256, license and path below `models/`; callbacks access that path relative
to `context["package_dir"]`. Read the advertised preparation/storage limits
before proposing a large ML stack. Existing SDK-only extensions need no download.
An older device explicitly reports preparation unsupported.

## Automatic device access and cleanup

The developer deploys and maintains the device analytics service before users
connect. The client automatically connects to `<camera device API>/framelab`
and retries temporary failures. All user operations use this API; no device SSH
account or manual Connect step is required, and there is no SSH fallback or
automatic service installation. An unavailable service must be resolved by the
developer, not by asking the user for device credentials.
The device gateway forwards HTTP and WebSocket traffic to the loopback analytics
service. This uses the existing trusted local-network camera API access model;
it does not introduce internet-facing authentication or change agent-account login.
Loopback API URLs remain supported for local development and isolated fixtures.
Automatic connection can be disabled for isolated tests with
`THORVISION_ANALYTICS_AUTOCONNECT=0`.

The Extensions page provides Prepare, Apply, Delete selected and Clean unused.
Cleanup first requests `POST /v1/cleanup/plan` with exact package digests, displays
items and recoverable space, and executes the reviewed plan with
`POST /v1/cleanup/execute`. Active, rollback and in-use packages are protected;
shared environments are retained while referenced. Plans expire and are rechecked
before deletion. No cleanup is silently triggered by a prompt or failed preparation.
Apply uses the server's current `ready` flag, not an old report's `passed` value.

The device currently budgets eight minutes for proxy preparation. Codex and
Claude tool timeouts are increased to fifteen minutes; the pinned Gemini ACP
runtime uses its ten-minute MCP default. No provider credentials are passed to
the relay. A failed preparation produces a report, not simulated detections.

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

## Initial verification on 2026-10-28

- `npm test`: 21 tests, including protocol fixtures for each adapter (sign-in
  required, streamed messages, tool calls, permission routing, cancellation,
  quota exhaustion, process exit), bridge policy, package digests and tools.
- Package digests match the Python SDK byte for byte; a candidate written and
  tested through the tools against the device service passed in its sandbox.
- Live account status (no prompts): Codex connected (ChatGPT plan), Claude
  connected (Claude Pro), Gemini sign-in required.
- No live agent prompt has been run yet; mocked sessions do not count as a
  provider scenario.

## Automatic workflow verification on 2026-10-28

The updated suite passes 35 tests, including all three provider adapters and
post-turn preparation when an agent edits a project without calling the prepare
tool. Tests cover fresh readiness, changed revisions, invalid manifests,
preparation/test failures, cancellation and avoiding repeated failed preparation.
An isolated device API gateway also completed an actual fresh PyPI download
through the PC relay, offline installation and a 12-frame device test. Cleanup
plan/execute removed that disposable fixture and rejected replay of its plan.
These checks do not claim new live model sessions for all three providers.

## Standalone Claude control mod

`plugins/thorvision-framelab` adds native Claude Code controls and a larger browser
live view using this same MCP server and the device API. ThorVision desktop and
SSH are not needed. See that folder's `README.md` for the launcher, design,
operation lifecycle and trusted-local security boundary.

Verification on 2026-10-28 passed 51 bridge tests and 3 native mod interaction
tests. A real Claude Pro session created and prepared an SDK rendering callback;
the browser's reviewed Run action displayed its crosshair and original frame
sequence on the camera. Restore previous returned the camera to the existing
YOLO11n + ByteTrack package. No package was deleted: the cleanup plan correctly
protected the rendering test as the previous revision. This is a live Claude
scenario; it does not imply equivalent native panels were implemented for Codex
or Gemini.
