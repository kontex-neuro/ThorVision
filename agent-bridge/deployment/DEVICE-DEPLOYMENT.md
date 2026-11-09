# Device prerequisite and developer deployment boundary

Workstation setup configures a client of an already deployed device. It does not install/restart services or restore product SSH access. Device system provisioning remains developer-owned. This document records the known source baseline and what must be qualified; it is not a newly tested clean-device image installer.

## Versioned sources

- Repository: https://github.com/kontex-neuro/videoserver/tree/feat/framelab
- Recorded baseline: 57581b8e8d294b895a9dab6ed4dc5c961b2c3f9f, published after the Nov3 audit.
- Canonical runtime: `extensions/runtime/thorvision_runtime/` (ingest/decode/worker/sandbox/store/preparation/proxy/cleanup/server).
- SDK and examples: `extensions/sdk/`.
- Device gateway: `src/api/framelab_proxy.py`, mounted by `src/server.py`; native clients use `http://DEVICE_IP:8000/framelab`.
- Service source: `extensions/runtime/systemd/thorvision-framelab.service`, `thorvision-framelab.slice`, `thor-vision-framelab.conf`.
- Camera/recording/display prerequisites: existing videoserver native capture and recording targets, `/run/pi-display` integration and base `thor-vision.service` deployment.

The commit above is the historical pre-FrameLab baseline. The listed FrameLab service/gateway names describe the new integration branch; the actual deployment must record its source tree and verification independently. At the earlier read-only device audit, installed analytics service/drop-in files matched the old-name tracked sources. That source assumed a home-directory `videoserver-loop-recording` checkout and system `/usr/bin/python3`; paths must be adapted by a deliberate device deployment, not copied blindly from this PC.

## Required runtime environment

The device needs its supported Linux/ARM64 capture environment, system Python with GStreamer/PyGObject and Pillow, bubblewrap, user systemd scopes, and preparation prerequisites pip/packaging/venv/prlimit. Its camera API and analytics gateway must support HTTP and WebSocket forwarding. Exact OS/native multimedia artifacts and package versions must be supplied by the device build/image release; this workstation recipe does not claim to reconstruct those from a generic OS.

Separate the system runtime, backend API environment and per-extension prepared dependency environments. The callback worker receives read-only SDK/package/environment mounts, bounded memory/CPU/tasks/temp storage and no network. A trusted preparation process may resolve wheels/model files through the temporary PC proxy and install verified artifacts offline. Original recording storage and provider credentials are outside that callback environment.

The baseline advertised SDK0.1.0/protocol1. The existing implementation's reviewed limits include 1GiB worker address space, 64 tasks, 2 CPU quota cores, 1.5GiB unpacked dependency allowance and 512MiB free-space reserve. Treat capabilities and effective configuration as authoritative at deployment time; these historical values are not a command to raise or overwrite limits.

## Developer deployment checklist

1. Select/pin the compatible videoserver build, SDK contract and native capture/runtime image. Verify required system tools and sandbox enforcement using the backend's own probes/tests.
2. Configure paths, service user, API port, multimedia runtime env and recording env for that image. Install the versioned service/slice/drop-in templates using the device deployment process.
3. Review effective recording/display/storage settings before any authorized service restart. A mounted recorder requires its established safe lifecycle; a workstation installer must never improvise unmount/reboot behavior.
4. Verify `GET /framelab/v1/capabilities`, state/catalog and preview/events WebSockets. Check preparation/reuse and cleanup plan protections against the intended configuration.
5. Run backend SDK/runtime/API tests, then an explicitly requested SDK-only prepare/test and human-reviewed live Run. Confirm original frame identity and recorded analytics diagnostics, not just an HTTP200.
6. Record device OS/image/native artifact version, backend commit, effective limits and verification results with the stack release. A PC doctor compatibility check is only one stage of this qualification.

Do not promote the prior SSD boundary-test override (`zzz-boundary-reserve.conf`, a specific SSD UUID and ~1.97TB reserve) into production defaults. Keep model artifacts, active/rollback state, recordings and machine-specific paths outside generic source recipes.

The current gateway inherits the existing trusted-LAN API access model. Internet-facing or multi-user authenticated operation needs a separate gateway/authentication design; publishing install artifacts in the cloud does not require exposing the camera API there.

FrameLab migration retains `/analytics` as a direct compatibility API alias and
the old service name as an alias to one runtime where safely configured. Keep
the current state store, socket paths, limits, package/model identities and
recording `.analytics` formats. New `THORVISION_FRAMELAB_*` settings support old
`THORVISION_ANALYTICS_*` values; conflicting values require explicit correction.
After runtime changes, reuse compatible exact dependency environments and
rebind/retest candidates instead of preserving stale readiness or reinstalling
everything. The generic ThorUpdateServer and its names are not FrameLab assets.
