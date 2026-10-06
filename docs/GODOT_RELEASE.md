# Godot Alpha candidates and publication

Godot is the active frontend. `make godot-app` produces a local development
app; it is not release approval. The original raylib candidate scripts,
attestations and tags remain historical and unchanged. Do not publish a new
raylib package by accidentally using the old `make alpha-candidate` entry point.

## Prerequisites and candidate construction

Use an Apple Silicon Mac with Xcode Command Line Tools and Python 3.9+.
Run `make godot-setup` for the hash-pinned Godot SDK/runtime. Candidate regression
also exercises the retained shared-core adapter and needs `brew install raylib`
(6.0); raylib is never linked into the Godot app.

Commit all intended changes, including new Godot scripts and resources, to a
clean worktree. Choose the version through `macos/Info.plist` and create the
exact immutable tag `v<VERSION>-godot.alpha.N` at HEAD. For example, after
committing version 0.1.0 for the first Godot Alpha:

```sh
git tag v0.1.0-godot.alpha.1
make godot-candidate GODOT_CHANNEL=alpha.1
make verify-godot-candidate GODOT_CHANNEL=alpha.1
```

Creating a tag does not push or publish it. The builder never creates commits,
tags or releases. It refuses dirty/untracked source, a missing/stale tag, an
existing candidate, failed gates or a concurrent candidate build. Do not run
other builds in the same checkout while constructing a candidate.

It performs a clean rebuild, release-tool/boundary tests, warning-clean debug
build, shared rules/legacy adapter regression, ASan/UBSan, Godot native/socket
checks, strict import/UI/presentation checks, package tests, two-instance LAN
and the existing production coverage report. Each gate has a timeout and output
budget. These are the same production checks used during development, without
updating gameplay golden data or weakening tests to approve a package.

The payload uses the pinned official Godot runtime already used by the local
app, a PCK and the engine-independent extension. No paid service or export
template download is needed. Its engine is the cached editor runtime (including
its debug title suffix), not a custom optimized engine build. Treat that exact
runtime as part of performance and compatibility QA.

Only after all gates and extracted-app smoke checks succeed is the candidate
atomically installed at `build/release/godot/<tag>/`. It contains:

- The ZIP and SHA-256 sidecar.
- An attestation binding version/build, commit/tag, archive, every bundled file,
  exact source-file manifest, QA requirements and gate logs.
- The signed-package receipt and captured gate outputs.

The bundle's version comes from the candidate version; its build number is the
source commit count. The bundle includes the source commit/tag and uses
`io.github.tourzhao.tanks3d.godot`, separate from the local development identifier.
Verification checks resource and file hashes, original notices, arm64/system
dependencies, actual Mach-O minimum OS and ad-hoc signatures, then launches the
extracted app from outside the repository without `--path`. No local resource
directory is required by that app. Ad-hoc seals detect changes; they are not a
Developer ID/notarization claim or publisher authentication.

The builder preserves logs under `build/release-evidence/godot-candidate-*` and
creates a **NOT_RUN** QA status there. `make clean` preserves candidates and
evidence. A successful candidate build is not permission to publish.

## Candidate-specific acceptance

Use the generated status, or initialize a new file while HEAD is still the tag:

```sh
make init-godot-release-status GODOT_CHANNEL=alpha.1 \
  GODOT_QA_STATUS=build/release-evidence/my-godot-qa/status.json
```

Create the containing directory first. Existing statuses are never overwritten.
The status is bound to this ZIP's hash, source identity and the exact semantic
checklist. It reuses the established gameplay, bases, pickups, report, control
and advanced-setting requirements in `macos-alpha-v2.json`. Godot uses Tab for
Pixel Style; the old raylib F8/N/B shortcuts are not claimed. Godot-specific
groups add human/AI co-op, two-Mac LAN, Bluetooth devices, clean-Mac/Gatekeeper,
audio, paused efficiency and an extended session.

For each observation record the tester, machine, real UTC completion time,
notes and evidence paths relative to the status file. Each evidence entry has
`path` and `sha256`; each group requires an actual 64 KiB–95 MiB MOV/MP4/M4V
recording. Container structure and hashes are checked. A recording still needs
human review for its contents; changing checkboxes is not evidence of hardware
testing. Attach the corresponding measurement logs/reports as well.

The long-session checklist retains at least 30 minutes, one completed stage,
average 50 FPS, 1% low 30 FPS, at most 256 MiB memory growth, at least 80% gameplay
and 95% focus. Record the clock/method used; the diagnostic Godot benchmark's
process intervals alone are not display-present measurements. Include Pixel
Style off/on, wide co-op and thermal observations. Run and record the exact
candidate app, not a newer source build. Short diagnostics and earlier model
captures do not approve this gate.

The `extended_session` observation also needs `metrics_report`, a path present
in its hashed evidence list. That JSON uses schema
`tanks3d-godot-session-metrics-v1`, the exact `candidate_sha256`, and numeric
`duration_seconds`, `stages_completed`, `average_fps`, `one_percent_low_fps`,
`memory_growth_bytes`, `gameplay_duration_ratio` and `focused_duration_ratio`.
Provide `worst_thermal_state` (`nominal` or `fair`), `measurement_tool`, `clock`,
`memory_metric`, `method`, and `raw_logs` paths also present in the evidence list.
The verifier enforces the thresholds and hashes; reviewers must cross-check the
measurements against those raw logs. It does not certify a manually entered
summary or a process-interval benchmark as display-present telemetry.

Record known issues and review them explicitly. P0/P1 or unresolved issues block
approval; a remaining P2/P3 needs a named owner and an explicit acceptance reason.
After reviewing all evidence, the QA lead and release owner each add an approval:

```json
{"name":"actual reviewer", "decision":"approved", "at_utc":"actual UTC time",
 "report_sha256":"SHA-256 of canonical status excluding approvals"}
```

Canonical JSON uses sorted keys, separators `(',', ':')` and UTF-8, as implemented
by `json_digest` in `scripts/godot_release.py`. A changed observation or issue
invalidates both approvals. No tool automatically supplies a human signature.

```sh
make verify-godot-release-ready GODOT_CHANNEL=alpha.1 \
  GODOT_QA_STATUS=build/release-evidence/my-godot-qa/status.json
```

This command fails on missing/failed observations, invalid evidence, stale
approvals, package/source/tag mismatches or a dirty caller tree. It has no
`--allow-blocked` approval mode. Only a full pass permits proposing publication
of that exact ZIP, checksum and attestation with the honest QA/known-issue notes.
Publication remains a separate user-authorized action.

## After documentation advances HEAD

Do not move a tested tag to include later QA documents. Use:

```sh
make verify-tagged-godot-candidate GODOT_CHANNEL=alpha.1
```

The command copies the candidate into an isolated checkout of its attested
commit, runs that tag's verifier and checks that the caller's tag and candidate
were not changed during verification. `verify-godot-release-ready` runs both
package and candidate-bound human-status verification using that tag's code.

## CI

The `Godot arm64` job uses the standard `macos-15` Apple Silicon runner,
as listed in [GitHub's runner documentation](https://docs.github.com/en/actions/reference/runners/github-hosted-runners).
It installs the pinned tools, then runs release-tool fixtures, native sanitizers,
real loopback TCP, import/UI/presentation, bundle and two-process LAN checks.
The separate shared-rules job retains historical adapter/regression coverage.
Both jobs are expected to pass for a mainline change. The workflow alone does
not enforce this: required status checks must be configured in a GitHub branch
protection rule or ruleset. The 2026-10-06 audit found no such protection on
`main`. Hosted headless checks cannot replace a real Metal performance run,
physical controllers or a second Mac.
