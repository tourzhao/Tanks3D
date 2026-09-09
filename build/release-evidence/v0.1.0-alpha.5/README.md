# Alpha 5 candidate preparation

Candidate source: `cc5f2a7eec073accf37c2085c10bf155061acef2`, tagged
`v0.1.0-alpha.5`. The tag remains on this source after documentation commits.

Artifact: `Tanks3D-0.1.0-alpha.5-macos-arm64-macos26.0.zip`

SHA-256: `d82b1ca75e6b9503fdfbe84baf32f2f7a6e3ea7a042d8a7a438cb7be501e59d8`

The unchanged ZIP and its four companion records are under
`build/release/v0.1.0-alpha.5/`. This is an Apple Silicon, macOS 26.0+
candidate with an ad-hoc integrity signature. It has not been approved for
publication.

## Automated build verification

`make alpha-candidate DIST_CHANNEL=alpha.5` completed all eight gates:
clean, release-tool regressions, strict Debug compilation, architecture,
game tests, ASan/UBSan, coverage and distribution verification.

- 209 Python release-tool tests, plus shell candidate/clean-preservation checks.
- 247 game-test suites / 15,700 runtime checks.
- 26 ASan/UBSan suites / 14,166 runtime checks.
- Main and supplementary drawing coverage reports, with no mismatched data.
- Exact bundle resources, all distributed notices, static dependencies,
  signature integrity, ZIP/checksum and embedded candidate identity checked.

The candidate's `alpha-candidate-gates.log` and `attestation.txt` retain the
authoritative outputs. No human observation is inferred from these checks.

## Candidate preview and audio provenance

`capture-previews.py` exports seven 1280x720 screenshots from the unmodified
candidate after verifying its ZIP and embedded identity. The base, pickup and
settlement images use explicit built-in showcases; they do not prove those
gameplay operations. `screenshot-capture-record.json` records every command
and digest. Native process logs are kept alongside it.

All seven exports completed successfully and were viewed by the development
agent. The spawn views include substantial outside-map ground below the players;
formal framing review is pending. HUD text is legible. The settlement preview
is captured before its count-up animation completes.

`audio-review/` contains an unsigned inventory of the exact 22 candidate OGG
files and four canonical source notices, checked against their pinned Git
blobs and archive bytes. It describes repository provenance claims; the audio
owner's decision remains pending.

## Remaining actual acceptance

`interactive/observation-plan.json` begins with all 70 observations NOT_RUN.
The fixed v2 matrix predates Pixel Style and enemy-nation selection, so their
additional checks are retained in the
[Chinese operator checklist](../release-hardening-20260909/OPERATOR_CHECKLIST.zh-CN.md).
Real controllers, a quarantined download on a clean Mac, a focused 30-minute
gameplay session, audio-owner review and independent approvals are outstanding.
The [operation plan](../release-hardening-20260909/QA_PLAN.md) gives exact
commands. There is currently no staged HTTPS download for this candidate.

The versioned release page, QA report and status under `docs/releases/` retain
these blockers. A successful `--allow-blocked` check is not release approval.
