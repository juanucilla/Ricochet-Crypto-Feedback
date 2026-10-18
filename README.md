# group-ricochet

An experimental community messenger combining private desktop group chat with
an internal DAO-credit wallet preview for Windows and Android, without blockchain.

Built on [Ricochet Refresh](https://github.com/blueprint-freespeech/ricochet-refresh)
by **Blueprint for Free Speech**, continuing the original Ricochet work by John
Brooks. Wallet and administration workflows are inspired by **Dyne.org**'s
[Social Wallet](https://github.com/dyne/social-wallet),
[Social Wallet API](https://github.com/dyne/social-wallet-api) and
[admin console](https://github.com/dyne/social-wallet-admin-console).
The current ledger is a separate implementation, not an integrated copy of Dyne's
code. Liquid governance is planned with reference to
[LiquidFeedback](https://liquidfeedback.com/en/open-source.html), published by
Public Software Group; it is **not implemented yet**. Thanks also to **g0v** for
the public [LiquidFeedback Core GitHub mirror](https://github.com/g0v/liquid_feedback_core)
used as a source reference. The mirror is not presented as the official upstream.

Independent fork: no affiliation with or endorsement by these projects or the Tor Project.
Existing upstream copyright and licence notices are preserved in [LICENSE](LICENSE).

## What works today

| Area | Preview status |
| --- | --- |
| Windows group chat | Fixed groups of 2–12 accepted contacts, consent, quoted replies, search, drafts, unread/mute, persistent outbox and per-recipient ACKs |
| Internal credits | Local authoritative ledger, multiple assets, capped issuance, transfers, own-credit burning, personal history and replay protection |
| Administration | Configured founding issuer permissions; explicit confirmation before issuance and asset creation |
| Android | Native wallet/admin preview, debug APK build, connection to the local test service via USB/emulator forwarding |
| Liquid governance | Planned; no active ballots, delegation, preferential results or automated treasury decisions |

**Not production-ready or security-audited.** Android does not yet support Tor chat.
The wallet is loopback-only and does not inherit chat anonymity. Real multi-peer Tor
group operation has not yet been validated. No real money, redemption, blockchain,
or legal token rights are provided. Group membership exposes the roster to members;
upstream clients cannot interpret the experimental group protocol.

## Build, test and contribute

- [Desktop group features, limitations and Windows build](GROUP_RICOCHET.md)
- [Desktop/Android DAO setup and security boundaries](doc/DAO-PREVIEW.md)
- [Measured verification results](doc/VALIDATION-group-ricochet.md)
- [DAO/governance architecture and roadmap](doc/ADR-003-dao-credits-and-liquid-governance.md)
- [LiquidFeedback source provenance and availability checks](doc/LIQUIDFEEDBACK-SOURCES.md)

The repository publishes source and build scripts, not a certified binary release.
Windows requires Qt 5.15, MSVC and Rust; the local DAO test service uses Python 3.10+.
Android uses the Android SDK/JDK without a Gradle dependency download.

Contributions should include tests, preserve upstream licences and document any
change to membership disclosure, credit authority or delivery guarantees. Never
commit profiles, credentials, balances or signing keys.

## Original upstream documentation (Ricochet Refresh, not fork guarantees)

The upstream description below is retained for context. Its platform availability,
release links and privacy claims concern Ricochet Refresh, not a security guarantee
or release announcement for this experimental fork or its wallet.

## Ricochet Refresh Alpha

## ⚠️ Warning ⚠️
This is the development branch of Ricochet-Refresh. Things here may be unstable, in-flight, and untested. For the current release version, see the [maint-3.0](https://github.com/blueprint-freespeech/ricochet-refresh/tree/maint-3.0) branch.

## R<sup>2</sup>: Ricochet Refresh
Ricochet Refresh is the new updated version of Ricochet, supported by Blueprint for Free Speech. We are a non-government, not-for-profit organisation working to safeguard the freedom of expression for whistleblowers, activists, and everybody else, worldwide. To find out more, check out our profile, or head to [blueprintforfreespeech.net](https://blueprintforfreespeech.net). Blueprint was the original sponsor of Ricochet, written by developer J. Brooks.

Ricochet Refresh is currently available for Windows, macOS (10.14 or later), and Linux. Visit the [releases page](https://github.com/blueprint-freespeech/ricochet-refresh/releases) for the latest version and changelog.

### What is Ricochet Refresh?
Ricochet Refresh is an instant messenger where no one knows your identity, who you're talking to, or what you're talking about.
  * You can talk to whoever you want, without them knowing your identity (or IP address)
  * No one can know who you're talking to, or when you're talking to them<sup>*</sup> (no metadata)
  * You talk *directly* to your contact - there's no middleman server that could be compromised, putting you at risk.
  * It's cross-platform, easy to install, and easy to use!
  * It's open source, and open to contribution!

[//]: # (we should update this screenshot)
![Screenshot](ricochetscreen.png) 

### How to use
Please consult our usage guide:
- [Usage guide](https://github.com/blueprint-freespeech/ricochet-refresh/blob/main/doc/usage.md)

### How it works
Ricochet Refresh uses the Tor network to establish a peer-to-peer connection between you and your contact. Ricochet Refresh creates a service on the Tor network which contacts can connect to. Tor's rendezvous system makes it extremely difficult for anyone to learn the identity of a Tor user, including you.

Ricochet Refresh is not affiliated with, or endorsed by, The Tor Project.

For more information about Tor, you can:
- [Learn about about Tor](https://support.torproject.org/about-tor/)

For more information about Ricochet Refresh, you can:
- [Learn about its design](https://github.com/blueprint-freespeech/ricochet-refresh/blob/main/doc/design.md)
- [Learn about the protocol](https://github.com/blueprint-freespeech/ricochet-refresh/blob/main/doc/protocol-v3.md)

### Disclaimer
Security and anonymity are difficult, complicated, and deep topics. We strongly encourage you to evaluate your risks, threats, and exposure with any software. *Do not rely on Ricochet Refresh for your safety*. We believe Ricochet Refresh does a really good job at protecting your privacy and anonymity, however, we can not guarantee this protection. 

### Building from source
See [BUILDING](https://github.com/blueprint-freespeech/ricochet-refresh/blob/master/BUILDING.md) for Linux, OS X, and Windows build instructions.

### Other
Bugs can be reported on the [issue tracker](https://github.com/blueprint-freespeech/ricochet-refresh/issues).

You should support [The Tor Project](https://www.torproject.org/donate/donate.html.en), [EFF](https://www.eff.org/), and [run a Tor relay](https://www.torproject.org/docs/tor-relay-debian.html.en).
