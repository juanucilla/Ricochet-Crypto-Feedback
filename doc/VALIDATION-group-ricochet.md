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

## DAO desktop/Android vertical slice (18 October 2026)

- SQLite ledger/API: 7 tests pass, including concurrent spends, role checks,
  caps, integer validation, replay/conflicting IDs, atomic accounting and restart.
- Qt CTest: 3/3 suites pass (existing group core/UI plus new DAO QML loading).
- Real Qt client -> temporary authenticated local service -> ledger transfer:
  25 units booked, balances 75/25; populated DAO screen visually inspected.
- Android SDK build, local debug signature verification and emulator installation:
  passed. Native Activity starts; application crash log empty during the check.
- Android UI input, authenticated refresh, transfer confirmation and updated
  balance against a temporary ledger: passed, balances 75/25.
- Final updated portable app `--smoke-test`: exit 0 and UI ready with software
  rendering. A default-renderer run timed out before UI ready; cause not established.
  The provided start-desktop-preview.ps1 uses the verified software mode.
- The first Android UI automation attempts failed because input events closed
  the Activity or lost credential characters during IME startup. The harness now
  waits for focus and enters short chunks. These were not treated as successful
  end-to-end tests. An early emulator System UI stall was also observed.

These are LOCAL, fake-credit tests. They do not establish Tor connectivity,
production security, independent audit, physical-device compatibility,
LiquidFeedback parity or Android group-chat operation. See DAO-PREVIEW.md.
