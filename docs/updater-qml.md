# Updater-only desktop feature

`feature/device-updater` starts directly from production `qml` at
`1523013fd5fbe598c35569676c98f7a212c6552c` (`v1.3.1`). The paired libxvc feature
starts directly from its production `qml` at `79c19ec`.

This branch changes only device update selection, transfer/migration verification
and the existing update controller. Production QML, visual assets, camera/SRT/PC
recording code and publishing workflows remain unchanged. No FrameLab PC runtime,
agent bridge, SDK, analysis client, sidecars, new GUI panels or setup kit is included.
FrameLab's separate Windows/macOS PC setup package is managed independently.

The reviewed modern updater source comes from the prior device-aware candidate
through `6a9d329`. The minimal paired library is
`8b55ac63105b8e79d1b92955674df357d3b59614`; no camera, capture, analytics,
recorder or endpoint module is imported. Its only non-updater change is a
compiler-portability lambda spelling in the existing validator, without schema
or validation-policy changes.

Modern AIO/NVR devices select only matching product/profile packages from the
device-aware catalog. Installed application build and API compatibility are
checked separately. HTTPS, size and SHA-256 protect downloads. New application
packages can require a matching updater migration first. Success requires durable
matching receipts, updater readiness and the installed application/device health.
This updates the device application, not the separate FrameLab agent setup on PC.

Unidentified legacy devices keep production's legacy catalog policy, including
required-series updates and recalled-build handling; they are not assumed AIO.
An exceptional legacy AIO migration is available only with the explicit process
setting `THORVISION_ALLOW_LEGACY_AIO_MIGRATION=1` and confirmation through the
existing update dialog. The bootstrap independently checks the actual AIO hardware
and supported old-updater fingerprint. This setting is not enabled by the private
ordinary-function test launcher. Unsupported API compatibility remains blocked
even if a cloud check fails. Required updates cannot be ignored to start streams.

The existing GUI has no added DEV menu. The controller retains a development-check
entry point for deliberate maintenance; routine checks use production packages.
Current AIO0.1.5 is DEV only, and no production migration qualification is claimed.

Use `THORVISION_LIBXVC_SOURCE` for the matching private source build and
`THORVISION_BUILD_UPDATER_CHECKS` for the non-GUI selection fixtures. Before later
publication, pin/package that libxvc revision, select the desktop release version
and pass operator/platform/update acceptance. No desktop publication is performed
or authorized in this pass; the operator controls the first application launch.

The incorrect local `feature/framelab` branches were removed at the user's request
after verified bundle restoration. Their worktrees remain detached recovery
checkpoints. Neither their code nor their standalone runtime kit enters this branch.
