# Reproduce the ThorVision FrameLab environment

This is the maintained deployment recipe for the currently tested Node.js MCP. A release kit installs an independent, versioned native-agent runtime with its WebUI, SDK reference and portable skill. It reuses the existing implementation; it does not port MCP to Python or require the Qt desktop program. PC Python is not a requirement. The camera device executes Python callbacks.

The package candidate is **0.2.0**. Its source and deployment documents are versioned on the desktop repository's `feat/framelab` branch. The package has not been published to a registry. The user reported a working OpenCode trial on 2026-11-04. That is historical evidence for the earlier development installation. Current local checks are recorded in [the Windows build verification](../../docs/build-verification-2026-11-09.md). Native macOS qualification remains pending.

## Install from a developer-supplied release kit

Prerequisites: Node.js 22+ with npm, the selected native agent, browser access, a writable project folder and the current developer-predeployed device API. npm needs internet or a populated package cache to install the locked production dependencies. The recipe does not install or sign in to a provider. No device SSH credentials are involved.

The developer supplies a `deploy-kit-VERSION-HASH/` folder (or an archive of it). Obtain it from the chosen trusted release location and retain its SHA-256/release provenance. In that folder run the following on Windows PowerShell or macOS Terminal, substituting your own workspace and device API:

```sh
node reproduce.mjs --agent opencode --workspace "/absolute/path/to/experiment" --device-url "http://DEVICE_IP:8000/framelab"
```

For Windows use an absolute workspace such as `C:/Users/you/Documents/ThorVision/experiment`. Quotes handle spaces; no JavaScript `-e` or shell-specific line continuation is used. The shared deployment recipe and Node scripts are the same on both systems. Native macOS execution is pending qualification.

This entry point:

1. Checks the kit's installer-file hashes and the runtime archive SHA-256.
2. Installs its shrinkwrapped production dependencies into a new content-addressed `~/.thorvision/runtimes/HASH/` prefix, with lifecycle scripts disabled. It rechecks an already-installed version's shipped files before reuse.
3. Generates the selected host's **workspace** MCP registration and `.thorvision/workspace.json` pointing to the installed package, never to a source checkout.
4. Installs the bundled `thorvision-framelab` skill and thin `thorvision-analytics` compatibility entry point into `~/.agents/skills`. Codex also gets project `.codex/skills` copies. Unchanged, previously managed skill files can upgrade with backups. A different unmanaged skill stops setup; after review, `--replace-skill` backs it up before replacement.
5. Runs an offline doctor: actual MCP handshake/list tools, SDK contract read and local WebUI asset fetch. This does not establish device availability, account login or model behavior.

Use `--prefix /another/versioned/runtime` to choose the installation directory. `--home /isolated/test/home` is available for setup tests. `--check-device` adds read-only device capability/state requests; it never prepares, applies, stops or deletes analytics. Setup preserves unrelated JSON settings and simple Codex MCP tables. Existing JSONC, conflicting FrameLab/ThorVision entries or ambiguous inline/root TOML declarations require review and manual merging. A restart/trust confirmation may be required before the host discovers newly registered tools.

Next, start the selected agent **in the configured workspace**, sign in with its own authorized account and select a model. OpenCode uses `/connect` and `/models`; Grok through Copilot is selected within OpenCode, not by this installer. Gemini/Codex workspace trust and Claude project MCP approval are host-owned steps. Configuration generation is not qualification of live prompts on every host.

Prompt:

> Use thorvision-framelab. Draw a moving green rectangle using the bundled SDK and show me the tested preview. I will review and click Run on camera.

Keep the agent/MCP session open while using its session-local panel. Run changes the selected camera's active analysis. Fixture tests establish execution compatibility, not model accuracy. Dependency reuse, fresh download through the PC proxy and live activation are separate observable verification results.

## Build a release kit from source

Check out the intended [desktop source revision](https://github.com/kontex-neuro/xvc/tree/feat/framelab). The skill inputs are included in `deployment/skills`, with their original provenance in `skills/SNAPSHOT.json`. No neighboring ai-skills checkout is required. From `xvc/agent-bridge`:

```sh
npm ci --ignore-scripts
npm run pack:standalone
```

The standalone build invokes TypeScript for only the MCP import graph. It does not generate the Codex protocol or compile desktop adapters. The main checkout's development dependencies are needed only for building/testing, not for installing a release kit. `deployment/runtime/npm-shrinkwrap.json` fixes the standalone production dependency graph, including transitive integrity values. Recreate that lock deliberately and requalify before upgrading it. To test external skill sources, append `-- --skill-dir /path/to/ai-skills/thorvision-framelab`. That directory needs its sibling `thorvision-analytics` skill, and its provenance is recorded as an external override.

Outputs:

```text
release-artifacts/
  kontex-thorvision-framelab-0.2.0.tgz
  kontex-thorvision-framelab-0.2.0.tgz.sha256
  release-manifest.json
  deploy-kit-0.2.0-HASH/
    reproduce.mjs, install.mjs, npm.mjs
    runtime .tgz + checksum + release-manifest.json
    stack.recipe.json + these deployment documents
```

The archive contains compiled MCP modules, control-ui, sdk-snapshot, CLI, skill, LICENSE, recipe and release.json. npm consumes the package's `npm-shrinkwrap.json` during installation; the shipped `dependency-lock.json` preserves an immutable audit copy. Runtime file verification covers **shipped application assets**, not every installed node_modules file. npm's lockfile/integrity checks govern dependency installation; offline doctor checks that the resulting MCP actually starts.

`release.json` records source HEAD, dirty flags, source-input/skill tree hashes, SDK identity and per-file hashes. It records uncommitted candidate sources honestly; HEAD alone is not the version of dirty files. Commit/pin the reviewed recipe/runtime/skill revisions before making a public or internal supported release. Build/test on a clean release checkout and retain the platform verification report with the artifact.

## Diagnose or upgrade an installed runtime

Use the CLI path printed by the installer:

```sh
node /installed/runtime/node_modules/@kontex/thorvision-framelab/bin/thorvision-framelab.mjs doctor --workspace /path/to/experiment --offline --json
node /installed/runtime/node_modules/@kontex/thorvision-framelab/bin/thorvision-framelab.mjs doctor --workspace /path/to/experiment --json
```

Upgrades install into another immutable prefix. Re-run setup to rebind a workspace; the prior installation remains available for rollback. Runtime and workspace data are separate. Never copy provider auth files, recordings, browser session tokens, prepared device venvs or personal agent settings into a release.

## FrameLab compatibility migration

The canonical package is `@kontex/thorvision-framelab`0.2.0 and its CLI is
`thorvision-framelab`; `thorvision-mcp` remains a shim to the same implementation.
New MCP registrations use `framelab`. The tool names and semantics are unchanged.
Existing `thorvision` registrations can still run the server; do not register
both names for the same workspace.

Setup recognizes old JSON registrations only when the saved
`.thorvision/workspace.json` proves the generated runtime/arguments match, or
when a recorded managed-entry hash matches. It replaces that owned entry with
one `framelab` entry and preserves other servers. Changed or unowned entries
stop setup before any writes. Old and new explicit Codex managed blocks migrate
to one FrameLab block; unrelated simple MCP tables are retained. Every changed
file gets a backup. Setup does not alter global host configurations.

Keep `.thorvision/workspace.json`, its `thorvision.workspace/1` schema, SDK imports,
recording sidecars and data identities. The CLI prefers
`THORVISION_FRAMELAB_URL`, falls back to `THORVISION_EXTENSIONS_URL`, and accepts
both `/framelab` and compatible `/analytics` device roots. Explicit CLI options
take precedence. Service migration belongs to the developer; this workstation
setup never restarts it or moves its state.

## Qualification and release boundary

- Windows x64: see [the dated verification record](../../docs/build-verification-2026-11-09.md) for the exact checks performed on this source. Offline package tests do not establish a fresh live OpenCode model session.
- Native macOS arm64/x64: pending. Source portability alone is not a passed installation test.
- Host config generation: OpenCode/Gemini/Claude/Codex fixtures are checked; this does not claim fresh live model runs on all hosts.
- Device: source baseline is recorded in stack.recipe.json; no new device inspection/restart is performed by this curation. See DEVICE-DEPLOYMENT.md.
- Desktop app: separate distribution. Use [the pinned Windows desktop build](../../docs/windows-build.md), including the required libxvc source commit and colocated agent bridge.
- Model examples/training data: optional separately versioned assets, outside the core MCP runtime. See MATERIALS.md.

The future CI job should run locked builds, package installation/doctor/configuration tests on Windows and macOS, then publish the exact kit plus checksums/provenance. Storage may be private GitHub Releases or Kontex artifact hosting. A skill may invoke the deterministic setup entry point; it should not improvise build commands. Remote cloud execution, device tunnels/authentication and a Python rewrite are separate work items.

Official host references: [OpenCode MCP](https://opencode.ai/docs/mcp-servers/), [Gemini MCP](https://geminicli.com/docs/tools/mcp-server/), [Codex MCP](https://developers.openai.com/codex/mcp).
