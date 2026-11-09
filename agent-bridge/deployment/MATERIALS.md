# Where the deployment materials live

Curated 2026-11-04. The source repositories are canonical. Installed copies, experiment data and historical audit artifacts have distinct roles. No account credentials are included in this inventory. Repository paths below refer to this PC; they are not baked into generated release packages.

| Layer | Canonical source | Existing location / state | How reproduced |
| --- | --- | --- | --- |
| Native MCP + browser controls | ThorVision `agent-bridge/src/{tools/server,device,device-url,package,preparation,preparation-proxy,control,control-server}.ts`, `control-ui/`, SDK snapshot | `C:/Users/kontex/Desktop/project/xvc-loop-recording/agent-bridge` | New standalone recipe and shrinkwrap under `deployment/`; versioned release kit. |
| Desktop provider bridge | Same repo `agent-bridge/src/main.ts`, adapters, generated protocol | Bundled Codex/Claude/Gemini dependencies and existing desktop build | Original npm build; separate from minimal native MCP. |
| Portable analytics skill | Versioned snapshot in `deployment/skills/`, upstream ai-skills | Original curation from `C:/Users/kontex/Desktop/project/ai-skills`; provenance in `skills/SNAPSHOT.json` | Default package builds use the snapshot in this desktop commit. Setup installs the packaged copy. |
| Installed skill | Generated installation | `C:/Users/kontex/.agents/skills/thorvision-framelab` | Keep existing working copy until deliberately upgrading; older `.codex/skills` / `.gemini/skills` duplicates may differ and should be reviewed. |
| Working OpenCode workspace | User-owned experiment/config | `C:/Users/kontex/Desktop/project/test_opencode/opencode.json`, `package/`, `history/` | New workspace setup selects a runtime and device. Do not bundle personal provider settings or experiment history. |
| Native Claude panel/mod | ThorVision `agent-bridge/plugins/thorvision-framelab/` and marketplace metadata | Source 0.3.1; historical installed-cache drift was documented in the Nov3 audit | Optional Claude plugin distribution; not required by shared browser WebUI. Validate plugin/runtime version pair separately. |
| Qt desktop / PC recording | ThorVision `thorvision/`, shared libxvc | `.../xvc-loop-recording/build/AgentWorkflow/ThorVision.exe`; `.../libxvc-loop-recording` | Optional C++/Qt build; source/runtime commits recorded separately. |
| Device capture/API/runtime/SDK | videoserver `src/`, `extensions/`, native capture/recording/display source and service templates | Last audited `/home/kontex/videoserver-loop-recording`, baseline57581b8 | Developer provisioned; DEVICE-DEPLOYMENT.md describes the boundary and required source files. |
| Models and isolated dependencies | Device analytics package/preparation store | Runtime-generated on device | Derived from callback manifest, model checksums and platform-compatible locks; not copied from the PC or shipped as generic venvs. |
| Toy-mouse/arena demo | Currently experiment artifacts, not yet a maintained SDK example source directory | Native project `C:/Users/kontex/AppData/Local/ThorVision/native-projects/claude-camera/package`; chat `outputs/toy-mouse-training/` | Retain source/training/labels and model provenance separately. Exact tested package/model identities below. |
| Earlier setup bundle | Historical bootstrap, superseded by this source recipe | Chat `outputs/opencode-workspace-setup/` | Previously used machine-specific PowerShell script/JS validator. Keep as evidence; don't use its hardcoded source path as a product installer. |
| Reports / slide demos | Engineering evidence, not runtime source | `C:/Users/kontex/Desktop/project/report/video_materials/thorvision-analytics-demo`, chat `outputs/pre-push-audit-2026-11-03/` | Preserve versioned QA/release evidence; don't ship whole report/temp folders in the runtime. |

FrameLab paths above are the canonical source/target installation names for the
0.2.0 migration. Historical installed `thorvision-analytics` skills and
`thorvision-control` plugin caches may still be present until an explicit host
upgrade. Their presence is not evidence that the new version was loaded. Keep
historical experiment/report directories and immutable artifact IDs unchanged.

## Source baseline

Prior published feature HEADs: ThorVision e4575dd4781fa11f3be98eacbde10bc7f45824f6; libxvc5c609cd0f50b80f4db063c155edb309632139707; videoserver57581b8e8d294b895a9dab6ed4dc5c961b2c3f9f. SDK snapshot explicitly derives from videoserver86bba58, SDK0.1.0/protocol1/schema34a34e64…; don't relabel it as the newer backend commit.

The 2026-11-09 desktop source publication preserves the standalone tooling, deployment documents, plugin sources, and portable skill snapshot. Libxvc `feat/framelab` is pinned at `d1a32a2df97df298ee31677de66eea04aebac92e`, whose only change from the implementation baseline above is its compatibility document. Use [the Windows build guide](../../docs/windows-build.md) for the complete pinned desktop build. Release metadata records the actual source tree hashes and dirty state. Source publication does not publish a package registry release or deploy a device.

## Optional toy-mouse artifact identity

- Package: arena-zone-dwell1.1.0, package digest d1bef939ff008ab7bc0043363c34c3a2e939cc74ad87d894e0fd00d7dc0ac354.
- Delivered ZIP: chat `outputs/toy-mouse-training/arena-zone-dwell-toy-mouse-package.zip`, archive SHA-256 c8187d1b855b6a7b964171fb3834cb8015724622acccc37ad76d1d024c386eb6.
- ONNX model: SHA-256 eef91f5a5ab198e5e3a68d298cfe0f2afe35616efb4e6072df771837a19ed6e1; manifest records AGPL-3.0 and local fine-tuning provenance.
- Labeled dataset, annotation/training/evaluation/replay scripts and requirement lock: chat `outputs/toy-mouse-training/`. Fixtures and camera footage need their chosen sharing scope preserved.

These are historical verified identities, not a new model validation. Until the example is curated into a maintained source/artifact destination, it is an optional reproducibility gap. Core assistant setup and the bundled SDK rectangle example do not depend on these toy-mouse assets.
