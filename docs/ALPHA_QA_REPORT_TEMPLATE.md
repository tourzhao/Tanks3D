# Alpha QA Report

> **Template state: BLOCKED.** Copy this file to a versioned report before use.
> Blank cells, deleted required rows, `TBD`, `NOT RUN`, missing evidence, or an
> unexplained `N/A` never mean PASS. Publish only after every required gate has
> an explicit result and the final approval is signed.

## Report and Artifact Identity

| Required field | Recorded value |
| --- | --- |
| Report owner | NOT RECORDED — BLOCKED |
| Test date and timezone | NOT RECORDED — BLOCKED |
| Planned release date (`YYYY-MM-DD`) | NOT RECORDED — BLOCKED |
| Candidate directory (`build/release/<tag>`) | NOT RECORDED — BLOCKED |
| Artifact filename | NOT RECORDED — BLOCKED |
| Published `.sha256` filename | NOT RECORDED — BLOCKED |
| Independently recomputed SHA-256 | NOT RECORDED — BLOCKED |
| Candidate `attestation.txt` | NOT ATTACHED — BLOCKED |
| Attested gate-log filename and SHA-256 | NOT RECORDED — BLOCKED |
| Source commit (full hash) | NOT RECORDED — BLOCKED |
| Release tag | NOT RECORDED — BLOCKED |
| Source tree clean at build time | NOT RECORDED — BLOCKED |
| Automated gate log or CI run | NOT RECORDED — BLOCKED |

The commit and tag must both exist and identify the exact source used for the
artifact. If either is unavailable, stop: the Alpha is **BLOCKED**. Verify the
downloaded files, not a similarly named local build:

```sh
shasum -a 256 -c <artifact>.zip.sha256
make verify-tagged-alpha-candidate DIST_CHANNEL=<alpha.N>
make check-alpha-release-evidence DIST_CHANNEL=<alpha.N>
```

The evidence command permits honest blockers and is not approval. After this
report, its release page, evidence, known-issue review, audio decision, and both
approvals are final and committed, the no-exception release-ready target with
`DIST_CHANNEL=<alpha.N>` must pass from a clean worktree.

## Clean-Mac Environment

The test Mac must be Apple Silicon, have no prior Tanks 3D approval or install,
and must not depend on the source checkout or Homebrew raylib.

| Required field | Recorded value |
| --- | --- |
| Tester | NOT RECORDED — BLOCKED |
| Mac model | NOT RECORDED — BLOCKED |
| Chip and `uname -m` output | NOT RECORDED — BLOCKED |
| RAM | NOT RECORDED — BLOCKED |
| macOS version and build number | NOT RECORDED — BLOCKED |
| Fresh account or clean-machine method | NOT RECORDED — BLOCKED |
| Download URL and browser/client | NOT RECORDED — BLOCKED |
| Downloaded filename and independently recomputed SHA-256 | NOT RECORDED — BLOCKED |
| Exact checksum command and exit code | NOT RECORDED — BLOCKED |
| Prior app/approval absent | NOT VERIFIED — BLOCKED |
| Meets filename minimum macOS version | NOT VERIFIED — BLOCKED |
| Source checkout absent / Homebrew raylib unused | NOT VERIFIED — BLOCKED |

Attach a hashed `tanks3d-command-log-v1` JSON record for the checksum command.
Its candidate SHA, machine, exact argument vector, timestamps, stdout/stderr,
and exit code must describe the downloaded archive named above. Reserved or
placeholder download hosts are invalid. Use the reproducible download command
`curl --fail --location --output <artifact>.zip <https-url>` and record `curl`
as the download client.

## Quarantine and Gatekeeper

Keep the download's quarantine metadata for this test. Record commands, exit
codes, and exact dialogs; do not report an ad-hoc signature as Apple signing or
notarization.

| Check | Required evidence | Result |
| --- | --- | --- |
| ZIP quarantine | `xattr -p com.apple.quarantine <artifact>.zip` output | NOT RUN |
| Extracted app quarantine | `xattr -p com.apple.quarantine Tanks3D.app` output | NOT RUN |
| Signature integrity | `codesign --verify --deep --strict --verbose=4 Tanks3D.app` output and exit code | NOT RUN |
| Gatekeeper assessment | `spctl --assess --type execute --verbose=4 Tanks3D.app` output and exit code | NOT RUN |
| First Finder launch | Screenshot and verbatim dialog text | NOT RUN |
| Documented launch path | Exact player steps tested from a fresh account | NOT RUN |
| Successful launch | App reaches the main menu without removing its signature | NOT RUN |

Record the four quarantine/signature commands in the candidate-bound
`tanks3d-command-log-v1` JSON artifact; prose copied into this table is not
command evidence. Download, checksum, ZIP quarantine, app quarantine,
`codesign`, and `spctl` entries must have non-overlapping timestamps in that
order and remain inside the same Gatekeeper session.

For this ad-hoc-signed Alpha, a Gatekeeper rejection is an observed limitation,
not something to hide. The gate may pass only when the actual behavior and a
working, narrowly scoped launch path are documented consistently in the release
notes. Record the conclusion and evidence:

- Gatekeeper conclusion: NOT RECORDED — BLOCKED
- Release-note wording verified against observation: NOT VERIFIED — BLOCKED
- Evidence paths/URLs: NOT RECORDED — BLOCKED

## One-Player and Two-Player Gameplay Matrix

Use `PASS`, `FAIL`, or `BLOCKED`. Complete a full stage in each mode; a brief
startup smoke test is insufficient.

| Scenario | One player | Two players | Evidence / notes |
| --- | --- | --- | --- |
| Start configured stage and control every active tank | NOT RUN | NOT RUN | |
| Four-direction movement and collision | NOT RUN | NOT RUN | |
| Fire, hit enemies, and opposing shells cancel | NOT RUN | NOT RUN | |
| Pause and resume with Enter | NOT RUN | NOT RUN | |
| Esc returns to setup without closing the app | NOT RUN | NOT RUN | |
| Player loses HP, dies, and respawns | NOT RUN | NOT RUN | |
| Streak increments and resets on player destruction | NOT RUN | NOT RUN | |
| Base wall is breached and core loss ends the stage | NOT RUN | NOT RUN | |
| Classified end-stage K.O. rows and totals are correct | NOT RUN | NOT RUN | |
| Grenade kills stay out of classified K.O. rows | NOT RUN | NOT RUN | |
| Final report and next-stage/menu transition | NOT RUN | NOT RUN | |
| F11 fullscreen toggle | NOT RUN | NOT RUN | |
| Window resize, HUD, camera, and minimap | NOT RUN | NOT RUN | |
| Music/audio cues and volume behavior | NOT RUN | NOT RUN | |
| Complete stage without crash, hang, or soft lock | NOT RUN | NOT RUN | |

## Published Controls

Exercise every binding printed in the release page: menu arrows/`WASD`,
`Enter`/`Space`, both players' movement and fire keys, pause, `Esc`, `R`, `F8`,
`F11`, `N`/`B`, and setup-screen `Q`/`Esc` exit.

| Tester / UTC time | Candidate-bound recording or signed report | Result |
| --- | --- | --- |
| NOT RECORDED | NOT ATTACHED | NOT RUN |

## National Base Matrix

Test each nation in live play, including wall damage, Shovel steel, core loss,
visibility, collision, and the central emblem/statue orientation.

| Nation and base | One player | Two players | Evidence / notes |
| --- | --- | --- | --- |
| United States — Pentagon and eagle | NOT RUN | NOT RUN | |
| Soviet Union — ring castle and Stalin statue | NOT RUN | NOT RUN | |
| Germany — Reichstag and Hitler statue | NOT RUN | NOT RUN | |

## Pickup Matrix

Force or observe every pickup in both modes. Verify the 3D pickup, blinking
classic icon beneath it, minimap star, collection cue, score, and gameplay
effect. `Bandage` must also be checked for its spawn restrictions.

| Pickup and required effect | One player | Two players | Evidence / notes |
| --- | --- | --- | --- |
| Grenade — destroys eligible enemies and credits bonus-only tally | NOT RUN | NOT RUN | |
| Helmet — temporary shield | NOT RUN | NOT RUN | |
| Clock — freezes live enemies | NOT RUN | NOT RUN | |
| Shovel — temporarily applies steel base material/protection | NOT RUN | NOT RUN | |
| Tank — grants one life | NOT RUN | NOT RUN | |
| Star — upgrades exactly one level with matching attributes | NOT RUN | NOT RUN | |
| Gun — upgrades to maximum level | NOT RUN | NOT RUN | |
| Boat — grants the water traversal/absorption behavior | NOT RUN | NOT RUN | |
| Bandage — heals one HP only when useful | NOT RUN | NOT RUN | |
| Bandage absent at full HP | NOT RUN | NOT RUN | |
| Bandage disabled when maximum HP is 1 | NOT RUN | NOT RUN | |
| Pickup lifetime is 12.5 seconds | NOT RUN | NOT RUN | |

## Extended-Session Performance

Run continuously for at least 30 minutes on the named performance-QA Mac. This
may differ from the source-free Clean-Mac/Gatekeeper machine. The fixed Alpha
thresholds are average FPS >= 50, 1% low FPS >= 30, continuous real one-second
windows (each 0.75–1.25 seconds), physical-footprint growth <= 256 MiB, active
gameplay >= 80%, and focused-window time >= 95%. Run
`make run-alpha-performance-qa DIST_CHANNEL=alpha.N`, play normally, and
complete at least one stage. Attach the generated v2 telemetry, QA receipt,
stdout, and stderr files. The verifier derives FPS, memory, gameplay/focus
coverage, player mode, and cleared-stage count from raw integer counters and
candidate game events; hand-written summaries or legacy v1 samples do not
pass.

| Required field | Recorded value |
| --- | --- |
| Duration, stages, and player mode mix | NOT RECORDED — BLOCKED |
| Measurement tools and sampling interval | NOT RECORDED — BLOCKED |
| Fixed average / 1% low criteria | 50 / 30 FPS |
| Average / minimum / 1% low FPS | NOT MEASURED — BLOCKED |
| Thermal state, throttling, and fan observation | NOT MEASURED — BLOCKED |
| Maximum allowed / observed memory growth | 256 MiB / NOT MEASURED — BLOCKED |
| Rendering artifacts or camera/HUD failures | NOT RECORDED — BLOCKED |
| Audio dropouts, distortion, overlap, or missing cues | NOT RECORDED — BLOCKED |
| Crash / hang / soft-lock counts (each must be zero) | NOT RECORDED — BLOCKED |
| Logs and capture locations | NOT RECORDED — BLOCKED |
| Extended-session result | NOT RUN — BLOCKED |

## Known Issues and Evidence

Every observed problem must be recorded, including release rationale and player
workaround. Writing “none” requires the tester's signature below.

| ID | Severity | Reproduction | Impact / workaround | Release decision | Owner |
| --- | --- | --- | --- | --- | --- |
| NOT RECORDED | | | | | |

| Review field | Recorded value |
| --- | --- |
| Conclusion (`NONE_KNOWN` or `RECORDED`) | NONE — BLOCKED |
| Reviewer and UTC time | NOT RECORDED — BLOCKED |
| Typed signature | NOT SIGNED — BLOCKED |

Required screenshots or recordings:

| Evidence | File or URL | Reviewed |
| --- | --- | --- |
| Main menu and advanced settings | NOT RECORDED | NO |
| One-player gameplay with HUD/minimap | NOT RECORDED | NO |
| Two-player gameplay | NOT RECORDED | NO |
| All three national bases | NOT RECORDED | NO |
| Representative 3D pickup and minimap marker | NOT RECORDED | NO |
| Classified end-stage report | NOT RECORDED | NO |
| Actual Gatekeeper dialog/launch path | NOT RECORDED | NO |
| Extended-session metrics | NOT RECORDED | NO |

Interactive PASS evidence must include hashed
`tanks3d-interactive-session-v1` and `tanks3d-gameplay-event-log-v1` JSON
artifacts. They bind the candidate SHA, tester, machine, test interval, each
coverage token, result, review time, and supporting artifact hashes. A static
screenshot, arbitrary video, or free-form one-line report cannot substitute for
the signed event record. The tester and reviewer must be different people.

## Inherited 22-Sound Decision

Review `ASSET_LICENSES.md`, `THIRD_PARTY_NOTICES.md`, the archive's `licenses/`
directory, and all 22 inherited OGG files. Non-commercial intent does not replace
this decision. Select **exactly one** option; no selection, multiple selections,
missing authority, or missing evidence blocks release.

- [ ] **ACCEPT** — The release owner acknowledges the documented chain-of-title
  limitation and explicitly accepts it for this Alpha.
- [ ] **CONFIRM** — Permission/provenance has been confirmed with the relevant
  rights holder. The v1 gate deliberately cannot trust a self-authored record;
  add an externally trusted cryptographic signer in a new requirements profile
  before using this option.
- [ ] **REPLACE** — The inherited set has been replaced; attach sources, licenses,
  hashes, and proof that the archive manifest contains only the approved set.

The v1 status gate approves only an explicit owner `ACCEPT` decision. It cannot
approve `CONFIRM` without a trusted signature profile or `REPLACE` against an
already attested candidate. Add the relevant verification and attest a new
candidate before selecting either option.

| Required decision field | Recorded value |
| --- | --- |
| Selected option | NONE — BLOCKED |
| Rationale | NOT RECORDED — BLOCKED |
| Evidence URL/path | NOT RECORDED — BLOCKED |
| Five base review checks plus decision-specific check | NOT VERIFIED — BLOCKED |
| Decision owner and authority | NOT RECORDED — BLOCKED |
| Typed signature | NOT SIGNED — BLOCKED |
| Date and timezone | NOT RECORDED — BLOCKED |

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

## Final Approval

- [ ] Artifact filename, SHA-256, commit, and tag are mutually traceable.
- [ ] Candidate attestation, build configuration, and gate log verify unchanged.
- [ ] Automated gates passed for this exact commit and artifact.
- [ ] Clean-Mac, Gatekeeper, gameplay, base, pickup, and extended-session gates
      have explicit passing conclusions.
- [ ] All failures are fixed or disclosed as approved known issues.
- [ ] Screenshots and final release notes reference this exact artifact.
- [ ] The inherited-sound decision is singular, evidenced, and signed.

| Approval | Name | Signature | Date/time | Decision |
| --- | --- | --- | --- | --- |
| QA lead | NOT RECORDED | NOT SIGNED | NOT RECORDED | BLOCKED |
| Release owner | NOT RECORDED | NOT SIGNED | NOT RECORDED | BLOCKED |

The QA lead and release owner must be different people. These signatures attest
that the named humans performed/reviewed the work; the consistency gate does not
cryptographically prove a physical test occurred.

**Overall Alpha gate:** BLOCKED
