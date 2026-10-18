# Ricochet Crypto Feedback (experimental preview)

This fork adds small private groups to Ricochet Refresh's Qt frontend. It has
no clock or energy calculator. Upstream: https://github.com/blueprint-freespeech/ricochet-refresh.

## Implemented preview

An additional **local-only DAO credit preview** now exists on desktop and Android:
see [DAO-PREVIEW.md](doc/DAO-PREVIEW.md) for setup, implemented features and security
boundaries. Android does not yet implement Tor chat; LiquidFeedback is not yet
integrated on either platform. The following group-chat features are desktop-only.

- Fixed rosters of 2–12 members, including the creator; explicit invitations.
- Group activation only after every participant accepts roster disclosure.
- Direct per-recipient delivery through existing authenticated Tor contact
  connections, with application-level ACK **after atomic local persistence**.
- Persistent outbox, retry every 30 seconds for seven days, duplicate suppression
  and separate per-recipient acknowledgement counts.
- Group list, accepted-contact picker, visible authors, quoted replies, plain
  selectable text, local search, drafts, unread counts and mute control.
- New messages follow the bottom only while already reading the latest;
  a jump-to-latest control appears when scrolled away.
- Local profile directory uses `group-ricochet`, without automatic migration
  of Ricochet profiles. History is stored in `groups.json` alongside that profile.
- Invalid/unreadable storage is preserved; failed writes never produce a
  delivery ACK. Local storage is not independently encrypted by this feature.

## Explicit limits

This is a compatibility prototype of the mesh design in ADR-001, not the full
proposed channel/epoch protocol. It intentionally requires all members to be
mutual accepted contacts using this fork. No contact is silently added.

The application protocol uses compact JSON text frames beginning
`group-ricochet/v1 ` over existing Ricochet chat channels. Upstream clients
cannot interpret these frames and may display raw JSON. Do not invite upstream
clients; native group capability negotiation remains future work. Ordinary
direct messages retain their existing handling except the reserved prefix.

The creator must be online to finish invitations. Any member leaving or
declining closes the **entire** fixed-roster group after the owner receives the
request; there are no partial removals, roster changes or late additions.
Disconnected members learn closure on reconnection, so there is no claim of
instantaneous revocation. An expired invitation cannot be accepted locally.

The preview allows 32 groups per profile, 250 stored messages per group,
500 UTF-16 code units per text, and 2,000 per serialized frame (upstream limit).
Unusually escape-heavy texts may require shortening. At the history cap new
messages are rejected rather than silently deleting conversation history.
There is no archive/reset action yet. Closed groups remain in local storage.

No remote read receipts, typing broadcast, server mailboxes, bots, public links,
files, topics, reactions, shared pins or disappearing messages are implemented.
No guarantee of delivery if the sender never reconnects. Times are local arrival
times; ordering is not globally agreed. Members know the onion roster and can
retain, copy or disclose received content. This preview is not security-audited.

## UX references and adaptation

- Signal group chats and explicit membership controls:
  https://support.signal.org/hc/en-us/articles/360007319331-Group-chats
- Telegram's conversation/topic organisation as a future direction:
  https://telegram.org/blog/topics-in-groups-collectible-usernames
- WhatsApp's group administration and approval patterns:
  https://faq.whatsapp.com/526742385997912/

These references inform the design, not claims that all their features exist
in this preview. Consent, understandable author identity, quoted replies,
quiet notifications, recoverable drafts and truthful delivery states take
priority over adding controls that the protocol cannot fulfil.

## Build and tests

Qt 5.15 is needed (the upstream QML uses QtQuick Controls 1). MSVC and Rust
are also required on Windows. Start with upstream BUILDING.md; a Tor executable
is required at runtime. `tests/groups` builds the group core and QML smoke
test separately from Rust/Tor:

```powershell
cmake -S tests/groups -B build-group-tests -DCMAKE_PREFIX_PATH=<Qt-5.15-prefix>
cmake --build build-group-tests --config Release
# Put the Qt bin directory on PATH before running tests.
ctest --test-dir build-group-tests -C Release --output-on-failure
```

For a local Windows build, test and portable Qt deployment:

```powershell
./script/build-group-windows.ps1 -QtRoot <Qt-5.15-prefix> -TorBinary <path-to-tor.exe>
```

The script writes `dist/group-ricochet/group-ricochet.exe`. Supply a supported
Tor executable and any accompanying runtime dependencies and licence files.
Bundled Qt/Tor redistribution requires their applicable licence notices and
source obligations; the script itself is a local development helper.

Tests simulate three authenticated peers; they do not establish real Tor circuits.
Production readiness requires real multi-peer Tor testing, compatibility and
load testing, profile handling review, and independent security review.
