# AI P2 release-readiness review

Verdict: the increment is suitable for continued internal testing, but is not
ready to claim the repository's publication gate. No blocking runtime defect
was reproduced in the reviewed paths. This is a review of the AI P2 increment,
not a fresh audit of all earlier uncommitted art, LAN and training features.
Production code was not modified by this review.

## Findings

### Required interactive acceptance remains incomplete

The ordinary menu and real AI match were previously rendered and captured,
but synthesized keyboard taps did not reliably reach raylib. Physical keyboard,
Bluetooth gamepad, disconnect/reconnect, and actual human/AI cooperation were
not fully exercised. The added native tests directly call the controller with
PlayerInputFrame values; they do not execute the device-read/merge/AI-selection
block at `src/main.cpp:7421`. The tests therefore cannot replace end-to-end
acceptance of either pad driving human P1, suppressing human P2 input only in
AI mode, and retaining isolation when switching back to human co-op or LAN.
This is missing evidence, not a demonstrated physical-input bug.

Required follow-up: record real menu selection, P1 movement/fire, AI P2 action,
pause/resume, death/respawn, restart, return to menu and switching back to
solo/human co-op; exercise the actual Bluetooth controller. Include human/AI
cooperation cases in acceptance rather than treating AI-pair clears as proof.
See AGENTS.md testing guidance and docs/RELEASE_CHECKLIST.md human gates.

### AI regression checks are incompletely connected to CI and coverage

`.github/workflows/macos.yml:34` runs `make test`, which includes the native AI
smoke test, but never runs `make test-ai`. The all-map observation/action parity
checks in `tests/test_ai_player.py` therefore passed locally but are not enforced
for future PRs. They need a CI step, or equivalent dependency-light native
fixtures with the same coverage. The parity-only Python test can be run with
NumPy/Gymnasium; game distribution need not gain any ML dependency.

`Makefile:459` defines APP_COVERAGE_TARGETS without the new AI test executable,
and COVERAGE_TEST_TARGETS consequently excludes it. Existing tests do exercise
AI in normal and sanitized builds; the coverage report does not measure those
executions. Wire an instrumented native AI test into the production-profile
merge. This is a regression/measurement gap, not proof of zero actual testing;
COVERAGE_BASELINE.md explicitly does not set a whole-project merge threshold.

### Current release evidence cannot approve these new bytes

`make verify-alpha-release-ready DIST_CHANNEL=alpha.5` completed the release
contract tests and then failed its strict candidate verifier because the
current Git worktree is not clean. The existing Alpha 5 status refers to an
older immutable candidate and commit, and its manual/clean-Mac/extended-session
entries remain NOT_RUN with approvals BLOCKED. It does not attest this AI
increment. Committing code alone will not satisfy those missing gates.

A new intended source snapshot must receive its own verified candidate and
candidate-bound human QA, 30-minute performance/memory evidence and clean-Mac
launch evidence, as specified by docs/RELEASE_CHECKLIST.md. Do not alter the old
candidate or tag. No candidate, tag, approval or publication was created here.

## Checks and evidence

- Rehashed all 201 files from the preceding full-test source snapshot: unchanged.
  Its successful `make clean`, `make test test-ai` (59 Python tests), and
  `make test-sanitize test-ai-sanitize` remain applicable; see the preceding
  integration validation receipt. No redundant full rebuild was claimed here.
- Inspected observation bounds, route traversal, command edges, 20 Hz scheduling,
  pause/intro/death handling, restart/stage resets, mode switching and LAN
  isolation. AI reads const state and owns no simulation RNG. No gameplay or
  map golden data changed.
- Added a review-only native runtime probe under this directory. On all 35
  canonical maps, with neutral P1 and native AI P2, ran both a constant 60 Hz
  clock and a repeating 1/120, 1/40, 1/60, 0.05 s clock, for at most 60 simulated
  seconds per run. 70 runs produced 73,377 decisions and 8,281 AI shots, with
  zero recorded AI-caused base-damage events and no P1 command writes.
  Three runs cleared, four lost, and 63 hit the short time limit. This probe
  measures execution and boundaries; it is not a human-partner win-rate study.
- Timed the production O2 AI object with a steady clock: mean 13.057 us,
  median 6.667 us, P95 70.209 us, P99 98.958 us, max 398.375 us per decision.
  Timing includes observation and controller work. These local headless figures
  support the low-compute design but do not certify GPU FPS or 30-minute RSS.
- `make -j4 test-dist DIST_CHANNEL=alpha.5`: passed. Verified the current source
  in the development static distribution, including resources/licenses,
  system-only dependencies, signature, extracted self-tests, performance
  capability contract and all 16 verifier rejection cases. This `build/dist`
  ZIP is an un-attested development test artifact, not a publishable candidate.
- `make verify-alpha-release-ready DIST_CHANNEL=alpha.5`: exit 2 at the final
  strict verifier; direct strict invocation also exited 1 with the dirty-tree
  reason. See `release-gate.log` and `strict-status.log`.
- `git diff --check`: passed. Production sources and prior QA artifacts retained.

`results.json` records outcomes; `runtime_probe.cpp` and `runtime-probe.log`
contain the reproducible probe and per-stage results. No new gameplay rules,
training runs, reward changes, assets, releases or remote writes.
