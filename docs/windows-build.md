# Build the FrameLab desktop on Windows

Build the desktop source at the supplied Git commit and the exact libxvc commit in
[`sources.json`](../build-support/windows/sources.json). This procedure recreates
the application from source with frozen third-party inputs. It does not promise
an identical executable hash across build directories, machines, or timestamps.

The pinned script below is the verified Windows build route. The legacy root
Conan dev reference and tag-triggered release workflow do not use these pins.
Pushing the feature branches publishes source. It does not publish an installer
or deploy an application.

## Install the prerequisites

Use Windows x86-64 with these versions:

- Visual Studio 2022 C++ tools, MSVC 19.40.33813, and Windows SDK 10.0.22621.0.
- Qt 6.8.0, `msvc2022_64`, including Qt Quick, Quick Controls, SVG, and WebSockets.
- GStreamer 1.26.8 MSVC x86-64 runtime and development installers.
- Python 3.12.4, Conan 2.24.0, Node 24.19.0, and npm 11.17.0.

The build script checks these versions. It obtains CMake 3.29.7 and Ninja 1.13.2
through the frozen Conan package list. Supply an actual Python executable, not a
version-manager shim without a selected interpreter.

The Qt GStreamer sink is preserved in
[`third_party/windows/gstreamer`](../third_party/windows/gstreamer/README.md).
The script checks its SHA256 and copies it into the build output. Read that
directory's provenance and license notes for the binary's source-history limits.

## Check out the source

Use short checkout paths to stay within Windows tool path limits. GitHub access
to the two repositories is required.

```powershell
git clone --branch feat/framelab https://github.com/kontex-neuro/xvc.git C:\src\xvc
# Check out the full desktop commit supplied in the build handoff.
git -C C:\src\xvc checkout <desktop-commit>
git clone --branch feat/framelab https://github.com/kontex-neuro/libxvc.git C:\src\libxvc
git -C C:\src\libxvc checkout d1a32a2df97df298ee31677de66eea04aebac92e
```

The libxvc working tree must have no tracked changes. Its final commit includes
the FrameLab compatibility document. No device backend checkout or connection
is required to compile or run the local build checks.

## Build and check the application

Run PowerShell from the desktop checkout. Adapt only the installed tool paths:

```powershell
Set-Location C:\src\xvc
.\scripts\build-windows.ps1 `
    -QtRoot C:\Qt\6.8.0\msvc2022_64 `
    -GStreamerRoot C:\gstreamer\1.0\msvc_x86_64 `
    -Python C:\Python312\python.exe `
    -LibxvcSource C:\src\libxvc
```

Conan must be installed beside that interpreter at `Scripts\conan.exe`, or passed
with `-Conan`. Supply `-VisualStudioRoot` if Visual Studio discovery selects a
different installation. Use `-BuildDirectory C:\build\ThorVision` to choose a
short output path, and `-Jobs` to change compilation parallelism.

The first run needs access to ConanCenter, the upstream dependency source
archives named by its locked recipes, the checksum-pinned metadata archive, and
the npm registry. Each build directory has its own `conan-cache` directory.
The script does not use a neighboring project's generated toolchain or its
existing dependency cache.

[`conan.lock`](../build-support/windows/conan.lock) pins dependency recipe
revisions. [`packages.json`](../build-support/windows/packages.json) pins the
retrievable binary revisions. Entries with `download: false` use the locked
recipe and must produce the recorded package ID. The metadata recipe verifies
and unpacks its frozen binary archive. The other such entries compile source.
The script
downloads frozen inputs first, resolves the Conan graph without remote lookup,
and verifies the result before compilation.

The build compiles libxvc into ThorVision, runs the filename test and the local
ExtensionClient API transport fixture, then installs and tests the locked agent
bridge. The bridge, its runtime dependencies, the SDK snapshot, and Node are
copied beside the executable. The build does not launch ThorVision or connect
to a camera. `-SkipBridge` is only an intermediate native-build check and does
not produce the complete application output.

## Inspect and start the result

The default output is `build\AgentWorkflow\ThorVision.exe`. Its
`build-receipt.json` records the desktop commit, tracked source changes, libxvc
commit, dependency lock hash, executable hash, and installed runtime paths.
`conan-graph.json` records the resolved package revisions and options.

Check the output from a fresh PowerShell session:

```powershell
.\build\AgentWorkflow\run-thorvision.ps1
```

This validates the native sink, pinned Node, and colocated bridge. To open the
application when you are ready to use your selected device, run:

```powershell
.\build\AgentWorkflow\run-thorvision.ps1 -Launch
```

The launcher sets Qt and GStreamer search paths and explicitly selects the
copied Node executable and bridge. Qt and the GStreamer SDK installation remain
runtime prerequisites. This output is a source-build directory, not a standalone
installer. Native provider login remains a separate user action.

Device deployment and the standalone FrameLab tool runtime are covered by
[`agent-bridge/deployment/README.md`](../agent-bridge/deployment/README.md).
