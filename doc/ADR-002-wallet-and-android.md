# ADR-002: Social wallet integration and Android client

Status: Proposed; internal DAO credits selected by Giovanni on 2026-10-18

Date: 2026-10-18

Decision owner: Giovanni Natella

## Context and verified sources

Giovanni requested a Social Wallet-inspired payment feature inside group-ricochet,
excluding blockchain integration, and an Android application.
He subsequently selected internally issued and administered credits representing
DAO tokenisation, requested LiquidFeedback-like governance, and included the
Social Wallet admin console. See ADR-003 for this expanded scope. Euro settlement
below remains an alternative considered, not the selected implementation.

Dyne's Social Wallet API explicitly supports a database-only configuration:
https://github.com/dyne/social-wallet-api. Its documented database features
include transaction creation, balances, labels and metadata; blockchain deposit,
withdrawal and address operations are separate. The existing implementation uses
Clojure/MongoDB; the concepts can be adapted without copying that architecture.

The user linked the Dyne organisation, so these repositories are the identified
likely references, not a user-confirmed selection of a specific revision:
https://github.com/dyne/social-wallet-api and https://github.com/dyne/social-wallet.
Do not copy source before checking the actual licences of the selected files.

## Decision proposed

Separate chat transport, wallet domain logic, and settlement authority. Share
Tor connectivity, user-approved peer associations and message presentation.
Never infer a payment or a balance update from a chat delivery acknowledgement.

For closed-community credits without blockchain, use one authoritative ledger
service per community, exposed as an onion service with no public HTTP ingress.
Clients send authenticated transaction intents; the ledger performs serialised,
atomic double-entry postings and returns a verifiable receipt. The operator and
its availability remain trust assumptions. Chat remains direct peer-to-peer;
payment settlement introduces a separate service and different metadata exposure.

If the intended asset is redeemable euro value, a database entry alone does not
perform bank settlement. A separate approved payment/settlement integration and
its account model are needed. Do not provision providers, collect funds, create
accounts, or authorise payments as part of this design task.

## Options considered

| Option | Benefit | Cost and assumption |
| --- | --- | --- |
| Community credits with authoritative ledger | No blockchain or chain APIs; straightforward accounting | Trust in ledger operator, issuer and redemption rules |
| Payment-provider-backed euro transfers | Real settlement through a payment rail | Provider integration, identifying data and external dependencies |
| Peer-signed IOUs only | Bilateral promises can travel directly | Records debt; does not create globally spendable, double-spend-resistant money |
| Peer-replicated transferable balance | No single ledger operator in principle | Requires explicit trust/consensus/conflict rules; copying chat state is insufficient |

No choice of Tor transport itself eliminates double spending. A distributed
authoritative ledger need not be a blockchain, but its agreement mechanism
cannot be omitted. Offline electronic cash is a separate research/security task.

## Protocol boundary

Keep protocol roles distinct even if multiplexed on an authenticated connection:

- Chat: payment requests, split-expense proposals and optional receipt references.
- Wallet authority: intent submission, status lookup, balance and ledger queries.
- Payment rail adapter: absent in community-credit mode; separate in euro mode.

Use a new negotiated, versioned feature for financial messages. Do not put money
commands in the existing experimental `group-ricochet/v1` text envelope. The
current preview's 2,000-UTF-16-unit limit and fixed-roster semantics are unsuitable
as a financial protocol. Removing blockchain APIs still leaves an internal
service contract, whether REST over Tor or a dedicated framed binary protocol.

Ledger invariants:

- Integer minor units with explicit currency/issuer and scale; no floating point.
- Unique client intent ID and server transaction ID; idempotent retries.
- Authorisation bound to payer, payee, amount, currency, nonce and expiry.
- Separate wallet signing credentials from onion-service identity keys.
- Atomic debit/credit and limits under concurrency; no negative balance unless
  the selected credit scheme explicitly authorises it.
- Receipts tied to a pinned/authenticated authority and exact intent.
- Append-only audit events; reversals are new entries, not silent history edits.
- Explicit states: request, authorised, pending, settled, failed, expired, reversed.
- Reconciliation by intent ID after timeout; never retry with a new payment ID
  until the prior outcome is resolved.

Key rotation, recovery, ledger receipt format and service authentication require
a reviewed security specification before real funds or redeemable credits.

## Chat UX

Composer actions: Request, Send, Split expense. A request becomes a plain,
understandable card with amount, currency, requester, beneficiary, description
and expiry. A received request never authorises a debit. A separate review
screen confirms beneficiary and amount before authorisation.

Split-expense proposals specify participants and deterministic integer rounding;
each participant authorises their own share. No automatic debits for joining a
group. Group membership removal/closure never deletes transaction records or
changes their settlement state. Chat identity linking to a wallet requires
explicit consent, with balances and receipts private by default. The community
ledger can observe transactions it settles despite Tor transport.

Render “request delivered” separately from “payment settled”. Offline submissions
remain pending; local display does not imply receipt of spendable value.

## Android architecture

Proposed client: Kotlin/Jetpack Compose, a native Rust bridge for the existing
Ricochet protocol, encrypted local storage appropriate to the platform, and
Tor Android integrated through a managed service.

The desktop prototype's GroupManager is currently Qt/C++, not a shared Rust
group engine. Extract its tested state machine and wire contract into a
platform-neutral library before claiming Android/desktop interoperability.
Do not ship a separate Android implementation with subtly different group rules.

Tor Android reference: https://github.com/guardianproject/tor-android.
It provides TorService/libtor.so. The locally inspected tor-interface 0.6.7
already has a SystemTor configuration (SOCKS/control endpoints), but the current
tego-rs FFI launches bundled Tor. Add an authenticated, loopback-only attachment
adapter for the Android-managed Tor instance and verify onion hosting, not just
outgoing HTTP proxying. A WebView wrapper is not this port.

Long-lived connectivity must follow Android foreground-service and background
execution rules. A user-visible notification accompanies an active persistent
service where permitted; test battery, Doze, reconnect and process termination.
No assumption of immediate receipt while Android suspends the app. Do not add
an external push service without making its metadata trade-off explicit.
https://developer.android.com/develop/background-work/services/fgs

Local readiness observed: Android SDK at the usual user-local path, platform
android-36.1 installed, and Android Studio's Java runtime available. No Android
module, APK, JNI bridge or functioning mobile wallet has been implemented by
this ADR.

## Action items and acceptance

1. Done: internal credits selected; issuance, governance and administration added.
2. Specify ledger authority/issuer and account recovery, then wallet protocol.
3. Implement isolated ledger tests: concurrent spends, replay, rounding, insufficient
   balance, crash recovery and duplicate submission after an ambiguous timeout.
4. Extract group core and add desktop/Android shared wire compatibility tests.
5. Build Android screens and JNI/Tor service integration; produce an APK only
   after the first actual cross-platform chat test succeeds.
6. Exercise requests with a non-redeemable test currency and simulated settlement;
   add real-value rails only after a separate implementation/review decision.
7. Verify Android background lifecycle, private storage/key protection and network
   egress. Review privacy and licensing before publishing the expanded fork.
