# LiquidFeedback provenance and availability

Checked 2026-10-18. The official project explicitly describes LiquidFeedback as
freely available under MIT/X11 at https://liquidfeedback.com/en/open-source.html.
That page links Public Software Group's original Core and Frontend repositories.

Public GitHub mirror: https://github.com/g0v/liquid_feedback_core

- Branch: `upstream`
- Observed revision: `39ee1b21318871132f401da5444a11ebe9e6932c`
- This is a third-party mirror, not evidence of current official-release parity.
- No LiquidFeedback code is currently vendored, executed or integrated here.
- Retain the reference while the public mirror exists. Never silently delete
  previously preserved source or migrate to private/leaked material if it vanishes.

Run `python script/check-liquidfeedback.py` for an anonymous public-access check.
It checks repository visibility, pinned commit, the pinned licence file and current
branch revision. Exit 0: available; 1: reference unavailable/changed licence status;
2: inconclusive (network, API limits, service errors). A 404 means not publicly
accessible through that endpoint, not proof the repository was deleted.

A daily Codex check reports meaningful changes only. New mirror commits are
informational and are never automatically incorporated. If availability changes,
retain this record and any separately preserved lawful copies; review official
sources and licence provenance before choosing a replacement. This check does not
create a backup and this repository currently contains no source archive of the mirror.
