# Changelog

All notable user-facing changes are documented here. Version numbers follow
[Semantic Versioning](https://semver.org/).

## [0.2.5] - 2026-09-21

### Packaging

- Wrapped the RPM long description so Fedora rpmlint accepts the package metadata.
- Added rpmlint to the producer release gate so packaging errors are caught before Ro-Repo acceptance.

## [0.2.1] - 2026-09-07

### Improved

- Made the dashboard and maintenance pages usable at compact desktop sizes,
  including 1366×768, without clipped or overlapping controls.
- Reworked the information dialog so its colors, contrast, and actions match
  both light and dark themes.
- Replaced the dashboard placeholder with the bundled Ro-ASD vector logo.
- Moved system-risk checks off the UI thread to keep the application responsive.
- Hardened system command execution and package validation.

### Packaging

- Added separate Fedora RPM release assets for x86_64 and aarch64.
- Completed AppStream metadata for Discover, including project license,
  developer identity, clear descriptions, and content rating.
