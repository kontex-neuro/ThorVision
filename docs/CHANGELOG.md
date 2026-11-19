# Changelog

All notable changes to ThorVision will be documented in this file.

The format is based on Keep a Changelog, and this project uses strict SemVer release tags.

## [Unreleased]

## [v1.3.2]

### Changed

- **Check for Device Updates** asks whether an older device is an AIO or an NVR when it cannot identify itself. Selecting a model shows the matching update. **Update Now** starts installation.
- Desktop can upgrade a factory AIO running videoserver 0.1.3 directly to a later compatible release. It installs the updater supplied with that release first and verifies the device type, updater version and readiness before sending videoserver.

### Fixed

- Windows and macOS installers include the video preview plugin that Desktop needs to open its main window.
- If an upgrade stops after ThorUpdateServer restarts, Desktop reads the device identity from the new updater on the next check, even while the old videoserver is running.
- When a factory device has no matching migration package, Desktop identifies the selected model in the message. Selecting NVR cannot offer an AIO package.

## [v1.3.1]

### Changed

- Windows installers are now digitally signed.

## [v1.3.0]

### Added

- Added H.265 video streaming support.
- Added a device-server firmware update workflow with compatibility checks, progress and log views, and recovery handling.
- Added automatic discovery and export of Kontex Conan recipes from the application's declared dependencies.

### Changed

- Upgraded libxvc to 0.4.0 and xdaqmetadata to 0.3.1.

## [v1.2.2]

### Changed

- Added CI/CD release publication for ThorVision installers.

## [v1.2.1]
