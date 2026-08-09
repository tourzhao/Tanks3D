# macOS Alpha Release Checklist

The first public target is a non-commercial, Apple Silicon GitHub Alpha. A
source checkout may still use the Homebrew-linked development app; files shared
with players must come from a verified `make alpha-candidate` directory. A
`make dist` ZIP is useful during development but is not a publishable candidate.

## Automated package gate

Commit the intended source with a clean worktree and create the exact tag
`v<VERSION>-<CHANNEL>` (for example, `v0.1.0-alpha.1`) at `HEAD`. Then run:

```sh
make test-alpha-candidate
make alpha-candidate DIST_CHANNEL=alpha.1
```

The candidate command starts from `make clean`, which preserves earlier
candidate directories and candidate-bound QA evidence while removing disposable
build outputs. It then runs `debug`, architecture,
the Alpha builder/verifier negative-contract fixtures, test, sanitizer,
coverage, and `test-dist` gates, and rechecks the clean worktree, `HEAD`, tag,
bundle version, and channel before publishing anything. The standalone
`test-alpha-candidate` command is a quick preflight; the candidate builder runs
and attests it again, so direct script invocation cannot bypass that gate.
It writes atomically to `build/release/<tag>/`; a failed run leaves no candidate
for that tag. Candidate construction holds one worktree-wide lock because every
channel shares `build/dist/` and the compiler object directories. Do not bypass
the lock or run another distribution build concurrently. The verifier
independently requires the captured build configuration's source commit and tag
to match the candidate attestation.

While `HEAD` still equals the attested tag, recheck an existing directory with:

```sh
make verify-alpha-candidate \
    ALPHA_CANDIDATE_DIR=build/release/v0.1.0-alpha.1
```

The strict target deliberately rejects later documentation commits. Once
`HEAD` has advanced, keep the tag immutable and verify from its source snapshot:

```sh
make verify-tagged-alpha-candidate DIST_CHANNEL=alpha.N
```

The tagged verifier requires a clean worktree, copies the exact five candidate
files into a private clone of the attested commit, runs that commit's strict
verifier, detects concurrent candidate/tag/`HEAD` changes, and always removes
the clone. Never move an attested tag to accommodate later documentation.

The directory contains the ZIP, `.sha256`, build configuration, gate log, and
`attestation.txt`. The v2 attestation binds all of them to one clean commit and
records all eight required gates as `PASS`. Verification also reruns the full
macOS distribution verifier against the candidate ZIP. Publish this exact file
set together.

`test-dist` statically links the installed raylib and rejects an architecture or
deployment-target mismatch. It verifies the ZIP checksum and structure,
exact resource and license manifest, bundle metadata, system-only dynamic
dependencies, ad-hoc signature integrity, absence of quarantine/DLP metadata or
local paths, and all integrated self-tests after extraction. Thirteen isolated
negative fixtures cover checksum-name/content mismatches, a checksum symlink,
input and resolved newline-bearing paths, lexical and symlink-parent path
escapes, an archive symlink, an extra top-level payload, an archived symbolic
link, a checksum-updated mutation of a signature-sealed runtime resource, and
re-signed mutations of both raylib notice files. The runtime-resource case
proves accidental-tamper detection by the ad-hoc resource seal; the notice
cases prove the independent byte comparisons. Neither mechanism authenticates
the publisher or replaces Developer ID signing.

The release path accepts only raylib 6.0 and verifies that the repository's
raylib license is the exact official 6.0 text before compiling. It also ships
and byte-compares the version-locked embedded-dependency notices. A raylib
upgrade therefore requires an explicit notice audit and a newly attested
candidate; do not bypass the version check.

The release object cache carries a configuration fingerprint. Changing the
compiler identity, raylib prefix or archive contents, architecture, deployment
target, or distribution flags automatically invalidates cached objects. A clean
build is still required for the final candidate because it exercises the same
path used by CI from first principles.

The ZIP name is authoritative: `macos-arm64-macos26.0` means Apple Silicon and
macOS 26.0 or newer. Do not rename an artifact to claim broader compatibility.
To target an older release, first provide a raylib build compiled for that same
deployment target and rebuild through `RAYLIB_PREFIX`.

## Human release gate

The [Alpha QA report template](ALPHA_QA_REPORT_TEMPLATE.md) and
[release notes template](RELEASE_NOTES_TEMPLATE.md) are completeness
references for v2; do **not** copy either one into `docs/releases/<tag>*`
before initialization. The one-shot initializer below owns those versioned
paths and refuses every overwrite. Run it first, then edit the generated
candidate-specific QA report and release page using the templates as
worksheets. Blank or `NOT RUN` fields, missing evidence, and an absent source
commit or release tag are blockers, not passing results.

The frozen Alpha 3 status remains at
`docs/releases/v0.1.0-alpha.3-status.json` under the legacy v1 profile and may
be validated only as blocked work in progress. A new candidate must name
`docs/release-requirements/macos-alpha-v2.json`; v1 can no longer approve a
release. Python 3 standard-library tooling
enforces exact keys, candidate/document/evidence hashes, the full manual matrix,
published controls, candidate-bound interactive evidence, clean-Mac download
and quarantine facts, numeric session criteria, known issues, the audio
decision, chronology, and both approvals.

For a new v2 candidate, first place these seven fresh 1280x720 PNGs in
`build/release-evidence/<tag>/screenshots/`: `one-player.png`,
`two-player.png`, `base-usa.png`, `base-ussr.png`, `base-germany.png`,
`bonuses.png`, and `settlement.png`. Then create the versioned, honestly
blocked baseline once:

```sh
make init-alpha-v2-status DIST_CHANNEL=alpha.N
```

The helper re-verifies the tagged candidate and rejects symlinks, malformed
images, partial destinations, and every overwrite. It copies the images and
creates the release page, QA report, and v2 status, but records every human gate
as `NOT_RUN` or `BLOCKED`. Pixel or byte differences cannot prove when an image
was captured; exact-candidate provenance remains an accountable human review.
The helper's success is not evidence or approval. Ordinary failures receive
best-effort rollback. If the process is forcibly terminated, inspect the ten
versioned destinations and remove partial outputs manually; the helper will not
guess ownership or delete them automatically.
Commit and inspect all generated files before the clean-worktree evidence check;
the tagged verifier intentionally rejects a dirty tree.

### Pre-release HTTPS staging

Clean-Mac quarantine QA necessarily downloads the candidate before final
approval. Upload the **already attested, byte-for-byte candidate ZIP** to a
private draft release or another access-controlled HTTPS staging location that
preserves its exact filename. This upload is QA staging, not an approved public
release: do not announce it, mark the release approved, or replace the asset
after testing. The URL must be safe to record in evidence, without credentials
or reusable secrets embedded in it. If no such staging location is available,
the Clean-Mac gate remains `BLOCKED`.

Record the staging URL and independently recomputed SHA-256. Final publication
must reuse that exact ZIP. If a draft release is promoted, do not re-upload or
recompress the asset; if the final host differs, upload the same file and
recompute its public download SHA-256 before announcing the release.

Run the contract and evidence checks with:

```sh
make test-release-status
make check-alpha-release-evidence DIST_CHANNEL=alpha.N
```

The second command uses `--allow-blocked`: success means the incomplete record
is internally honest, not that publication is approved. It still fails on a
bad candidate, stale hashes, wrong evidence classes, or invalid claims. Each
interactive evidence block must identify the candidate SHA-256, tester, Mac,
UTC test time, typed signature, and coverage references. The six compiled
interactive categories require candidate-bound
`tanks3d-interactive-session-v1` and `tanks3d-gameplay-event-log-v2` JSON
artifacts with per-category results and supporting hashes; a screenshot or
free-form report is insufficient. Clean-Mac
and Gatekeeper commands use `tanks3d-command-log-v1`. Under v2, acquire the
exact HTTPS asset with Safari so the download receives genuine quarantine
metadata; never synthesize that attribute. A
`tanks3d-browser-acquisition-v1` record binds its client, URL, filename, ZIP
quarantine agent, tester, machine, typed signature, and interval to the
candidate. Its URL uses a public multi-label ASCII DNS host and standard HTTPS
port; reserved/test domains, IP/numeric forms, and malformed host labels are
rejected. The command log then records checksum, ZIP quarantine, app
quarantine, `codesign`, and `spctl` through their fixed macOS absolute paths as
five canonical, non-overlapping command intervals. The containing interactive
session begins before acquisition, which
must complete before checksum. Performance uses
candidate-generated `tanks3d-performance-log-v2` raw integer windows plus a
`tanks3d-performance-qa-receipt-v1`. The receipt binds the tagged ZIP, embedded
source identity, extracted executable hash, random nonce, exact argv,
START/COMPLETE markers, exit status, and telemetry/stdout/stderr hashes. The
verifier recomputes weighted average and 1% low FPS from continuous 0.75–1.25
second windows and checks >=50 average FPS, >=30 1% low FPS, <=256 MiB
physical-footprint growth, >=80% active-gameplay time, >=95% focused-window
time, and at least one candidate-reported cleared stage. It also cross-checks
the recorded stage count and player-mode summary. A consistently hashed
artifact may support multiple categories only when its structured schema names
each one. The interactive compiler is stricter for supporting captures: its six
category groups must use distinct repository paths, even when files were
exported from one longer source recording. Main-menu, one-player, and two-player
evidence must each attach its own recording of at least 64 KiB; none may reuse a
path or substitute a still image for that recording. Every recording must be no
larger than 95 MB so the required evidence commit remains below ordinary
GitHub Git's 100 MiB single-file rejection threshold.

### Interactive observation compiler

After initializing the candidate-specific status, create the observation plan:

```sh
make init-alpha-v2-interactive-plan DIST_CHANNEL=alpha.N
```

The no-overwrite plan contains all 70 main-menu/control, gameplay,
national-base, pickup, and settlement observations as `NOT_RUN`. Fill the
candidate SHA, tester/machine,
session interval, signatures, reviewer, and review fields once at manifest
level. For every observation row, enter only its explicit result, observation
time, exact `checks_confirmed`, and notes; do not add keys. Add one or more real
PNG/recording paths to each of the six category-level
`supporting_artifacts` groups. There is no bulk-PASS option. Any missing,
reordered, or non-PASS row makes compilation fail. PNG files must decode as
valid PNGs; each recording must contain 64 KiB–95 MB. Placeholder identities,
future timestamps, and notes that say a PASS was blocked, skipped, pending, or
not exercised are rejected during compilation.

The three control contexts are exact, not interchangeable:

- `main_menu_and_advanced_settings` covers both menu-navigation families, both
  confirmation keys, both setup exit keys, defaults/reset, every HP/rate range
  step, and Advanced-menu `Esc` value preservation.
- `one_player_gameplay` covers all P1 movement/fire alternatives, the shared
  battle/display/stage hotkeys, live tuning after start/restart, and both sides
  of the Bandage eligibility rule.
- `two_player_gameplay` repeats P1 and shared-hotkey coverage, adds every P2
  movement/fire alternative, and repeats live tuning and Bandage coverage with
  both players active.

An `or` in a check ID groups publicly supported alternatives; it never permits
testing only one. Record both Arrow/WASD menu families, `Enter` and `Space`, all
three fire keys for each player, `N` and `B`, and both `Q` and `Esc` setup exits.
For `F8`, capture the observed render change and the `high-quality` 2048x2048 or
`balanced` 1024x1024 shadow-map line in the context recording, then repeat that
line in the observation notes; the compiler does not accept a standalone log
artifact. `F11` is the borderless toggle.

Place final supporting captures in a persistent candidate evidence directory
before compiling, then choose a new output directory there. For example:

```sh
mkdir -p docs/assets/releases/v0.1.0-alpha.N/evidence
make compile-alpha-v2-interactive-evidence DIST_CHANNEL=alpha.N \
  ALPHA_INTERACTIVE_QA_OUTPUT_DIR="$PWD/docs/assets/releases/v0.1.0-alpha.N/evidence/interactive"
```

The compiler creates one canonical manifest, six v2 event logs, six session
reports, and `status.next.json` without changing the source status or replacing
any file. The event producer is honestly identified as the project QA compiler,
and every event log binds the canonical observation-manifest SHA-256.

The tagged verifier requires a clean worktree. Review the new pack first, then
commit the pack and every referenced supporting capture as a draft evidence
commit. From that clean commit, verify the generated status explicitly:

```sh
make check-alpha-release-evidence DIST_CHANNEL=alpha.N \
  RELEASE_STATUS_FILE="$PWD/docs/assets/releases/v0.1.0-alpha.N/evidence/interactive/status.next.json"
```

Only after this blocked-status check passes should you deliberately copy
`status.next.json` over `docs/releases/v0.1.0-alpha.N-status.json`, update the
candidate documents, and commit that promotion. Re-run the canonical evidence
check from a clean worktree. The compiler does not update release prose,
document hashes, known issues, audio, or approvals.

This compiler covers main-menu/published-control evidence and the five live-play
categories. Safari acquisition and Gatekeeper use a separate two-machine
collector/compiler workflow; never copy synthetic fixtures or hand-author a
PASS.

On the release workstation, prepare a new source-free kit from the already
attested candidate and its exact staging URL:

```sh
make prepare-alpha-v2-clean-mac-qa-kit DIST_CHANNEL=alpha.N \
  CLEAN_MAC_DOWNLOAD_URL='<recordable HTTPS URL ending in the candidate filename>'
```

The default output is
`build/release-evidence/<tag>/clean-mac-kit/`. It must not already exist. The
transfer kit contains only `START_HERE.command`, `clean-mac-plan.plist`,
`README.txt`, and `kit-manifest.json`; it contains no source checkout and no
candidate ZIP. Verify its manifest before transfer.
The preparer re-reads all four published files and verifies their expected
manifest bytes before it succeeds; transfer the four-file directory intact.
Give that kit to a fresh Apple Silicon Mac. Do not clone the repository or
install/run `make`, Python, Homebrew, or raylib there.

Before starting, arrange a continuous system recording or external-camera
recording that can show the Safari download, Finder extraction, first launch,
actual Gatekeeper dialogs and launch path, and the main menu. Then follow the
kit README and run the collector from Terminal with a brand-new destination:

```sh
/bin/zsh -f /path/to/clean-mac-kit/START_HERE.command \
  /path/to/new-clean-mac-intake
```

The destination's real parent must already exist, while the final directory
must not. The collector refuses replacement. A successful output contains
exactly `clean-mac-plan.plist`, `clean-mac-intake.plist`, `where-froms.hex`,
`checksum.stdout`, `checksum.stderr`, `zip-quarantine.stdout`,
`zip-quarantine.stderr`, `app-quarantine.stdout`, `app-quarantine.stderr`,
`codesign.stdout`, `codesign.stderr`, `spctl.stdout`, `spctl.stderr`, and the
final empty `COMPLETE` marker. A `COMPLETE` marker means collection finished,
not that the gate passed. The directory is mode `0700` and its files are mode
`0600`; preserve those files as one unit. Keep the downloaded ZIP. Use Safari
for the real HTTPS download and Finder/Archive Utility for extraction so
quarantine propagation is tested. If Safari expands the ZIP automatically,
record that path and still retain and inspect the ZIP. Never add, rewrite, or
delete quarantine attributes.

Launch the app from Finder. If macOS offers it after a blocked launch, test
**System Settings > Privacy & Security > Open Anyway**, authenticate, and
confirm **Open**. This must be a human action: do not automate Finder, System
Settings, authentication, or the dialog. A nonzero `spctl` result is allowed
for this ad-hoc-signed Alpha, but it must be recorded exactly and the documented
narrow launch path must genuinely reach the main menu without changing the app
signature or quarantine metadata.

Return the untouched raw intake directory and capture to the release
workstation. A reviewer other than the tester compiles a new persistent pack:

```sh
make compile-alpha-v2-clean-mac-evidence DIST_CHANNEL=alpha.N \
  CLEAN_MAC_INTAKE_DIR=/absolute/path/to/raw-intake \
  CLEAN_MAC_MEDIA=/absolute/path/to/gatekeeper-capture.mov \
  CLEAN_MAC_REVIEWER='Reviewer Name' \
  CLEAN_MAC_REVIEWER_SIGNATURE='Reviewer Name' \
  CLEAN_MAC_REVIEWED_AT_UTC="$(date -u '+%Y-%m-%dT%H:%M:%SZ')" \
  CLEAN_MAC_REVIEW_NOTES='Safari, Finder, dialog, and main menu reviewed.' \
  CLEAN_MAC_RELEASE_NOTE_WORDING_VERIFIED=yes
```

Set `CLEAN_MAC_RELEASE_NOTE_WORDING_VERIFIED=yes` only after comparing the
actual dialog and launch path with the candidate's release-note wording.
Run the compiler immediately after that review so the generated UTC value
follows the recorded session completion time.

By default this creates
`docs/assets/releases/<tag>/evidence/clean-mac-compiled/`. It refuses existing
output and rejects test-mode or incomplete intake. The pack contains
`clean-mac-plan.plist`, `clean-mac-intake.plist`, `where-froms.hex`,
the same ten command `.stdout`/`.stderr` transcripts,
`browser-acquisition.json`, `command-log.json`,
`clean-mac-compiler-receipt.json`, `gatekeeper-launch-session.json`, the copied
`gatekeeper-launch-recording.<ext>`, and `status.next.json`. A static PNG is not
valid launch-path media: use a 64 KiB–95 MB MOV, MP4, or M4V file with a
structurally valid ISO-BMFF container, positive duration, video track, and
sample table. These are exactly 19 output files;
the 18 files other than `status.next.json` are attached to
`gatekeeper_launch` evidence. It does not replace the canonical status. Review
the media and generated records, commit them, and from the resulting clean
worktree run:

```sh
make check-alpha-release-evidence DIST_CHANNEL=alpha.N \
  RELEASE_STATUS_FILE="$PWD/docs/assets/releases/<tag>/evidence/clean-mac-compiled/status.next.json"
```

This `--allow-blocked` path checks honest integration while unrelated gates may
remain blocked. Promote `status.next.json` deliberately only after review; do
not treat compilation or a successful allow-blocked check as release approval.

Treat source-free launch QA and performance QA as separate gates. The named
performance-QA Mac may have a clean tagged checkout and Python, but its runner
executes only a private, hash-checked snapshot of the static candidate ZIP.
These may be different Macs. If one physical Mac is used, finish and sign the
Clean-Mac/Gatekeeper gate before installing or cloning performance tooling; do
not claim `source_checkout_absent` for the later performance phase.

Under v2, Clean-Mac status binds only `gatekeeper_launch`; do not attach or reuse
the `main_menu_and_advanced_settings` control session for that gate. The
Gatekeeper details still require `main_menu_reached`, observed after the
quarantined Finder launch, as proof that the tested launch path reached the
application rather than merely clearing command-line checks.

After building the immutable candidate, create the performance evidence once:

```sh
make run-alpha-performance-qa DIST_CHANNEL=alpha.N
```

The default private output is
`build/release-evidence/<tag>/performance/`. It must be absent or empty. The
visible one-player session starts automatically; play normally, keep the game
focused, and complete at least one stage. The candidate exits after at least
1,801 seconds and a complete one-second sample window. `Esc` or closing the
window aborts the run without a valid receipt. The runner snapshots and rehashes
the candidate ZIP before extraction, so a concurrent replacement cannot alter
the executed bundle. Add all four generated files to the
`extended_session_metrics` evidence before final verification.

`build/release-evidence/<tag>/` is an ignored local intake directory, not a
persistent audit location. Before any evidence category becomes `PASS`, copy
every referenced JSON, log, report, recording, and performance file into
`docs/assets/releases/<tag>/evidence/` without overwriting an existing file.
No committed recording may exceed 95 MB.
Update the artifact paths and SHA-256 values in the status JSON, then commit
those files with the versioned report and page. The seven publication PNGs are
already copied into `docs/assets/releases/<tag>/` by the initializer.

All committed evidence must be safe for public distribution: exclude secrets
and obtain consent for recorded tester/machine information. If an artifact
cannot be committed because of privacy or size, the Alpha remains `BLOCKED`
until the release contract supports and verifies a separately published,
immutable evidence bundle. Do not leave a final status pointing only at ignored
local files. Publish the committed evidence alongside the final status and
requirements profile so the audit record remains resolvable after a fresh
clone.

This gate proves candidate identity, record integrity, chronology, and internal
consistency; it cannot cryptographically prove that a human performed a test.
The named tester and evidence reviewer must therefore be different people, as
must the QA lead and release owner. Their signatures are accountable human
attestations, not software-generated proof.

After completing QA, update the release page and QA report to their canonical
`APPROVED`/`PASS` markers, update their hashes in the status JSON, commit all
records, and leave the worktree clean. The QA report's canonical 13-row
`Release gate summary` must contain only `PASS`. Reviews must follow the actual
test times; known-issue review follows evidence review, then QA approval, then
release-owner approval. The pre-release staging upload above is the sole
exception; final publication, promotion, and announcement are allowed only
after:

```sh
make verify-alpha-release-ready DIST_CHANNEL=alpha.N
```

This no-exception target must report that every required gate passes.

Capture publication images from the executable inside the extracted candidate,
not from a development build. For example:

```sh
Tanks3D.app/Contents/MacOS/Tanks3D \
    --quick-start \
    --release-screenshot=/absolute/path/one-player.png \
    --release-screenshot-frame=240
```

Frame 240 is the default and normally clears the 3.2-second stage introduction
at 60 Hz. Capture mode uses a fixed gameplay seed, locks the logical canvas,
ignores the borderless toggle, normalizes Retina output to 1280x720, refuses to
replace an existing path even if it appears during capture, saves one PNG, and
exits. Use
`--quick-start-2p`, the national quick-start options, and the settlement
showcase to cover the required release views. Record the candidate ZIP hash,
exact command, Mac model, macOS version, capture time, and PNG hash in the QA
report. `make test-release-screenshot` is an explicit local GPU smoke test; it
opens a window and intentionally remains outside CI and candidate gates.

- Test the extracted ZIP on a clean Mac matching the declared minimum system.
- Complete one-player and two-player stages, including pause/Esc, every pickup,
  all three national bases, player death/respawn, base loss, stage settlement,
  borderless toggle, resize, sound, and the final report.
- Run a longer session and record frame-rate, thermal, rendering, and audio
  problems as known issues.
- Review the archive's `licenses/` directory and decide whether to accept,
  confirm, or replace the inherited sound set described in
  `ASSET_LICENSES.md`. The current v2 profile approves only explicit owner
  `ACCEPT`;
  `CONFIRM` requires a new profile with an externally trusted cryptographic
  rights-holder signature, and `REPLACE` requires a newly attested candidate
  plus replacement-manifest verification.
- Add release screenshots, concise notes, known limitations, and all five files
  from the verified candidate directory to the GitHub release. Also publish the
  final status JSON, its fixed requirements profile, and the persistent evidence
  files referenced by that status as audit records.
- Publish only an archive produced on `arm64`. The current CI job validates the
  native architecture of `macos-latest` but does not upload its generated ZIP;
  pin an Apple Silicon runner before turning CI output into release artifacts.

## Signing boundary

The current ad-hoc signature detects bundle modification and is checked by
`codesign --verify`. It does not provide a Team ID, trusted timestamp, Hardened
Runtime approval, notarization, or a warning-free Gatekeeper download. Do not
describe it as Apple-signed or notarized. Developer ID signing, notarization,
stapling, and a quarantined-download test remain required for a polished beta.
