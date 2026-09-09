# Fresh macOS Alpha v2 QA plan

The Alpha 5 candidate, seven previews and initial BLOCKED status now exist.
Use those outputs directly. The construction and initialization steps below
document their workflow; do not rerun them over existing candidate artifacts.
See `OPERATOR_CHECKLIST.zh-CN.md` for the remaining actual acceptance work.

Prepared 2026-09-09 from the current repository tools. This is an operation
plan, not a completed human observation or an approval.

## Immediate prerequisites and parallel work

1. Channel `alpha.5` and tag `v0.1.0-alpha.5` are selected for the repaired
   source. Read the source commit and candidate SHA from
   `build/release/v0.1.0-alpha.5/attestation.txt`; do not reuse or move Alpha 4.
2. Finish source changes and the source commit, make the worktree clean, and
   create the exact new `v<VERSION>-<CHANNEL>` tag at that source commit.
   A normal
   `make dist` ZIP is insufficient: current v2 QA requires a v3 candidate
   attestation and the exact embedded performance capability contract.
3. Needed for external QA: two distinct actual tester/reviewer identities,
   physical controllers for interactive checks, a fresh Apple Silicon Mac or
   qualifying clean account, and an HTTPS staging URL for the exact candidate
   ZIP with no credentials/secrets in the recorded URL.
4. Once the candidate exists, screenshot export and all-NOT_RUN plan creation
   are automatable. Do not run screenshot capture concurrently with the
   performance session: focus and GPU contention affect its real result.
   Source-free Clean-Mac QA can run on a separate Mac while performance QA runs.

The formal v2 workflow requires tracked files under
`docs/assets/releases/<tag>/evidence/` for PASS evidence. `build/` is only local
intake. The initializer itself requires new `docs/releases/<tag>*` and
`docs/assets/releases/<tag>/` destinations; there is no output-directory flag
to relocate those outputs into `build/`. Any later release-workflow promotion
must respect that explicit tool contract and never overwrite historical
release records.

## Shared shell parameters

Run from the project root using the selected `alpha.5` channel.

```sh
set -e
QA_CHANNEL=alpha.5
QA_VERSION=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' macos/Info.plist)
QA_TAG="v${QA_VERSION}-${QA_CHANNEL}"
QA_CANDIDATE="$PWD/build/release/$QA_TAG"
QA_LOCAL="$PWD/build/release-evidence/$QA_TAG"
QA_SHOTS="$QA_LOCAL/screenshots"
QA_STATUS="$PWD/docs/releases/$QA_TAG-status.json"
```

Candidate preparation commands, after the clean source/tag prerequisite, are:

```sh
make alpha-candidate DIST_CHANNEL="$QA_CHANNEL"
make verify-tagged-alpha-candidate DIST_CHANNEL="$QA_CHANNEL" \
  ALPHA_CANDIDATE_DIR="$QA_CANDIDATE"
```

The builder starts with `make clean` and runs its full attested gates; do not
run competing distribution builds. It creates exactly five candidate files
(ZIP, ZIP checksum, build config, gate log, attestation). All later commands
must use that candidate. A later documentation commit is allowed; the original
tag remains unchanged. Tagged verification requires a clean current worktree.

## Seven screenshots directly from the candidate binary

These are publication previews, not proof of human gameplay coverage.
All export commands use production code already inside the candidate; no
patched harness, relink, asset substitution, or image composition is involved.
Native graphics access is required when executing them through Codex.

Prepare a new extraction directory after candidate verification:

```sh
QA_ZIP_NAME=$(sed -n 's/^artifact_filename=//p' "$QA_CANDIDATE/attestation.txt")
QA_SHA=$(sed -n 's/^artifact_sha256=//p' "$QA_CANDIDATE/attestation.txt")
test -n "$QA_ZIP_NAME"
test -n "$QA_SHA"
QA_CAPTURE="$QA_LOCAL/candidate-capture"
test ! -e "$QA_CAPTURE"
test ! -e "$QA_SHOTS"
install -d -m 700 "$QA_LOCAL" "$QA_CAPTURE" "$QA_SHOTS"
(cd "$QA_CANDIDATE" && shasum -a 256 -c "$QA_ZIP_NAME.sha256")
/usr/bin/ditto -x -k "$QA_CANDIDATE/$QA_ZIP_NAME" "$QA_CAPTURE"
QA_BINARY="$QA_CAPTURE/Tanks3D.app/Contents/MacOS/Tanks3D"
/usr/bin/codesign --verify --deep --strict "$QA_CAPTURE/Tanks3D.app"
"$QA_BINARY" --self-test=release-performance-capabilities \
  > "$QA_LOCAL/screenshot-candidate-capabilities.json"
```

Check the probe's source commit/tag against the attestation. Keep logs outside
`screenshots/`: the initializer accepts exactly these seven filenames and no
other directory entries. Run the following sequentially and stop on any
nonzero capture exit:

```sh
capture_candidate_preview()
{
    local qa_name="$1"
    local qa_frame="$2"
    shift 2
    "$QA_BINARY" "$@" \
      "--release-screenshot=$QA_SHOTS/$qa_name.png" \
      "--release-screenshot-frame=$qa_frame" \
      > "$QA_LOCAL/capture-$qa_name.stdout" \
      2> "$QA_LOCAL/capture-$qa_name.stderr"
}

capture_candidate_preview one-player 600 --stage=1 --quick-start
capture_candidate_preview two-player 600 --stage=1 --quick-start-2p
capture_candidate_preview base-usa 2 --stage=1 --camera-elevation=60 \
  --quick-start --base-damage-showcase
capture_candidate_preview base-ussr 2 --stage=1 --camera-elevation=60 \
  --quick-start-ussr --base-damage-showcase
capture_candidate_preview base-germany 2 --stage=1 --camera-elevation=60 \
  --quick-start-germany --base-damage-showcase
capture_candidate_preview bonuses 2 --stage=1 --quick-start --bonus-showcase
capture_candidate_preview settlement 2 --stage=1 --quick-start-2p --settlement-showcase
shasum -a 256 "$QA_BINARY" "$QA_SHOTS"/*.png > "$QA_LOCAL/screenshot-sha256.txt"
```

CLI ordering matters: `--stage` and camera configuration precede quick-start.
Screenshot mode fixes a 1280x720 export, enables `FLAG_WINDOW_ALWAYS_RUN`, and
refuses to replace output files. Frame is a rendered-frame count, not seconds;
its allowed range is 1–3600. The first two previews show normal play after
intro. The three base images deliberately use the built-in damaged-wall
showcase; bonuses and settlement use their built-in showcases. Describe those
staged states accurately in captions. They cannot stand in for observed damage,
pickup, stage-completion, or two-player control tests.

Inspect all seven images for correct content, readable HUD, and clipping.
The initializer checks real PNG decoding, exact 1280x720 size, seven distinct
decoded pixel images, canonical paths, and absence of symlinks. It cannot prove
capture provenance: preserve the candidate identity, commands, logs and hashes.
Recapture failed images only into a new intake after reviewing the failed
attempt; do not silently overwrite evidence.

## Initialize fresh status and the interactive plan

With a clean worktree and the seven candidate screenshots:

```sh
make init-alpha-v2-status DIST_CHANNEL="$QA_CHANNEL" \
  ALPHA_CANDIDATE_DIR="$QA_CANDIDATE" \
  ALPHA_RELEASE_SCREENSHOT_INPUT_DIR="$QA_SHOTS"
make init-alpha-v2-interactive-plan DIST_CHANNEL="$QA_CHANNEL"
```

The screenshot argument is mandatory at script level and must resolve to the
canonical `build/release-evidence/<tag>/screenshots/`; the Make target supplies
that default. Status initialization creates seven publication PNGs plus the
new release page, QA report and status. Every human gate remains NOT_RUN or
BLOCKED. It refuses any pre-existing destination, including partial outputs.
Review and commit only these new release files before another tagged-verifier
call; leaving them dirty blocks the next candidate-dependent operation.

The plan is `build/release-evidence/<tag>/interactive/observation-plan.json`.
It contains 70 ordered observations, including the current 35 control/settings
checks, across six evidence categories. The initializer never marks PASS.

Actual people must supply these manifest fields after testing:

- `candidate_sha256`, `tester`, `machine`, `tester_signature`, a different
  `reviewer`, `reviewer_signature`, UTC `started_at_utc`, `completed_at_utc`,
  `reviewed_at_utc`, and substantive `review_notes`.
- Each observation's explicit `result`, `observed_at_utc`, exact ordered
  `checks_confirmed`, and actual `notes`. Keep generated coverage IDs,
  required checks and row order intact. Incomplete or failed observations stay
  honestly incomplete; compilation requires every required row to be PASS.
- Six category-level `supporting_artifacts` groups, containing actual PNG or
  recording paths with `kind` set to `png` or `recording`. Paths are normalized
  repository-relative paths. Use real captures; recording limits are 64 KiB–95 MB.

The required categories include menu/advanced settings, solo play, co-op,
national bases, pickups/minimap and settlement. Physical controller checks,
all alternative controls, camera angle/window sweeps and audio observations
require real execution. Synthetic fixtures and the seven preview images do
not prove them. See `docs/ALPHA_QA_REPORT_TEMPLATE.md` for the exact procedure.

After genuine completion and review, a new local pack can be compiled with:

```sh
make compile-alpha-v2-interactive-evidence DIST_CHANNEL="$QA_CHANNEL" \
  RELEASE_STATUS_FILE="$QA_STATUS" \
  ALPHA_INTERACTIVE_QA_PLAN="$QA_LOCAL/interactive/observation-plan.json" \
  ALPHA_INTERACTIVE_QA_OUTPUT_DIR="$QA_LOCAL/interactive/compiled"
```

This creates `status.next.json`, not an approval or canonical-status overwrite.
For formal PASS, first place reviewed supporting media in the required
versioned `docs/assets/releases/<tag>/evidence/` prefix, point the plan there,
and compile once into a new directory in that same prefix. Commit the reviewed
pack, then verify its `status.next.json` from a clean worktree before deliberate
promotion. Do not reuse old candidate records or alter historical status files.

## Real 30-minute performance record

Required input is a verified candidate and an absent or empty private output
directory. Run on the named performance-QA Mac with native graphics access,
the game focused, and no competing GPU capture or build:

```sh
make run-alpha-performance-qa DIST_CHANNEL="$QA_CHANNEL" \
  ALPHA_CANDIDATE_DIR="$QA_CANDIDATE" \
  RELEASE_PERFORMANCE_QA_OUTPUT_DIR="$QA_LOCAL/performance"
```

For an explicit duration, the equivalent direct runner command is:

```sh
install -d -m 700 "$QA_LOCAL/performance"
python3 -B scripts/run_release_performance_qa.py \
  --project-root "$PWD" --candidate-dir "$QA_CANDIDATE" \
  --output-dir "$QA_LOCAL/performance" --duration-seconds 1801
```

Choose one command, not both. Duration defaults to 1801 seconds; the candidate
finishes a complete sample window before exiting. Although shorter diagnostic
runs are supported, they cannot satisfy the 30-minute release gate.

The runner privately snapshots and hashes the attested ZIP, probes the exact
embedded capability, and executes that extracted binary with only
`--quick-start` plus its generated telemetry path, candidate SHA, nonce and
duration. Do not manually invent a nonce/receipt, add CLI flags, edit the
candidate, attach a patched harness, or substitute a showcase. Its four outputs
are `performance-log-v2.json`, `performance-qa-receipt.json`,
`performance-stdout.log`, and the necessarily empty `performance-stderr.log`.
Any stderr output, watchdog expiry or capture failure prevents a valid receipt.

Acceptance, recomputed from native raw windows and real game events:

| Requirement | Fixed value |
| --- | --- |
| Continuous wall duration | At least 30 minutes |
| Player mode / status `mode_mix` | Exactly `one-player` |
| Completed stages | At least 1 real `StageEnded(Cleared)` |
| Gameplay duration | At least 80% |
| Focused-window duration | At least 95% |
| Average FPS / 1% low FPS | At least 50 / 30 |
| Sample windows | One-second target, each 0.75–1.25 seconds |
| Physical-footprint growth | At most 256 MiB |

The current binary disables controllers, pause, restart, manual stage skipping,
F8 quality changes and F11 in performance mode. Use ordinary P1 keyboard
movement/fire and settlement confirmation; keep clearing/playing real stages.
Escape, window close or a defeat that returns to setup aborts the run. Two-player
performance is not required by this fixed contract; co-op belongs to the
separate interactive gate.

The runner automates measurement and exit, not gameplay. There is no existing
autoplay interface that guarantees a genuine cleared stage and survival for
30 minutes. A separate native OS keyboard driver could operate the unmodified
candidate through real input, but it would need actual successful gameplay and
must be described as automated stress. It is not human/physical-controller QA.
Do not consume 30 minutes on an idle/showcase run expecting it to pass. A human
player is the currently available reliable execution path; do not claim any
thermal, fan, audio, artifact, hang or soft-lock observations without recording
them. The numerical receipt is provenance, not overall acceptance.

For formal promotion, copy the untouched four files into a fresh tracked
candidate evidence directory and bind `extended_session_metrics` to them.
Record tester/machine/session/review facts and derive every numeric detail from
the raw data. The runner does not fill the status or human observation fields.

## Source-free Clean-Mac kit and returned evidence

After fresh status initialization has been reviewed/committed, prepare the kit
on the release workstation. A real, recordable HTTPS URL ending in the exact
attested ZIP filename is required. Staging can be access-controlled, but the
recorded URL must not contain reusable secrets. Staging is not publication.

```sh
: "${QA_DOWNLOAD_URL:?Set the exact candidate HTTPS staging URL}"
make prepare-alpha-v2-clean-mac-qa-kit DIST_CHANNEL="$QA_CHANNEL" \
  ALPHA_CANDIDATE_DIR="$QA_CANDIDATE" RELEASE_STATUS_FILE="$QA_STATUS" \
  CLEAN_MAC_DOWNLOAD_URL="$QA_DOWNLOAD_URL" \
  CLEAN_MAC_QA_KIT_DIR="$QA_LOCAL/clean-mac-kit"
```

The new kit contains only `START_HERE.command`, `clean-mac-plan.plist`,
`README.txt`, and `kit-manifest.json`. Transfer it intact, verify its manifest,
and arrange a continuous screen/external-camera recording on the actual clean
Apple Silicon Mac. Do not install this checkout, Python, make, Homebrew or
raylib on that test account. Existing app approval must be absent, and the
machine must meet the candidate's attested minimum macOS version.

On that Mac, an actual tester runs:

```sh
/bin/zsh -f /path/to/clean-mac-kit/START_HERE.command \
  /path/to/new-clean-mac-intake
```

The destination parent must already exist, and the final directory must not.
The collector asks for the tester name/signature, clean-machine method and
real observations; it collects machine details and command results itself.
The human uses Safari to download the exact candidate, Finder/Archive Utility
to extract it, and Finder to launch it. Record real Gatekeeper dialogs and the
documented Open Anyway path if offered. Do not automate authentication or
approval, edit quarantine attributes, re-sign the app, or set collector test
environment variables. A nonzero `spctl` outcome can be legitimate for an
ad-hoc Alpha and must be retained exactly.

Return the untouched 14-file intake including its empty `COMPLETE` marker,
the original downloaded ZIP, and the continuous capture. COMPLETE means only
collection finished. A different actual reviewer must inspect the video and
compare the observed launch path with the new candidate release notes.
After that review, supply genuine values in the following command:

```sh
make compile-alpha-v2-clean-mac-evidence DIST_CHANNEL="$QA_CHANNEL" \
  RELEASE_STATUS_FILE="$QA_STATUS" \
  CLEAN_MAC_INTAKE_DIR="$QA_RETURNED_INTAKE" \
  CLEAN_MAC_MEDIA="$QA_GATEKEEPER_RECORDING" \
  CLEAN_MAC_REVIEWER="$QA_REVIEWER" \
  CLEAN_MAC_REVIEWER_SIGNATURE="$QA_REVIEWER_SIGNATURE" \
  CLEAN_MAC_REVIEWED_AT_UTC="$QA_REVIEWED_AT_UTC" \
  CLEAN_MAC_REVIEW_NOTES="$QA_REVIEW_NOTES" \
  CLEAN_MAC_RELEASE_NOTE_WORDING_VERIFIED=yes \
  CLEAN_MAC_EVIDENCE_OUTPUT_DIR="$QA_LOCAL/clean-mac-compiled"
```

Every `QA_*` argument in this last command is required factual input; none is
auto-filled by this plan. Set `yes` only after the stated comparison. Media
must be a real MOV/MP4/M4V, 64 KiB–95 MB, with valid video samples and duration;
a PNG is insufficient. The compiler produces a new 19-file pack including
`status.next.json`, without replacing canonical status. For formal PASS,
compile into a fresh `docs/assets/releases/<tag>/evidence/` destination and
commit it before the evidence check. This gate uses only `gatekeeper_launch`,
not the menu/control evidence identity.

## Final boundaries

- `make check-alpha-release-evidence ...` permits honest BLOCKED status; its
  successful exit is not approval. `make verify-alpha-release-ready ...` is the
  strict no-blocker publication gate, from a clean worktree.
- Both QA-lead and release-owner approvals require their actual named actions.
  Audio requires a separate explicit owner ACCEPT decision after reviewing the
  candidate's 22 files and provenance limitations; changing two cues is not
  an approval. Leave absent decisions empty/BLOCKED.
- If source changes after the tag, create a new source tag/candidate and redo
  candidate-bound evidence. Do not relabel old measurements or move the tag.
- Current authority: `docs/RELEASE_CHECKLIST.md`,
  `docs/release-requirements/macos-alpha-v2.json`, initializer/compiler scripts,
  `scripts/run_release_performance_qa.py`,
  `scripts/release_performance_contract.py`, and the screenshot/performance
  parsers in `src/app/`.
