# ThorVision FrameLab standalone MCP

Native agents use this Node.js package to access a developer-deployed ThorVision analytics API and the bundled local control WebUI. No desktop application, PC Python environment, provider SDK bundle or source checkout is required after installation. Node.js 22+ and the chosen native agent are prerequisites. Agent sign-in stays with that agent.

The package contains the tested MCP implementation, a pinned npm shrinkwrap, WebUI, SDK reference, portable analytics skill and hashed release manifest. It is currently an internal local release candidate, not a published npm registry package.

After installation, run the package CLI using its absolute path:

```sh
node /path/to/installed/bin/thorvision-framelab.mjs setup --agent opencode --workspace /path/to/experiment --device-url http://DEVICE_IP:8000/framelab
node /path/to/installed/bin/thorvision-framelab.mjs doctor --workspace /path/to/experiment --offline
```

`setup` supports opencode, gemini, claude and codex. It generates one `framelab` MCP registration, installs `thorvision-framelab` plus the thin `thorvision-analytics` alias in ~/.agents/skills (plus project .codex/skills copies for Codex), and saves .thorvision/workspace.json. Existing JSON settings and unrelated simple MCP tables are preserved. Old owned `thorvision` entries/managed Codex blocks migrate without a duplicate server. Changed or unowned FrameLab/ThorVision entries require review before any writes. Files are backed up; known managed skills can upgrade, while a different unmanaged skill requires review and --replace-skill. Existing OpenCode JSONC and ambiguous inline/root Codex MCP declarations require a manual merge. Setup never changes provider accounts, model selection or device services.

`doctor --offline` checks package files, starts the actual MCP subprocess, reads the SDK contract and checks the local WebUI assets. Without --offline it also performs a read-only device capabilities request and checks SDK/protocol compatibility. It never prepares, activates or deletes packages. `--json` is accepted; diagnostic output is JSON in either mode.

Start your chosen agent from the configured workspace, trust that workspace if requested, sign in/select a model, and ask it to use thorvision-framelab. The agent calls open_control_panel to return a session-local link. The human reviews and clicks Run on camera after prepare_extension passes. Keep the session open while using its panel.

MCP communication uses stdio; diagnostic messages must not be written to stdout while serving. `serve` accepts --workspace / --device-url / --camera, THORVISION_FRAMELAB_URL (preferred over THORVISION_EXTENSIONS_URL), existing THORVISION_WORKSPACE / THORVISION_CAMERA settings, or the saved workspace record. Explicit CLI options win. Both `/framelab` and `/analytics` roots remain supported. The old `thorvision-mcp` CLI delegates to the canonical `thorvision-framelab` implementation; it does not start a second server. Runtime dependencies and model packages are prepared on the device through automatic preparation, validation and testing, with supported downloads through a temporary PC proxy. Original recordings and model datasets are not shipped in this package.

For upgrades, install another immutable runtime prefix and rerun setup for each workspace. Previous versions remain available for rollback. No automatic update, provider login, model request, Git push or device restart occurs.

Windows fresh-install tests and other platform status belong to the accompanying release verification report. The portable source/config format does not establish native macOS validation.
