# Local validation — 2026-10-18

Target: Windows x64, Visual Studio 18 / MSVC 19.50, Qt 5.15.2,
Rust backend in release mode.

- Full `ricochet-refresh-qt` CMake target: passed, produces `group-ricochet.exe`.
- Local portable Qt deployment: passed with `windeployqt`.
- Group core tests: 11 passed, 0 failed (9 scenarios plus setup/cleanup).
- QML loading/rendering with a populated conversation and quoted reply: passed
  without QML engine warnings, including after the variable-height layout fix.
- Final portable application's `--smoke-test` with an isolated profile: exit 0;
  application, backend, profile and UI initialization completed.
- `git diff --check`: passed.

Core scenarios: delayed identity initialization; invitation consent; owner cannot
activate an unaccepted invitation; three-peer fanout and per-recipient delivery;
rejection of outsiders/malformed/oversized content; offline retry, deduplication
and restart persistence; drafts, mute, unread, search and replies; group closure;
unknown contacts; corrupt storage preservation; disk-write failure without ACK.

The full build emits existing dependency/Qt 5 and upstream source warnings; it
is not a warning-free build. Windows compatibility fixes include CMake-managed
environment variables, copied headers, corrected generated-header output path
and the MSVC C++20 preprocessor option.

The local portable directory includes Tor 0.4.9.13 copied from the user's existing
Tor Browser installation, with its supplied licence directory. This executable
was checked with `--version`, not independently downloaded or certified here.

Not verified: real Tor circuits between group participants, long-running
reconnection/load tests, other operating systems, independent security review,
complete translation coverage, or redistribution compliance for a public binary
release. The implemented fixed-roster application protocol differs from the
future native-channel design in ADR-001; see GROUP_RICOCHET.md for limits.
