# ADR-003: Internal DAO credits, administration and liquid governance

Status: Proposed functional architecture; not implemented in the desktop preview

Date: 2026-10-18

Decision owner: Giovanni Natella

## Confirmed scope

group-ricochet will combine private group chat, internally created/administered
credits, liquid governance and administrative tools, with an Android client.
Giovanni describes the credits as tokenisation of a DAO. Blockchain connectors
and external crypto settlement remain excluded. Transferable credits can also
represent voting weight under an explicit DAO policy; no automatic assumption
of equity, euro redemption, or token-holder legal rights follows from this design.

The current Windows executable only implements the experimental chat preview.
This ADR does not claim a working wallet, governance engine, console or APK.

## Reference projects found

| Reference | Verified role | Proposed use |
| --- | --- | --- |
| https://github.com/dyne/social-wallet-api | Wallet REST service; database-only operation supported | Functional reference for balances, transfer metadata and ledger access |
| https://github.com/dyne/social-wallet | Configurable web wallet frontend | Reference for account and transfer workflows |
| https://github.com/dyne/social-wallet-admin-console | Interactive administrator REPL | Reference for operator operations; not an embedded general-purpose REPL for chat users |
| https://github.com/g0v/liquid_feedback_core | GitHub mirror of upstream Core | Inspect algorithms and fixtures; verify revision against official source |
| https://liquidfeedback.com/en/open-source.html | Official Core/Frontend source links and architecture | Authoritative provenance; Core uses PostgreSQL/PL/pgSQL; Frontend uses Lua/WebMCP |
| https://liquidfeedback.com/en/full-feature-list.html | Published governance capabilities | Feature parity reference, distinguishing optional features |

An old GitHub mirror is not evidence of current upstream parity. Select and pin
an actual official Core release before reuse and inspect its schema/API/tests.
Record per-component licences from selected files before incorporating code.
The proposed native UI is a functional adaptation, not a claim of affiliation.

## Decision

Keep four product areas in one client: Chat, Wallet, Governance, Administration.
The logical backend contains membership, ledger, governance, policy execution
and audit modules. They may share PostgreSQL while retaining separate permissions
and schemas. Use Tor onion access and an authenticated service API distinct from
the existing experimental group text frames. No blockchain process is required.

The ledger and governance backend are authoritative services. Operating them
over Tor does not make the organisation trustless or remove the operator's
ability to observe records. Peer-to-peer chat remains a different trust domain.
Replicas improve availability only if writes still have a defined authority.

Reuse of LiquidFeedback Core is preferred for its delegation, phase transitions
and preferential aggregation, rather than silently approximating it with a
simple chat poll. Determine a supported transaction/API integration after
reviewing the selected release. A custom engine is a separate option and must
pass reference-result comparisons before claiming matching voting semantics.

## Wallet and internal token administration

Each community defines assets with a stable asset ID, issuer/DAO, display name,
symbol, integer scale, transfer rules, issuance policy and optional supply cap.
Multiple assets are a new product requirement, not assumed functionality of the
referenced Social Wallet database backend.

Administrative operations to build/adapt:

- Create an asset and an issuance policy; initial bootstrap requires explicitly
  recorded founding authority until governance can approve subsequent changes.
- Mint credits under a policy, allocate to treasury/members and distribute
  contribution rewards with a decision/reference attached.
- Transfer, request and split internal payments; balances and audit history.
- Burn authorised credits; burning another user's funds cannot be a silent
  administrative convenience. Define holder approval or explicit enacted policy.
- View supply, allocations and treasury; role management, export and reconciliation.
- Record corrections through compensating entries; never rewrite history.

All amounts use checked integer arithmetic. Issuance/burn events explain every
supply change; ordinary transfers conserve supply and never silently overdraw.
Mint, burn, payment and policy commands carry unique idempotency IDs and a
verifiable authorisation chain. Replays cannot repeat an execution.

The UI admin area exposes typed, permission-checked operations. A web/mobile
screen must not receive unrestricted shell/SQL/Clojure-REPL execution just because
the original Dyne console uses a REPL. A restricted compatibility adapter for
the original console can be evaluated separately if direct reuse is required.

## Governance capabilities to reproduce/adapt

1. Organisational units and subject areas with explicit membership/accreditation.
2. Issues containing competing initiatives, rationale and versioned drafts.
3. Support, suggestions and feedback during deliberation, not just final ballots.
4. Configurable admission/quorum, discussion, verification/freeze and voting phases.
5. Delegation per organisation, subject area or issue, with specificity precedence.
6. Transitive delegations and direct-vote override, revocation and cycle handling
   using the selected Core's semantics rather than ad hoc recursive sums.
7. Preferential ballots over alternatives, including status quo, reproducible
   result calculations, participation/quorum reporting and explicit tie rules.
8. Binding/non-binding proposals with an audit trail and approved execution rules.

This is the target functional scope. Do not label an ordinary majority poll as
a complete copy of LiquidFeedback, or imply all optional upstream features are
included in the first implementation.

## Relationship between credits, membership and voting

Configure voting policy per DAO/area: one accredited member one vote, weight
from a selected credit asset, or a separately defined hybrid. Credits may be
both spendable units and the source of voting weight; the wallet balance and
the weight assigned to a particular issue are still different recorded values.
The user has not yet selected the default weighting rule.

For credit-weighted voting:

- Snapshot eligible members and balances at a defined, recorded ledger sequence
  at the issue's voting boundary. All alternatives in that issue use that snapshot.
- Minting, burning and transferring after the boundary do not change that ballot's
  weight. The same units cannot count again under a recipient in the same issue.
- A snapshot prevents that replay, but does not by itself prevent borrowed holdings
  before the snapshot or concentration of power. Any holding-period/anti-capture
  rule is explicit and approved, not implied by tokenisation.
- A delegation carries the delegant's snapshot weight once; it does not transfer
  wallet ownership or permit spending. Resolve delegations with exactly defined
  cutoff/direct-vote/cycle rules; do not count both direct and delegated weight.
- Repeated proposals cannot change their voting basis mid-process. Rule changes
  apply prospectively under their own approval policy.

Membership is a separate anti-Sybil boundary. Tor onion identities alone do not
establish one natural person per vote. Opening additional accounts must not
manufacture accredited memberships or issuer-granted credits automatically.

## Executing approved decisions

Example: a proposal authorises 1,000 credits for an approved contribution. The
governance engine finalises the result; an executor validates the immutable
decision, policy version, beneficiary, asset, amount and unique execution ID.
The ledger books it once and links a receipt back to the decision/chat card.
An approved spending proposal fails or remains awaiting funds if treasury is
insufficient; it must not silently mint extra credits.

Proposal approval can authorise an action when the enacted policy explicitly
permits automatic execution. Higher-risk treasury/issuance actions may require
multiple independent approvers. Distinguish this application-level approval
threshold from blockchain multisignature transactions.

Technical administrators operate backups, availability and migrations; they
must not quietly gain political power to mint, alter voting weights or change
results. Any emergency pause/recovery power is declared, scoped and audited.
An off-chain DAO here means community rules with verifiable automated execution,
not the absence of hosting/operator trust.

## UX and privacy

Chat presents concise proposal/payment cards linking to dedicated screens.
Voting shows own/delegated weight, delegate path and whether a direct vote
overrides delegation. Weight in this issue and current spendable balance are
labelled separately. The app explains status transitions and impending deadlines.
Administrative screens show policy, approvals needed and consequences before
submitting a command. No unauthorised mint/burn is triggered by a chat message.

Choose transparency deliberately: private messages and individual balances
remain private by default; decisions, treasury actions and governance audit
have a defined audience. LiquidFeedback's transparent process does not imply
secret ballots or coercion resistance. Do not promise simultaneous full public
audit, unlinkable identities and secret voting without a separate cryptographic
design. DAO membership/financial links require consent and a documented threat
model even when services are reachable only over Tor.

## Android and delivery sequence

The Android plan from ADR-002 remains active: native mobile UI, shared group
protocol/core, JNI bridge and managed Tor integration. Extend it with the same
wallet/governance service contracts; never implement independent balance or
vote-authority logic on the handset. Protect signing credentials and require
explicit user authorisation for spending/delegation/admin operations.

1. Pin and review reference sources; define governance charter and asset policies.
2. Build/test ledger and audited administrative commands with test-only assets.
3. Integrate the selected LiquidFeedback Core and test membership/delegation flows.
4. Add snapshot weights, outcome-to-ledger execution and replay protection.
5. Integrate client screens and Tor service access; then Android cross-client tests.
6. Perform security, recovery, concurrency and independent vote-result checks
   before enabling valuable/redeemable community credits.

Acceptance must cover concurrent spends, supply caps, overflow/rounding,
unauthorised mint, duplicate approved-action execution, delegations with cycles,
direct-vote override, snapshots during transfers, post-snapshot issuance,
membership revocation boundaries, deadlocks/retries and role separation.
