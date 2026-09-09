# Alpha QA Report

> **Template state: BLOCKED.** For a v2 candidate, do not copy this file into
> `docs/releases/<tag>-qa.md`: `make init-alpha-v2-status` creates that path and
> refuses to overwrite it. Run the initializer first, then use this document as
> a completeness worksheet while editing the generated candidate-specific
> report. Blank cells, deleted required rows, `TBD`, `NOT RUN`, missing evidence,
> or an unexplained `N/A` never mean PASS. Publish only after every required gate
> has an explicit result and the final approval is signed.

> **Overall Alpha gate: BLOCKED.**

## Report and Artifact Identity

| Required field | Recorded value |
| --- | --- |
| Report owner | NOT RECORDED — BLOCKED |
| Test date and timezone | NOT RECORDED — BLOCKED |
| Planned release date (`YYYY-MM-DD`) | NOT RECORDED — BLOCKED |
| Candidate directory (`build/release/<tag>`) | NOT RECORDED — BLOCKED |
| Artifact filename | NOT RECORDED — BLOCKED |
| Draft/pre-release HTTPS staging URL | NOT RECORDED — BLOCKED |
| Staging access/publication state | NOT RECORDED — BLOCKED |
| Published `.sha256` filename | NOT RECORDED — BLOCKED |
| Independently recomputed SHA-256 | NOT RECORDED — BLOCKED |
| Candidate `attestation.txt` | NOT ATTACHED — BLOCKED |
| Attested gate-log filename and SHA-256 | NOT RECORDED — BLOCKED |
| Source commit (full hash) | NOT RECORDED — BLOCKED |
| Release tag | NOT RECORDED — BLOCKED |
| Source tree clean at build time | NOT RECORDED — BLOCKED |
| Automated gate log or CI run | NOT RECORDED — BLOCKED |

The commit and tag must both exist and identify the exact source used for the
artifact. If either is unavailable, stop: the Alpha is **BLOCKED**. The
following repository commands run only on the release workstation, never on
the source-free Clean-Mac tester. Verify the downloaded files, not a similarly
named local build:

```sh
shasum -a 256 <artifact>.zip
make verify-tagged-alpha-candidate DIST_CHANNEL=<alpha.N>
make check-alpha-release-evidence DIST_CHANNEL=<alpha.N>
```

Compare the direct `shasum` output with the published sidecar. The source-free
Mac instead runs the standalone kit's `START_HERE.command`; it does not receive
this checkout or run `make`. The collector executes this exact three-argument
checksum form through `/usr/bin/shasum` as the first command in its Clean-Mac
command log; do not substitute `shasum -c` there.

The evidence command permits honest blockers and is not approval. After this
report, its release page, evidence, known-issue review, audio decision, and both
approvals are final and committed, the no-exception release-ready target with
`DIST_CHANNEL=<alpha.N>` must pass from a clean worktree.

Clean-Mac QA happens before final publication, so first upload the already
attested ZIP to a private draft release or access-controlled HTTPS staging
location that preserves its exact filename and bytes. This is a QA input, not
an approved release. Do not announce it, replace it after testing, or put
credentials/reusable secrets in the recorded URL. If no suitable staging URL
exists, leave the Clean-Mac and overall gates `BLOCKED`. Final publication must
reuse the tested ZIP; record and compare the final public download SHA-256
before announcement.

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
| QA-kit manifest and transfer method | NOT RECORDED — BLOCKED |
| Raw intake directory and `COMPLETE` marker | NOT RECORDED — BLOCKED |
| Continuous capture path and SHA-256 | NOT RECORDED — BLOCKED |
| Independent reviewer, signature, and UTC review time | NOT RECORDED — BLOCKED |
| Persistent compiled pack and `status.next.json` | NOT RECORDED — BLOCKED |

The release workstation creates the no-overwrite kit with
`make prepare-alpha-v2-clean-mac-qa-kit`, an exact
`CLEAN_MAC_DOWNLOAD_URL`, and the candidate's `DIST_CHANNEL`. The kit contains
only `START_HERE.command`, `clean-mac-plan.plist`, `README.txt`, and
`kit-manifest.json`—not the candidate or repository. On the fresh test Mac,
begin the capture and run:

```sh
/bin/zsh -f /path/to/clean-mac-kit/START_HERE.command \
  /path/to/new-clean-mac-intake
```

Use an absent destination beneath an existing real parent. The collector never
overwrites evidence and the final `COMPLETE` file records completion, not PASS.
This Mac must not clone the repository or install/run `make`, Python, Homebrew,
or raylib. Record the complete Safari-to-main-menu path with system recording
or an external camera; a static PNG is insufficient. Preserve a 64 KiB–95 MB
MOV, MP4, or M4V file with a structurally valid ISO-BMFF video container.

Download the exact staged candidate asset over HTTPS with Safari and record
`Safari` as the download client. This is intentional: command-line `curl` does
not create the `com.apple.quarantine` attribute needed for a genuine
downloaded-artifact test.
Do not add quarantine metadata manually. Attach a hashed
`tanks3d-browser-acquisition-v1` record with the candidate SHA, tester, machine,
exact client/URL/filename, interval, ZIP quarantine agent, and typed signature.
The acquisition interval must be inside the Gatekeeper session and finish
before checksum verification. Also attach a hashed `tanks3d-command-log-v1`
record beginning with the independent checksum command. Its candidate SHA,
machine, exact argument vectors, timestamps, stdout/stderr, and exit codes must
describe the Safari-downloaded archive named above. Reserved domains, their
subdomains, placeholder hosts, raw IP/numeric forms, single-label names, and
nonstandard HTTPS ports are invalid; use a normal public multi-label ASCII DNS
host on port 443.

## Quarantine and Gatekeeper

Keep the download's quarantine metadata for this test. Record commands, exit
codes, and exact dialogs; do not report an ad-hoc signature as Apple signing or
notarization.

| Check | Required evidence | Result |
| --- | --- | --- |
| ZIP quarantine | `/usr/bin/xattr -p com.apple.quarantine <artifact>.zip` output | NOT RUN |
| Extracted app quarantine | `/usr/bin/xattr -p com.apple.quarantine Tanks3D.app` output | NOT RUN |
| Signature integrity | `/usr/bin/codesign --verify --deep --strict --verbose=4 Tanks3D.app` output and exit code | NOT RUN |
| Gatekeeper assessment | `/usr/sbin/spctl --assess --type execute --verbose=4 Tanks3D.app` output and exit code | NOT RUN |
| First Finder launch | Continuous recording and verbatim dialog text | NOT RUN |
| Documented launch path | Exact player steps tested from a fresh account | NOT RUN |
| Successful launch | App reaches the main menu without removing its signature | NOT RUN |

Record checksum, ZIP quarantine, app quarantine, `codesign`, and `spctl` in the
candidate-bound `tanks3d-command-log-v1` JSON artifact; prose copied into this
table is not command evidence. These five entries must have non-overlapping
timestamps in that order and remain inside the same Gatekeeper session, which
must begin before the Safari download. The Safari action itself is documented
by the structured acquisition record and interactive capture, not fabricated
as a shell command. Keep the original ZIP, verify its checksum and quarantine,
then expand it with Finder/Archive Utility and verify the app's inherited
quarantine. If Safari expands it automatically, record that fact and still
retain and inspect the downloaded ZIP. Do not substitute Terminal extraction.
The collector preserves `clean-mac-plan.plist`, `clean-mac-intake.plist`,
`where-froms.hex`, the five exact pairs `checksum.{stdout,stderr}`,
`zip-quarantine.{stdout,stderr}`, `app-quarantine.{stdout,stderr}`,
`codesign.{stdout,stderr}`, and `spctl.{stdout,stderr}`, followed by the empty
`COMPLETE` marker. Do not edit, rename, or selectively copy those files before
review.

For this ad-hoc-signed Alpha, a Gatekeeper rejection is an observed limitation,
not something to hide. The gate may pass only when the actual behavior and a
working, narrowly scoped launch path are documented consistently in the release
notes. On current macOS, test the first Finder launch; if it is blocked and
**Open Anyway** is offered, open **System Settings > Privacy & Security**, use
that control, authenticate, and confirm **Open** in the follow-up dialog. Never
disable Gatekeeper or remove quarantine attributes. Record the actual
conclusion and evidence:

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
| F11 borderless toggle | NOT RUN | NOT RUN | |
| Window resize, HUD, camera, and minimap | NOT RUN | NOT RUN | |
| Music/audio cues and volume behavior | NOT RUN | NOT RUN | |
| Complete stage without crash, hang, or soft lock | NOT RUN | NOT RUN | |

## Advanced Settings

Exercise the actual menu and start gameplay with the selected values; a menu
screenshot alone is insufficient. Menu/range checks belong to the main-menu
context, while effects that can exist only in play must pass independently in
one- and two-player contexts.

| Check | Main menu | One player | Two players | Evidence / notes |
| --- | --- | --- | --- | --- |
| Default HP is 3 and `R` restores all defaults | NOT RUN | — | — | |
| Player HP covers 1 through 6 in steps of 1 | NOT RUN | — | — | |
| Enemy speed covers -30% through +30% in steps of 5% | NOT RUN | — | — | |
| Fire frequency covers -30% through +30% in steps of 5% | NOT RUN | — | — | |
| Spawn pace covers -30% through +30% in steps of 5% | NOT RUN | — | — | |
| Selected HP and enemy tuning take effect after start/restart | — | NOT RUN | NOT RUN | |
| HP 1 disables Bandage and normal HP restores eligibility | — | NOT RUN | NOT RUN | |
| Advanced-menu `Esc` preserves every selected value | NOT RUN | — | — | |
| Camera horizontal rotation covers -45° through +45° in 5° steps | NOT RUN | — | — | |
| Camera elevation covers 40° through 70° in 5° steps | NOT RUN | — | — | |
| Camera defaults are 0° horizontal / 50° elevation; reset restores both | NOT RUN | — | — | |
| Both camera angles survive Advanced-menu Esc, start, and restart | NOT RUN | NOT RUN | NOT RUN | |
| Camera framing across angle and window samples below | — | NOT RUN | NOT RUN | |
| Solo camera follows small movements continuously and recenters on reset | — | NOT RUN | — | |
| Co-op camera follows the midpoint, widens for separation, and handles respawn | — | — | NOT RUN | |

The current v2 profile (`interactive_controls_revision: camera-controller-v1`)
binds these fifteen checks through
`advanced_settings_checks`; a `published_controls` PASS must confirm them in
addition to every binding below. Do not treat
`main_menu_and_advanced_settings` as covered by a menu image. The v2 interactive
compiler creates this attestation together with the five live-play categories;
its main-menu, one-player, and two-player categories each require a distinct
recording of 64 KiB–95 MB.

## Published Controls

Exercise every binding printed in the release page: menu arrows/`WASD`,
`Enter`/`Space`, both players' movement and fire keys, pause, `Esc`, `R`, `F8`,
`F11`, `N`/`B`, and setup-screen `Q`/`Esc` exit.

The three contexts have fixed responsibilities:

- Main menu: both Arrow/WASD navigation families, both confirmation keys, both
  setup exit keys, every advanced range/default/reset check, and `Esc` value
  preservation.
- One player: every P1 movement/fire binding, all shared battle/display/stage
  hotkeys, runtime tuning after start/restart, and Bandage eligibility.
- Two players: repeat P1 and shared hotkeys, exercise every P2 movement/fire
  binding, and repeat runtime tuning and Bandage eligibility with both players.

The controller and camera checks below also belong to these contexts. An absent
controller leaves those checks `NOT RUN` or `BLOCKED`; keyboard-only play and
injected input tests do not satisfy the hardware observations.

| Check | Main menu | One player | Two players | Evidence / notes |
| --- | --- | --- | --- | --- |
| Controller navigation, both confirms, Back cancel, and top-face reset | NOT RUN | — | — | |
| Every controller fire binding, Start pause/resume, Back return, no face-button exit | — | NOT RUN | NOT RUN | |
| Controller assignment stays with each connected player | — | NOT RUN | NOT RUN | |
| Disconnect/reconnect clears held input and restores usable controls | NOT RUN | NOT RUN | NOT RUN | |
| Stick follows the view at every yaw; D-pad/keyboard retain world-cardinal lanes | — | NOT RUN | NOT RUN | |
| Returning to setup suppresses a held stick until release | — | NOT RUN | NOT RUN | |
| Stick release is recognized while D-pad is held; switching back works immediately | — | NOT RUN | NOT RUN | |

Every `or` names alternatives that are all publicly promised and therefore all
must be tested. `F11` means borderless, not exclusive fullscreen. Because `F8`
has no persistent HUD label, capture its visual change and the `high-quality`
2048x2048 / `balanced` 1024x1024 shadow-map line in the context recording, then
repeat the line in the observation notes; do not add an unsupported standalone
log artifact.

| Tester / UTC time | Candidate-bound M/1P/2P recordings and sessions | Result |
| --- | --- | --- |
| NOT RECORDED | NOT ATTACHED | NOT RUN |

### Camera and controller procedure

For `camera_framing_all_angles_and_window_sizes`, sweep all 19 horizontal
values at elevation 50°, then all seven elevations at horizontal 0°. Check the
four endpoint combinations (horizontal ±45° with elevation 40°/70°) as well.
Repeat the default and those four endpoint combinations at 960×540, 1280×720,
900×900, 800×900, and 1680×720 window sizes. These are sample sizes, not a
minimum-size or exhaustive display-compatibility claim. Confirm that active
tanks, HUD, and minimap remain visible; inspect forest transparency, terrain
edges, national bases, and shadows for clipping or ordering defects. Record the
window sizes and angle samples in the observation notes and context recording.

In solo play, drive toward all four map edges and make small movements to
check continuous follow, then restart and respawn. In co-op, separate the tanks
along both map diagonals, drive toward opposite edges, and let one player die
and respawn. Confirm midpoint tracking, both tanks remaining in view, and
smooth zoom changes. Keyboard movement must not rotate with the camera.

At every horizontal value, test each connected controller's four main stick
directions and turns between lanes. Confirm D-pad priority while the stick is
deflected; repeat keyboard and D-pad movement at the endpoint angles. Test all
four fire inputs separately: bottom face, left face, right shoulder, and right
trigger. Test bottom face and Start confirmation, Start pause/resume, Back
return/cancel, and top-face Advanced reset. On Nintendo layouts these are
`B`, `Y`, `R`, `ZR`, `+`, `-`, and `X`. No face button may exit the game. In
two-player mode verify the second controller controls P2 independently;
disconnect/reconnect each controller in turn while the other keeps playing.
Also disconnect with movement/fire held, checking for stuck input in both
gameplay and menus.

For both return-menu checks, keep the stick deflected while leaving battle;
the held stick must not cause setup navigation until it crosses its release
threshold; D-pad navigation remains available. First release and re-engage
normally. Then repeat while holding
the D-pad: center the stick, deflect it again, and release the D-pad. The new
stick direction must navigate immediately without a second recenter. A stick
that never centered must remain suppressed. Record both sequences separately
in each player-mode recording; a generic “controller works” note is inadequate.

## National Base Matrix

Test each nation in live play, including damage to the eight original Π wall
cells, Shovel steel, core loss, visibility, collision, and the command-core
emblem. Buildings must fit their original map cells without covering adjacent
terrain or leaving invisible wall collision after destruction.

| Nation and base | One player | Two players | Evidence / notes |
| --- | --- | --- | --- |
| United States — radio headquarters | NOT RUN | NOT RUN | |
| Soviet Union — watch-turret headquarters | NOT RUN | NOT RUN | |
| Germany — cupola headquarters | NOT RUN | NOT RUN | |

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

## Settlement Report

Complete and review a classified report produced by live play. The showcase is
useful for a publication image but does not replace the live settlement check.

| Required settlement result | Result | Evidence / notes |
| --- | --- | --- |
| Basic-tank K.O. row | NOT RUN | |
| Fast-tank K.O. row | NOT RUN | |
| Power-tank K.O. row | NOT RUN | |
| Armor-tank K.O. row | NOT RUN | |
| K.O. total equals the four classified rows | NOT RUN | |
| Score/points match the live tally | NOT RUN | |
| Grenade destructions remain excluded from classified K.O. rows | NOT RUN | |

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
pass. The receipt, telemetry, and stdout limits are respectively 64 KiB,
32 MiB, and 16 MiB. Successful stderr must be exactly empty. The runner aborts
on overflow or stderr output, and the verifier checks those constraints before
accepting hashes.

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

Every dynamic PASS category needs a hashed `tanks3d-interactive-session-v1`
artifact. Main-menu, one-player, two-player, national-base, pickup, and
settlement categories also need their exact
`tanks3d-gameplay-event-log-v2`; Gatekeeper needs the browser-acquisition and
five-command records; the extended session
needs its generated telemetry and receipt. These records bind the candidate
SHA, tester, machine, intervals, coverage tokens, result, review time, and
supporting artifact hashes. A static screenshot, arbitrary video, or free-form
one-line report cannot substitute for the required structured records. The
tester and reviewer must be different people.

For the six compiled interactive categories, run
`make init-alpha-v2-interactive-plan DIST_CHANNEL=alpha.N`, explicitly complete
all 70 observations, and then run
`make compile-alpha-v2-interactive-evidence DIST_CHANNEL=alpha.N` with a new
persistent `ALPHA_INTERACTIVE_QA_OUTPUT_DIR`. The compiler has no bulk-PASS
option, never changes the source status, and emits a reviewable
`status.next.json`, one manifest, and candidate-bound v2 event/session records.
The event logs identify `Tanks3D Alpha QA Evidence Compiler` as their producer
and bind the exact manifest hash. Safari acquisition and the five commands use
the separate source-free collector. Return its untouched raw intake and the
continuous capture to the release workstation; an independent reviewer then
runs `make compile-alpha-v2-clean-mac-evidence` with the intake, media, reviewer
identity/signature/time, review notes, and
`CLEAN_MAC_RELEASE_NOTE_WORDING_VERIFIED=yes` only after the observed launch
path matches the release note. The compiler rejects incomplete or
test-mode intake, refuses existing output, and writes canonical acquisition,
command-log, Gatekeeper-session and compiler-receipt records, preserves the ten
raw command streams, copies the media, and emits `status.next.json` under the
default persistent `docs/assets/releases/<tag>/evidence/clean-mac-compiled/`.
It never edits the canonical status. Commit and verify that pack from a clean
worktree before deliberate promotion; do not use fixtures or hand-change those
gates to PASS. Under v2, Clean-Mac status references only
`gatekeeper_launch`, not the controls session; `main_menu_reached` remains a
required Gatekeeper detail from the quarantined Finder-launch observation.

Treat `build/release-evidence/<tag>/` as a temporary, ignored intake directory.
Before marking evidence `PASS`, copy every referenced artifact into
`docs/assets/releases/<tag>/evidence/`, update its status path and SHA-256, and
commit it with the versioned report. No recording may exceed 95 MB. Do not
publish secrets; obtain consent for
tester/machine data. If privacy or size prevents committing an artifact, remain
`BLOCKED` until the verifier supports an immutable external evidence bundle.
The final published status must not point only to ignored local files.

## Inherited 22-Sound Decision

Review `ASSET_LICENSES.md`, `THIRD_PARTY_NOTICES.md`, the archive's `licenses/`
directory, and all 22 inherited OGG files. Non-commercial intent does not replace
this decision. Select **exactly one** option; no selection, multiple selections,
missing authority, or missing evidence blocks release.

- [ ] **ACCEPT** — The release owner acknowledges the documented chain-of-title
  limitation and explicitly accepts it for this Alpha.
- [ ] **CONFIRM** — Permission/provenance has been confirmed with the relevant
  rights holder. The current gate deliberately cannot trust a self-authored
  record; add an externally trusted cryptographic signer in a new requirements
  profile before using this option.
- [ ] **REPLACE** — The inherited set has been replaced; attach sources, licenses,
  hashes, and proof that the archive manifest contains only the approved set.

The v2 status gate approves only an explicit owner `ACCEPT` decision. It cannot
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
- [ ] The staged Clean-Mac ZIP and final public ZIP are byte-for-byte identical.
- [ ] Candidate attestation, build configuration, and gate log verify unchanged.
- [ ] Automated gates passed for this exact commit and artifact.
- [ ] Clean-Mac, Gatekeeper, gameplay, base, pickup, and extended-session gates
      have explicit passing conclusions.
- [ ] All failures are fixed or disclosed as approved known issues.
- [ ] Screenshots and final release notes reference this exact artifact.
- [ ] Every status-referenced evidence file is committed and publishable.
- [ ] The inherited-sound decision is singular, evidenced, and signed.

| Approval | Name | Signature | Date/time | Decision |
| --- | --- | --- | --- | --- |
| QA lead | NOT RECORDED | NOT SIGNED | NOT RECORDED | BLOCKED |
| Release owner | NOT RECORDED | NOT SIGNED | NOT RECORDED | BLOCKED |

The QA lead and release owner must be different people. These signatures attest
that the named humans performed/reviewed the work; the consistency gate does not
cryptographically prove a physical test occurred.

Final review remains **BLOCKED** until every required row and approval passes.
