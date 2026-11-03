# ThorVision control mod

A native Claude Code conversation with a camera control panel and a browser live view. ThorVision desktop does not need to be open. The developer pre-deploys and maintains the device analytics service. All user operations use its API; this client does not install or upgrade the service, request device SSH credentials or fall back to SSH when the API is unavailable.

## Start

Requires Node.js 22+, Claude Code **2.1.312+**, and this repository's built `agent-bridge` with its SDK snapshot and `control-ui` directory.

For a persistent installation, add `agent-bridge/plugins` as a local marketplace, then install with the runtime, device and workspace paths:

```powershell
claude plugin marketplace add C:\path\to\agent-bridge\plugins
claude plugin install thorvision-control@thorvision-local --scope user --config "bridge_dir=C:/path/to/agent-bridge" --config "device_url=http://DEVICE:8000/analytics" --config "workspace=C:/Your/AnalyticsProject"
```

Then start ordinary `claude` and type `/thorvision`. No special launcher or environment variables are required. The installed bootstrap resolves the configured bridge path even when Claude copies the plugin into its cache. Change saved settings through `/plugin configure thorvision-control@thorvision-local`.

Installation alone does not change ordinary chats. `/thorvision` opts the current conversation into analytics authoring. `/thorvision-off` leaves that mode and closes the panel without stopping the device. It waits for editing/preparation to finish before leaving.

For development with a source plugin, run `npm ci` and `npm run build` in `agent-bridge` if needed. Sign in using Claude Code's normal login flow, then:

```powershell
.\scripts\start-claude-control.ps1 -DeviceUrl http://DEVICE:8000/analytics -Workspace C:\Your\AnalyticsProject
```

Use `-ClaudeExecutable C:\path\to\claude.exe` for a separate installation. The launcher checks the version and inherits Claude's configured account and billing method; the mod never stores provider credentials. Existing API-key configuration still means API billing.

Type `/thorvision`, select a camera, and describe your idea in the normal conversation. Example: “Detect people with YOLO11n and ByteTrack. Show IDs, a two-second trail and the visible count.” Existing projects are preserved. A fresh project is created only in the workspace you specify.

The workflow instructions and bundled analytics skill ask the agent to inspect the contract and capabilities and finish with `prepare_extension`. That stage owns validation, automatic compatible environment reuse, dependency installation through the PC relay and testing of the exact revision. The mod additionally checks changed projects after a completed turn. Failed preparation stays failed; it does not loop or silently activate an older candidate. No manual SSH login is involved.

Open **Live video & storage**, or use the native panel. **Run on camera** first shows the exact camera, revision and package digest. Confirming starts the operation; its progress stays visible while the worker loads. **Stop processing** and **Restore previous** also require review. The large view shows continuous JPEGs from one persistent device preview connection, with actual FPS, latency and worker memory. It shows the active extension; a prepared candidate has no live preview until run. Raw camera preview is not available through the current analytics API.

Packages & storage uses the device cleanup plan and execution APIs. Active/previous packages stay protected. Nothing is deleted automatically. Cancel preparation stops this client's waiting/download relay; a device test already started may finish. Interrupt an agent-owned tool in Claude, rather than using a panel cancel button.

## Architecture and boundaries

```text
Native Claude conversation + provider sign-in
         |                  /thorvision native controls
         | MCP stdio                   | trusted mod HTTP calls
         v                             v
Shared ThorVision MCP process -- local bearer-protected control server
         |                             | browser live view
         +------------ device API -----+
                            |
             package / dependency / test / activation services
                            |
                    isolated Python worker
                            |
          frame-associated results -> processed preview + recorder
```

Claude launches and owns the MCP child process. The first panel/status request opens its loopback server on a random port; it closes with that MCP session. The URL contains a short-lived session token in its fragment, which the browser removes from the address bar and retains in tab session storage. Do not publish that URL. Local Host/Origin checks reject unrelated web origins.

The ThorVision MCP surface exposes authoring, preparation and status, not activation or cleanup. The launcher limits built-in model tools to Read. User-configured MCP integrations and other plugins retain their native settings. The control token is still available through MCP, so this is a trusted local integration, not cryptographic proof of a human click. Adding unrestricted shell/HTTP tools removes that separation. Device readiness, exact-digest/config checks, revision conflicts and protected cleanup are enforced independently by the device/control APIs.

The current device API is intended for the trusted device LAN. Multi-user deployment needs device authentication and authorization. This mod neither adds device TLS nor exposes a public network service.

## Verification

Run `npm test` in `agent-bridge`, and `claude plugin validate .` / `claude plugin test` in this directory. Native mod tests exercise terminal and desktop element trees, review-before-execute, untested-candidate gating and turn lifecycle. Shared control tests cover stale revisions, backend readiness, cleanup protection, loopback authorization and operation ownership. Live camera checks remain necessary; a 12-frame synthetic fixture establishes execution compatibility, not person-detection accuracy.

This first mod targets Claude Code. Codex and Gemini can use the same MCP server but do not gain Claude's native panel. The existing ThorVision desktop adapters for those providers are unchanged.
