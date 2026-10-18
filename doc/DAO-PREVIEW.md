# DAO vertical slice: desktop and Android

Status: local development preview, 18 October 2026. No production DAO, banking,
blockchain, or LiquidFeedback result engine. No external service is deployed.

## Implemented

`services/dao` is a Python standard-library service with an authoritative SQLite
ledger. Desktop Qt and native Android clients use the same `/v1/state` and
`/v1/commands` contract. This is an initial local implementation of ADR-003,
not a port of Dyne source code or a full copy of LiquidFeedback.

- Configured members and founding issuer role; clients cannot supply their identity/role.
- Multiple named assets, integer base units, supply caps, no overdraft.
- Explicitly confirmed create/mint/transfer/own-balance burn commands.
- Transactional balanced postings including the issuance counter-account.
- Atomic command/receipt storage and actor-scoped idempotency keys. Reusing a key
  with different content is rejected. Concurrent spends serialize in SQLite.
- Personal outgoing audit, own incoming entries, own balances and aggregate supply.
- Pending commands saved BEFORE sending; retry uses the same ID, including after
  client restart. Different credentials cannot accidentally reuse a pending command.
- Android credentials remain in memory, screenshots/backups are disabled by the app.

## Run locally

Requires Python 3.10+ (verified locally with 3.14). Supply `GROUP_DAO_AUTH` as a
process environment JSON object. Its `members` map contains member IDs and roles
(`issuer` or `member`); its `tokens` map contains independent randomly generated
credentials of at least 32 characters mapped to member IDs. Do not commit or paste
actual credentials. No default password and no automatically created token exist.
Supply `GROUP_DAO_DB` as a private absolute test-database path if desired.

Start `python services/dao/server.py` from the repository. It binds ONLY
`127.0.0.1:17842`. The native API rejects browser Origins, foreign Hosts, redirects
on clients, and unauthenticated access. Do not expose it using a public proxy.

Desktop: launch the executable with `GROUP_DAO_TOKEN` set to that member's
credential. Open **DAO (test locale)** and refresh. Founding issuers see creation
and minting controls; members see only transfer and own-balance burn. Enter integer
units, never euro or decimal amounts. No action is silently sent from a chat message.

The portable smoke passed with `QT_QUICK_BACKEND=software`; a run with the default
renderer timed out on this machine. Use `script/start-desktop-preview.ps1` for the
verified rendering mode. This observation alone does not establish the root cause
of the default-renderer timeout.

Android: build with `script/build-android-preview.ps1`. This uses only the locally
installed JDK/Android SDK, without Gradle downloads, and signs with a LOCAL DEBUG
key in the ignored build directory. The APK is NOT a production-signed release.
For an emulator or a deliberately selected USB device, establish
`adb reverse tcp:17842 tcp:17842`; enter that member's credential in the app.
The only cleartext exception is loopback `127.0.0.1`, not LAN or internet hosts.

Native Android views are used for this bootstrap rather than Compose; the
platform-independent service contract remains the same. No Rust JNI/Tor group
chat core has been ported to Android yet. Desktop group messaging is unchanged.

## Verification commands

- `python -m unittest discover -s services/dao -v`
- `script/build-group-windows.ps1 -QtRoot .qt-sdk/5.15.2/msvc2019_64`
- `script/build-android-preview.ps1`
- `python services/dao/smoke_desktop.py` (actual Qt transfer against temporary ledger)
- `python services/dao/smoke_android.py --serial emulator-5554` (installed preview,
  native UI confirmation/transfer, temporary ledger; emulator only)

Run the two end-to-end client harnesses sequentially: both intentionally use the
single loopback test-service port. The Android harness cleans up its own reverse
mapping. It does not wipe application data or operate a physical handset.

Build/test outputs are deliberately ignored. Rebuild scripts never provision
cloud servers or publish a GitHub release.

## Important boundaries / remaining work

These internal test credits have no monetary redemption or legal rights.
SQLite DB and pending-command records are not independently encrypted. A machine
administrator can bypass SQLite triggers and rewrite files: this is NOT tamper-proof
audit, Byzantine consensus, or an independently verifiable public ledger. Roles are
bootstrap configuration, not yet governed/audited role changes. Each successful
mint is explicitly authorized by the configured issuer, not a DAO ballot.

LiquidFeedback Core selection, release/license audit, PostgreSQL adapter, units,
issues, competing initiatives, deliberation, quorums, scoped/transitive delegation,
preferential results and snapshot-weight policies remain to implement and compare
against reference fixtures. Governance controls are deliberately unavailable.
No token-weight default has been selected for the DAO.

Treasury approvals, policy execution, admin-role separation, exports, requests/splits,
signed receipts, durable client receipt reconciliation, encrypted key storage,
authentication lifecycle/revocation, Tor transport and Android group chat are not yet
complete. Pending-command storage failure blocks sending. Preserve a pending record
until its receipt has been reconciled; do not manually replace an uncertain command
with a fresh request ID. A lost device/profile requires server-side reconciliation.

Before production: replace this loopback HTTP development adapter with a reviewed
authenticated Tor service; define membership and charter, migrate the authoritative
ledger if needed, add backups/recovery and independent security review. Do not point
it at real-value balances or claim parity with Social Wallet/LiquidFeedback.
