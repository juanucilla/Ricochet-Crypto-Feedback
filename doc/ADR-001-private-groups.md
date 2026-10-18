# ADR-001: group-ricochet — Small private groups over Tor

Status: Proposed, not implemented or security-reviewed

Implementation note (2026-10-18): an experimental fixed-roster application-layer
prototype is now implemented. See `../GROUP_RICOCHET.md` for actual behaviour
and limits. Native channel negotiation,
dynamic membership epochs and Rust integration below remain proposals.

Date: 2026-10-18

Decision owner: Giovanni Natella; security review required before release

## Context

The checked-out upstream main branch implements peer connections and chat
channels, but no group conversation abstraction was found. `doc/protocol-v3.md`
defines independently negotiated channels over authenticated Tor connections.
The repository is an alpha branch; the README points stable users to maint-3.0.
The new feature must preserve direct Tor transport and existing one-to-one chats.

## Decision

Start with invite-only groups of at most 12 members, including the owner.
Use a direct full mesh between consenting members. Send each message separately
to each authorised recipient over authenticated Tor connections. Reuse the
existing connection where possible. Do not add a central service or public rooms.
The cap is a provisional engineering budget, to be validated under Tor load.

All members see the membership roster and each other's onion identities. State
this explicitly before accepting an invitation. IP addresses remain protected
by Tor, but group membership and message timing are visible to fellow members.
This design does not provide sender anonymity within a group, protect against
malicious participants, or prevent copying messages already received.

## Alternatives

| Option | Benefit | Cost / limitation |
| --- | --- | --- |
| Direct mesh (proposed) | No permanent host; reuse peer transport | Up to 66 pair connections for 12 members; roster visible to members |
| Owner relays every message | Fewer connections per member | Owner availability required; sees all traffic and can omit deliveries |
| Public or private mailbox service | Easier asynchronous delivery | Adds service trust, abuse handling, metadata and operational obligations |
| Dedicated group key protocol | Potentially better bandwidth and group key lifecycle | Requires a reviewed protocol/library integration and larger security work |

An owner relay is not a transparent substitute for end-to-end authenticated
group delivery. Avoid inventing a group cryptosystem merely to simplify the UI.

## Membership and authorisation

One owner serialises changes for the first version. No moderators, ownership
transfer or voting in the MVP. The owner is not needed for ordinary messages
between members who already share a membership version.

The owner publishes an authenticated membership snapshot containing a random
256-bit group ID, protocol version, monotonically increasing epoch, predecessor
snapshot digest, member identities, group title and explicit policy. Signatures
and identity binding need a separate security specification using reviewed
primitives; the existing Tor identity must not be exported or repurposed without
review of the current authentication implementation.

An invitation expires and can be accepted once by its designated identity.
The recipient must explicitly accept roster disclosure. Adding an onion identity
to a group must not silently add it to the ordinary contact list. Group transport
authorisation is scoped to this group and must never grant access to one-to-one
chat or other channels. Non-contact member authentication requires an explicit
implementation and abuse tests; the current contact policy cannot be bypassed.

Only the owner can add/remove members or rename the group. Members can leave;
the local client immediately stops group traffic and requests an updated roster.
Removal applies after each sender receives the authenticated membership change.
There is no claim of instantaneous revocation for disconnected peers. Clients
with an old epoch must resynchronise before sending queued messages. Conflicting
snapshots of the same epoch pause the group and display a consistency error.
There is no distributed consensus or protection against a deliberately
equivocating owner in this initial design.

New members receive no old messages. Removed members retain material already
received. Pending messages must be rechecked against the current membership
before transmission; do not deliver old queued group content to new members.

## Messages and delivery

Proposed vendor channel: `org.group-ricochet.group.v1`; final identifier must
be checked for collisions before implementation. Negotiate it separately from
the existing chat channel. Old clients reject it gracefully and retain ordinary
one-to-one compatibility. Unsupported invitees get an upgrade indication.

Envelope: protocol version, group ID, membership epoch, random message ID,
author identity, per-author sequence, optional reply target, author timestamp
and UTF-8 body. Bind the envelope author to the authenticated transport sender.
Author time is display-only: it is not an authority for ordering or access.
Deduplicate by group/author/message ID and reject replays or inconsistent IDs.
Define deterministic display order, preserve the local arrival sequence and
show late arrivals rather than pretending a globally agreed order exists.

Initial limits: 8 KiB text body, 12 members, 1 MiB backlog per group, at most
7 days of retries, bounded incoming traffic per peer/group. Enforce limits in
the Rust layer, before UI allocation. Local outgoing queue survives restart.
The sender retains per-recipient delivery state and retries on reconnection.
There is no guaranteed delivery when the sender never reconnects, and no peer
store-and-forward in the MVP. Expired or evicted messages display failed status.
Acknowledgement follows a successful durable local write, not just packet receipt.

Per-recipient state: queued, sent, acknowledged, failed/expired. Display
`Delivered to 3/5 recipients`, with the recipient denominator captured for that
message, and provide details. Do not mark a whole group delivered after one ACK.
No read receipts, typing indicators, files, voice, reactions, edits, deletion or
automatic history sharing in the MVP.

## User experience

- Separate Groups section, create action and explicit invitation inbox.
- Invitation preview: title, inviter, roster, identity disclosure and compatibility.
- Conversation: visible authors, reply preview, unread separator, jump to latest,
  keyboard send/navigation, accessible labels and selectable plain text.
- Group details: member list and connection state, owner controls, leave action.
- Configurable mute and local unread counts; quiet joining/leaving notifications.
- Clear offline/partial delivery indicators and retry action.
- Treat group titles and messages as untrusted plain text; never fetch links,
  images or avatars automatically. No presence broadcast outside the group.
- Clock and energy calculator are outside the scope of group-ricochet and must
  not be included in its changes or releases.

## Code boundaries

These are proposed integration points, not existing group APIs:

| Area | Responsibility |
| --- | --- |
| `src/libtego_rs/crates/rico-protocol` | Capability negotiation, bounded group channel parsing and ACKs |
| `src/libtego_rs/crates/rico-profile` | Versioned schema migrations: groups, roster epochs, invitations, messages, recipient outbox |
| `src/libtego_rs/crates/tego-rs` | Membership state machine, lifecycle, scheduling, callbacks and FFI |
| `src/ricochet-refresh-qt/shims` | Group model and conversation adapters without cryptographic policy |
| `src/ricochet-refresh-qt/ui` | List, invitations, group timeline, details and notifications |

## Acceptance and release gates

1. Three peers exchange group messages; existing one-to-one tests still pass.
2. Unsupported peers reject group capabilities without breaking normal chats.
3. Explicit consent is required; non-contact group membership does not grant
   ordinary chat/file access. Outsiders, forged authors and expired invites fail.
4. Duplicate/replayed packets do not duplicate visible messages. Oversize payloads
   and flooding remain bounded, including persisted queues and UI rendering.
5. Restart, reconnect and partial delivery preserve recipient status. ACK only
   follows durable persistence; no false delivery indications on disk errors.
6. Membership changes invalidate obsolete authorisations; new members never
   receive queued history; removal/concurrent-change limitations are visible.
7. Twelve-peer Tor load test establishes acceptable connection, latency, memory
   and energy behaviour. Suspend and clock/timezone changes do not reorder policy.
8. Independent review covers identity binding, roster authenticity, transcript
   handling, local data protection, migrations and abuse boundaries.

## Delivery plan and fork recommendation

First record this ADR and a wire-format/security specification. Implement Rust
state machine tests before FFI/UI integration, then run the three-peer prototype.
Scope this fork exclusively to private group messaging. Existing local
clock/energy changes belong to the earlier task and must be excluded from fork
commits and releases. Qt availability and a runnable application still need
confirmation.

Recommend a GitHub fork of `blueprint-freespeech/ricochet-refresh`, named
`group-ricochet` as requested by Giovanni, with its relationship to upstream prominent.
Verify the intended owner/account, licence and upstream contribution rules
before publication. Choose a stable release baseline after comparing maint-3.0
to main; the current local checkout is main. Use the branch
`codex/private-groups-design` for the group proposal. A public release must
use a distinct app identity/profile directory and must not auto-upgrade existing
Ricochet profiles. Existing local changes are preserved; no fork or push has
been performed as part of this proposal.
