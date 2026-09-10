# Alpha 5 release preparation

Prepared on 2026-09-09 from the enemy-nation and two-source audio changes.
These records are engineering checks, not signed interactive, clean-Mac,
audio-owner, or publication approval evidence.

## Repairs

- Both upstream MIT notices now participate in the release validator's shared
  list of allowed repository evidence and mandatory audio-decision hashes.
  A notice cannot replace the separately reviewed decision report.
- Legacy rendering calls without a nation again select German enemy models
  and American player models. Explicit national calls retain their behavior.
- The geometry-command regression test runs in normal tests and ASan/UBSan.
  The standalone nation-boundary tests also run under ASan/UBSan.

## Verification

- Clean rebuild: `make clean`, then `make -j4 test test-sanitize coverage debug`.
  Game tests, strict compilation and ASan/UBSan completed successfully.
- The drawing regression has 3 suites / 80 checks, including explicit national
  calls and shadow forwarding. It passes both normal and optimized NDEBUG
  builds; against the previous headers, it detects 16 legacy-enemy failures.
- Release-status verifier: 75 Python tests passed, including missing/tampered
  notices, canonical publication paths and the independent report requirement.
- Drawing coverage is reported separately for the two production headers;
  neither report emits a profile mismatch warning. Both legacy overloads
  execute 16 times. Their main-program unused maps must not be combined with
  the standalone test's active maps.
- `performance/` retains all raw frames and reproduction helpers for six
  short heavy-enemy rendering scenes. See its README for measured results and
  the below-threshold German 1% low sample. It is not formal performance QA.

The immutable Alpha 5 candidate gate log is the authoritative record for the
final tagged build. Actual controller play, the clean-Mac download/launch,
30-minute gameplay measurements, audio-owner decision and independent
approvals remain separate release gates.
