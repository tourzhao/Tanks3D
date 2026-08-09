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
local paths, and all integrated self-tests after extraction. Eleven isolated
negative fixtures cover checksum-name/content mismatches, a checksum symlink,
input and resolved newline-bearing paths, lexical and symlink-parent path
escapes, an archive symlink, an extra top-level payload, an archived symbolic
link, and a checksum-updated mutation of a signature-sealed runtime resource.
The last case proves accidental-tamper detection by the ad-hoc resource seal; it
does not authenticate the publisher or replace Developer ID signing.

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
UTC test time, typed signature, and coverage references. Live gameplay also
requires candidate-bound `tanks3d-interactive-session-v1` and
`tanks3d-gameplay-event-log-v2` JSON artifacts with per-category results and
supporting hashes; a screenshot or free-form report is insufficient. Clean-Mac
and Gatekeeper commands use `tanks3d-command-log-v1`. Under v2, acquire the
exact HTTPS asset with Safari so the download receives genuine quarantine
metadata; never synthesize that attribute. A
`tanks3d-browser-acquisition-v1` record binds its client, URL, filename, ZIP
quarantine agent, tester, machine, typed signature, and interval to the
candidate. Its URL uses a public multi-label ASCII DNS host and standard HTTPS
port; reserved/test domains, IP/numeric forms, and malformed host labels are
rejected. The command log then records checksum, ZIP quarantine, app
quarantine, `codesign`, and `spctl` as five canonical, non-overlapping command
intervals. The containing interactive session begins before acquisition, which
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
each one. The live-gameplay compiler is stricter for supporting captures: its
five category groups must use distinct repository paths, even when the files
were exported from one longer recording.

### Live-gameplay observation compiler

After initializing the candidate-specific status, create the observation plan:

```sh
make init-alpha-v2-interactive-plan DIST_CHANNEL=alpha.N
```

The no-overwrite plan contains all 67 gameplay, national-base, pickup, and
settlement observations as `NOT_RUN`. Fill the candidate SHA, tester/machine,
session interval, signatures, reviewer, and review fields once at manifest
level. For every observation row, enter only its explicit result, observation
time, exact `checks_confirmed`, and notes; do not add keys. Add one or more real
PNG/recording paths to each of the five category-level
`supporting_artifacts` groups. There is no bulk-PASS option. Any missing,
reordered, or non-PASS row makes compilation fail. PNG files must decode as
valid PNGs; each recording must contain at least 64 KiB. Placeholder identities,
future timestamps, and notes that say a PASS was blocked, skipped, pending, or
not exercised are rejected during compilation.

Place final supporting captures in a persistent candidate evidence directory
before compiling, then choose a new output directory there. For example:

```sh
mkdir -p docs/assets/releases/v0.1.0-alpha.N/evidence
make compile-alpha-v2-interactive-evidence DIST_CHANNEL=alpha.N \
  ALPHA_INTERACTIVE_QA_OUTPUT_DIR="$PWD/docs/assets/releases/v0.1.0-alpha.N/evidence/interactive"
```

The compiler creates one canonical manifest, five v2 event logs, five session
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

This compiler covers the five live-play categories only. Main-menu/published-
control evidence and the source-free Safari acquisition/five-command records
still require their own accountable collection workflow; keep those gates
`NOT_RUN`/`BLOCKED` rather than copying synthetic fixtures or hand-authoring a
false PASS.

Treat source-free launch QA and performance QA as separate gates. The Clean-Mac
machine downloads only the staged candidate ZIP in Safari and must have no
source checkout or Homebrew raylib. Before the quarantined app's first Finder
launch, retain and inspect the downloaded ZIP, then expand it with Finder/Archive
Utility rather than Terminal tools so app-quarantine propagation is part of the
test. If Safari expands it automatically, record that path. If macOS offers it
after a blocked launch, test **System Settings > Privacy & Security > Open
Anyway**, authenticate, and confirm **Open** without disabling Gatekeeper or
deleting extended attributes. The named
performance-QA Mac may have a clean tagged checkout and Python, but the runner
executes only a private, hash-checked snapshot of the static candidate ZIP.
These may be different Macs. If one physical Mac is used,
finish and sign the Clean-Mac/Gatekeeper gate before installing or cloning the
performance tooling; do not claim `source_checkout_absent` for the later
performance phase.

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
  fullscreen, resize, sound, and the final report.
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
