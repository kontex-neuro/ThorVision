# Updater-only desktop feature

`feature/device-updater` starts directly from production `qml` at
`1523013fd5fbe598c35569676c98f7a212c6552c` (`v1.3.1`). The paired libxvc feature
starts directly from its production `qml` at `79c19ec`.

This branch adds device update selection and transfer/migration verification
to the existing update controller and dialog. No FrameLab PC runtime,
agent bridge, SDK, analysis client, sidecars, new GUI panels or setup kit is included.
FrameLab's separate Windows/macOS PC setup package is managed independently.

The reviewed modern updater source comes from the prior device-aware candidate
through `6a9d329`. The minimal paired library starts at
`8b55ac63105b8e79d1b92955674df357d3b59614`, with reviewed updater corrections
on that branch; no camera, capture, analytics,
recorder or endpoint module is imported. Its only non-updater change is a
compiler-portability lambda spelling in the existing validator, without schema
or validation-policy changes.

Modern AIO/NVR devices select only matching product/profile packages from the
device-aware catalog. Installed application build and API compatibility are
checked separately. HTTPS, size and SHA-256 protect downloads. New application
packages can require a matching updater migration first. Success requires durable
matching receipts, updater readiness and the installed application/device health.
This updates the device application, not the separate FrameLab agent setup on PC.

On a factory device with a compatible API, **Check for Device Updates** asks
whether the connected unit is an AIO or an NVR. An automatic check also asks
when the selected channel advertises a product release. Selecting the model
looks up matching packages. **Update Now** starts the transfer. A missing
compatible migration is reported for the selected product. On AIO the bootstrap
checks the Jetson hardware and old-updater fingerprint before installing.
After migration, the updater identifies the device even if the old videoserver
is still running. Desktop forgets manual choices on disconnect, so another unit
connected at the same address gets its own check.

The existing GUI has no added DEV menu. Process setting
`THORVISION_DEVICE_UPDATE_CHANNEL=dev` routes the existing update check to DEV;
absent settings default to `release`, and an explicit `release` is supported.
Any other value, including an explicitly empty value, is refused visibly without
selecting or installing a package. This setting changes only the current process.
The DEV channel uses the same product choice and Update confirmation. DEV
installation also requires device-side maintenance authorization.

The Windows DEV updater bundle contains `Start-ThorVisionDevUpdate.ps1` beside
`bin/ThorVisionDevUpdater.exe`. Run `.\Start-ThorVisionDevUpdate.ps1 -DeviceAddress <IPv4>`,
and verify the address in the window title as the availability check runs. Review
the device and package versions before selecting **Update Now**. The launcher
sets `THORVISION_DEV_UPDATE_HOST` for its child process. This opens the update
dialog independently of the recording and camera clients. A modern DEV package
must match the selected device and its release profile.

Initialize the pinned library with `git submodule update --init third_party/libxvc`.
Use `THORVISION_LIBXVC_SOURCE` to test a different local checkout and
`THORVISION_BUILD_UPDATER_CHECKS` for the non-GUI selection fixtures.
The publishing workflow uses `dev-vX.Y.Z` for development builds and `vX.Y.Z`
for production releases.

The incorrect local `feature/framelab` branches were removed at the user's request
after verified bundle restoration. Their worktrees remain detached recovery
checkpoints. Neither their code nor their standalone runtime kit enters this branch.

The private handoff does not qualify updater migration or recovery. Modern
verification failures show an unverified outcome, without claiming rollback.
AIO always requires device FrameLab health; NVR requires it when the installed
descriptor advertises FrameLab support. NVR hardware qualification remains pending.
An unsupported API reports no compatible update and keeps streams blocked.
The unchanged dialog cannot bypass a required compatibility update. Reconnect
may briefly wait for an outstanding network request; no GUI responsiveness
qualification is claimed.

The offline pilot requires updater 1.2.0 and PC-verified transfer support. The PC
fetches catalog/archive hashes over HTTPS; the device validates the uploaded bytes,
metadata and actual profile locally, without a cloud lookup or internet gateway.
The new cohort is 0.1.6; the immutable 0.1.5 migration uses the superseded online
policy and is not an offline-test candidate. DEV channel selection is explicit
through `THORVISION_DEVICE_UPDATE_CHANNEL=dev` in a separate manual-test launcher.
The normal launcher keeps its production default and no installation runs until
the operator selects Update in the existing GUI.

Windows cloud requests use the Windows certificate store through the shared
libxvc HTTPS policy, with certificate and hostname verification enabled. The
private test must exercise real catalog and archive downloads; successful LAN
upload fixtures alone do not qualify HTTPS. Connection errors retain their
underlying cause in the existing dialog so PC cloud failures can be distinguished
from device connection failures.
