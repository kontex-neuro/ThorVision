# Windows desktop build verification, 2026-11-09

This record covers the Windows source build in this desktop commit and libxvc
`d1a32a2df97df298ee31677de66eea04aebac92e`. The build procedure and required tools
are in [the Windows build guide](windows-build.md). The generated build receipt
records the actual desktop commit, dependency identities, tests, and executable
hash for each invocation.

## Inspected application

The original application was
`C:/Users/kontex/Desktop/project/xvc-loop-recording/build/AgentWorkflow/ThorVision.exe`.
Its size was 9,781,248 bytes and its SHA-256 was
`61c9f9b18b6ae87ed68806d80039c1d16c55a688a216437a61a2d44bbae36cdb`.
Its file modification time was 2026-11-04 19:03:03 in Asia/Taipei. A process was
running that exact path during the initial inspection. The original process,
build directory, and source worktrees were left in place.

The source publication preserves the reviewed local FrameLab changes,
deployment documents, compatibility documents, plugin, and design text.
Historical review screenshots containing laboratory footage are excluded.
SDK examples keep LF line endings because their package digests include file
bytes. The first clean Windows checkout exposed two digest-test failures from
Git CRLF conversion. The checkout rule fixed those failures without changing
the sealed SDK examples or their expected digests.

## Local checks

| Check | Result |
| --- | --- |
| Locked agent-bridge install and TypeScript build, including Codex protocol generation | Passed |
| Agent-bridge tests | 53 passed, 0 failed |
| Claude FrameLab plugin manifest validation | Passed; optional catalog icon absent |
| Claude FrameLab panel tests | 13 passed, 0 failed |
| Standalone runtime build using the committed skill snapshot | Passed |
| Standalone tests against the staged runtime | 18 passed, 0 failed |
| Fresh tarball installation outside the source tree, with locked production dependencies | Passed |
| Standalone tests against that installed runtime | 18 passed, 0 failed |

The standalone tests exercise an actual MCP handshake, SDK access, local WebUI
assets, configuration preservation and migration, integrity failures, and
localhost device fixtures. They do not use a physical camera or provider login.
The default standalone build needs no neighboring ai-skills checkout.

## Native source build

A fresh configure and all 207 native build steps completed with an initially
empty, build-local Conan cache. Libxvc compiled from its pinned Git checkout.
The filename regression test and the localhost ExtensionClient API transport
fixture passed. No previous desktop build directory or generated toolchain was
used.

The fresh retrieval check found that several binaries in the original developer
cache were unavailable from ConanCenter. The recorded package list selects
retrievable revisions for downloaded binaries. Fmt, spdlog, cpr, libcurl, oatpp,
and the metadata wrapper build from their locked recipes. The metadata wrapper
verifies the original archive's SHA-256, and its DLL matches the inspected
application's metadata dependency.

The default build script also builds and tests the bridge and copies its locked
runtime beside the executable. `build-receipt.json` identifies a full run with
`bridgeIncluded: true` and records the actual desktop commit and source status.
The output launcher's default mode validates executable, metadata, Node, and
the actual loaded Qt GStreamer plugin without starting the application.

## Scope

This is a repeatable application source build using frozen third-party inputs.
It is not a claim of byte-for-byte executable output. Compiler paths and binary
metadata can differ. The prebuilt Qt GStreamer plugin is preserved with its
SHA-256 and installed notices. Its original source revision is unknown, as
recorded in [the plugin provenance](../third_party/windows/gstreamer/README.md).

No new device deployment, camera activation, SSD recording test, provider login,
or live model run was performed. Native macOS qualification remains pending.
