# Native-agent workspace setup

Read this when installing ThorVision FrameLab MCP in a new workspace or diagnosing missing tools. An existing working analytics session does not need infrastructure reinstallation.

The implementation and maintained deployment recipe live in [ThorVision/agent-bridge/deployment](https://github.com/kontex-neuro/ThorVision/tree/feat/framelab/agent-bridge/deployment). This skill contains operating guidance; it does not contain a second MCP server. Use a matching, versioned runtime package supplied by the developer. The `@kontex/thorvision-framelab` 0.2.0 deployment recipe is a candidate, not a claim that a package has been published to a public registry.

## Prerequisites and ownership

- Node.js 22 or newer, a chosen native agent, and that agent's authorized provider login.
- A developer-deployed ThorVision device API at the supplied local-network address ending in `/framelab` (or the compatible `/analytics` root). Use the current device URL, not a remembered IP.
- A managed runtime directory and a writable, absolute workspace path. Each workspace owns its `package/` and `history/`; its MCP configuration selects the device.
- The runtime includes the MCP server, browser controls and SDK reference snapshot. PC-side Python, inference libraries, Qt and the desktop provider adapters are not prerequisites for this native workflow. Supported extension dependencies are prepared on the device.

Provider sign-in remains with the selected native agent. Do not copy account credentials, full personal agent configuration, browser control tokens, recordings or prepared device environments into an installation recipe. Extension source/history stay in local workspace files; the selected model provider can receive prompts and tool results according to that agent's configuration. This is not an offline-model guarantee.

## Install and configure

Obtain the developer's runtime `.tgz` or HTTPS artifact URL, its SHA-256 from a trusted release manifest, and the matching deployment installer (`install.mjs` with `npm.mjs` beside it). Developers build the artifact from ThorVision source using `npm run build:standalone` and `npm run pack:standalone`; use the maintained deployment README for the exact artifact and validation procedure. The installed native runtime does not need the full desktop bridge dependency set.

PowerShell example: set these variables to the selected local paths and current device API before running the commands. Do not run placeholders literally.

```powershell
$RuntimeDirectory = '<new absolute versioned runtime directory>'
$DeploymentDirectory = '<absolute directory containing install.mjs and npm.mjs>'
$PackageArchive = '<absolute runtime .tgz path or HTTPS artifact URL>'
$ExpectedSha256 = '<trusted release manifest SHA-256>'
$Workspace = '<absolute analytics project directory>'
$DeviceUrl = 'http://<current private IPv4>:8000/framelab'

node (Join-Path $DeploymentDirectory 'install.mjs') --artifact $PackageArchive --sha256 $ExpectedSha256 --prefix $RuntimeDirectory
$McpCli = Join-Path $RuntimeDirectory 'node_modules/@kontex/thorvision-framelab/bin/thorvision-framelab.mjs'
node $McpCli setup --agent opencode --workspace $Workspace --device-url $DeviceUrl
node $McpCli doctor --workspace $Workspace --offline --json
```

The installer verifies the archive checksum before installing with lifecycle scripts disabled, then verifies shipped runtime files. Use a new immutable runtime prefix for upgrades; retain the previous one for rollback and rerun setup for each workspace. npm and registry access (or an appropriately populated npm cache) are installation prerequisites; the tarball is not an offline bundle of node_modules.

The setup interface accepts `opencode`, `gemini`, `claude` or `codex`. Select the host the user requested. Setup manages that host's workspace MCP configuration, the shared `~/.agents/skills/thorvision-framelab` copy and a thin `thorvision-analytics` compatibility alias; Codex also receives project-local copies. Unchanged previously managed skill files can upgrade with backups. A different unmanaged skill requires review and explicit `--replace-skill`; replaced contents are backed up. New MCP registrations use `framelab`; legacy `thorvision` registrations remain supported. Setup migrates only entries it can identify as managed and never runs both for the same workspace. Review conflicts instead of overwriting unrelated settings. Consult the runtime recipe for host-specific registration/trust steps; generation of a config file does not prove the host loaded it.

The shared skill is reusable; device and project selection belong to workspace configuration. If the same skill name also exists in an agent-specific directory, inspect which copy that host uses and resolve the duplication intentionally. Do not silently replace user-maintained skills.

For a custom MCP host, the server entry point is:

```text
node <installed package>/bin/thorvision-framelab.mjs serve --workspace <absolute workspace> --device-url <device API>/framelab
```

The runtime prefers `THORVISION_FRAMELAB_URL`, falling back to `THORVISION_EXTENSIONS_URL`, and retains `THORVISION_WORKSPACE` and optional `THORVISION_CAMERA`. Explicit CLI options take precedence over environment settings, then the saved workspace record. It always uses its verified bundled SDK snapshot, overriding any inherited `THORVISION_SDK_SNAPSHOT`. Preserve stdout for MCP messages. Keep the owning agent session open while using its browser controls.

## Verify in stages

1. Run offline doctor to check the local installation/configuration. This does not establish device availability or provider login.
2. With the current device endpoint selected, run `doctor --workspace <absolute workspace> --device-url <device API>/framelab --json` for the recipe's read-only connectivity checks. An unavailable API belongs to the device/network deployment owner; do not fall back to SSH.
3. Start the chosen agent in that workspace and confirm it exposes the `framelab` tools (legacy registration `thorvision` is also supported). Configure its own provider sign-in if needed. Configuration support for all four hosts is not proof that live model sessions have been qualified on each host.
4. When the user requests an analytics test, inspect the SDK/capabilities, create an SDK-only candidate, call `prepare_extension` and report its actual result. This test can consume provider usage and create a device candidate; keep it separate from an installation-only check.
5. Open the browser control panel. The user reviews the exact camera and tested package before Run. Check advancing preview frame references after activation; a successful fixture test alone is not evidence of live detection quality.

The existing workflow has Windows OpenCode usage evidence. This does not certify every new packaged-runtime combination. macOS installation and host integration remain pending verification; do not label them tested based on portable JavaScript alone. See the runtime recipe's current validation matrix for exact versions and checks.

The current device API is for a trusted LAN. Authentication for shared or internet-facing deployment is a separate product requirement. Dependency preparation may temporarily relay repository downloads through the PC; callback workers remain isolated on the device. Missing storage or incompatible native dependencies must be reported using the actual preparation error, without changing limits or deleting data implicitly.
