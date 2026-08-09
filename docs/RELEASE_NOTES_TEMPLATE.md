# Tanks 3D Alpha Release Notes

> **DRAFT — DO NOT PUBLISH.** Copy this template for a specific release. Blank
> fields, `TBD`, placeholder links, missing attachments, or unverified claims do
> not count as complete and block publication.

The final versioned page must use the exact marker
`**Release status: APPROVED.**`; retain `BLOCKED` while any status gate remains.

> **Release status: BLOCKED.**

## Release Identity and Downloads

- Version/channel: NOT RECORDED — BLOCKED
- Release date: NOT RECORDED — BLOCKED
- Source commit (full hash): NOT RECORDED — BLOCKED
- Release tag: NOT RECORDED — BLOCKED
- QA report: NOT LINKED — BLOCKED

If the commit or tag does not exist, do not publish the release.

| Download | Exact filename | SHA-256 or link | Status |
| --- | --- | --- | --- |
| Apple Silicon app ZIP | NOT RECORDED | NOT RECORDED | BLOCKED |
| SHA-256 file | NOT RECORDED | NOT RECORDED | BLOCKED |
| Candidate attestation | `attestation.txt` | NOT LINKED | BLOCKED |
| Automated gate log | `alpha-candidate-gates.log` | NOT RECORDED | BLOCKED |
| Build configuration | `build-config.txt` | NOT RECORDED | BLOCKED |
| Source at release tag | NOT RECORDED | NOT RECORDED | BLOCKED |

## Compatibility and Signing Disclosure

- Architecture: NOT RECORDED — BLOCKED
- Minimum macOS version from the artifact filename: NOT RECORDED — BLOCKED
- Clean-Mac version actually tested: NOT RECORDED — BLOCKED
- Signature/notarization status: NOT RECORDED — BLOCKED

State plainly that the Alpha is ad-hoc signed and not Apple-notarized unless the
exact published artifact has subsequently passed a documented Developer ID and
notarization process. Copy the **observed** Gatekeeper behavior and tested launch
steps from the QA report; do not guess or silently recommend disabling system
security.

### Tested launch steps

1. NOT RECORDED — BLOCKED
2. NOT RECORDED — BLOCKED

Observed Gatekeeper message/result: NOT RECORDED — BLOCKED

Gatekeeper conclusion: **BLOCKED**

## Highlights

Replace these prompts with concise, verified player-facing changes:

- Core gameplay: NOT RECORDED — BLOCKED
- One-player and local two-player support: NOT RECORDED — BLOCKED
- Tanks, national progression, and advanced settings: NOT RECORDED — BLOCKED
- Bases, environment, pickups, effects, audio, and settlement: NOT RECORDED — BLOCKED

## Controls

Confirm every line against the published build before release:

- Menus: NOT VERIFIED — BLOCKED
- Player 1 movement/fire: NOT VERIFIED — BLOCKED
- Player 2 movement/fire: NOT VERIFIED — BLOCKED
- Pause, setup/menu return, restart, stage navigation, and fullscreen: NOT VERIFIED — BLOCKED

## Release gate summary

| Gate | Status |
| --- | --- |
| Gameplay matrix | BLOCKED |
| National bases | BLOCKED |
| Pickups | BLOCKED |
| Settlement | BLOCKED |
| Published controls | BLOCKED |
| Clean Mac | BLOCKED |
| Gatekeeper | BLOCKED |
| Extended session | BLOCKED |
| Evidence manifest | BLOCKED |
| Known issues | BLOCKED |
| Audio decision | BLOCKED |
| QA approval | BLOCKED |
| Release-owner approval | BLOCKED |

For an approved page, every row must be `PASS` and must agree with the linked,
hashed QA report. Any non-PASS row blocks publication.

## Known Issues and Limitations

Include Gatekeeper friction, OS/architecture limits, gameplay defects, rendering
or audio problems, and workarounds. “None known” is allowed only when it is the
signed conclusion of the linked QA report.

Known-issues review: **NONE — BLOCKED**

| ID | Severity | Description | Reproduction / workaround | Resolution plan |
| --- | --- | --- | --- | --- |
| NOT RECORDED | | | | |

## Screenshots

Use images captured from the exact release artifact. Add useful alt text and do
not substitute concept art or an older build.

| View | Image/link | Caption | Verified artifact |
| --- | --- | --- | --- |
| One-player gameplay | NOT ATTACHED | NOT RECORDED | NO |
| Two-player gameplay | NOT ATTACHED | NOT RECORDED | NO |
| National base/environment | NOT ATTACHED | NOT RECORDED | NO |
| Pickup/effect or end-stage report | NOT ATTACHED | NOT RECORDED | NO |

## Licensing and Audio Decision

This is a non-commercial, source-available fan project; third-party materials
retain their own terms. Link the notices shipped inside the exact artifact.

- Project license and required notice: NOT VERIFIED — BLOCKED
- Third-party notices and asset provenance: NOT VERIFIED — BLOCKED
- Inherited 22-sound decision: ACCEPT / CONFIRM / REPLACE — NONE SELECTED
- Signed decision owner/date: NOT RECORDED — BLOCKED
- Decision evidence: NOT LINKED — BLOCKED

The audio decision must exactly match the signed section of the QA report.
Under the current v2 profile, the final page must state `Audio decision: **ACCEPT**`
exactly. `CONFIRM` is unavailable until a new profile verifies an externally
trusted cryptographic rights-holder signature; `REPLACE` requires a newly
attested candidate and replacement-manifest validation.

## Release Approval

- [ ] Every required field above contains verified release-specific information.
- [ ] Filename, SHA-256, commit, tag, QA report, and download all refer to one
      artifact.
- [ ] The published attestation, gate log, and build configuration are the files
      from the verified candidate directory.
- [ ] Known issues and Gatekeeper behavior are stated accurately.
- [ ] Screenshot assets and links resolve.
- [ ] Audio provenance decision is explicit, evidenced, and signed.

- Release owner: NOT RECORDED — BLOCKED
- Typed signature: NOT SIGNED — BLOCKED
- Approval date/time: NOT RECORDED — BLOCKED
