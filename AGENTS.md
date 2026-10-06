# Repository Guidelines

## Persistent Project Decision: Godot Only

User-confirmed on 2026-09-27: **Godot is the sole active development mainline.**

- Implement new art, rendering, UI and features for Godot. Do not also update
  the raylib frontend merely to keep the two versions visually or functionally
  aligned, unless the user explicitly requests raylib work.
- Freeze the raylib frontend as a historical reference. Retain its source and
  existing evidence; this decision does not authorize deleting the old version.
- Keep the engine-independent C++ gameplay/AI/networking core and its
  GDExtension integration. Godot-only development does not require rewriting
  tested rules in GDScript. Shared-core changes still need relevant regression
  checks, including existing legacy-adapter checks when affected.
- Build, launch, inspect and package the Godot version by default. Use
  `make run-godot-app` (or `make run-godot`), `make test-godot-core`,
  `make test-godot-import` and `make test-godot-bundle` as applicable.
  The legacy aggregate `make test` does not replace these Godot checks.
- Godot CI covers the active frontend. Formal Godot candidates use
  `make godot-candidate`, `make verify-godot-candidate`, and
  `make verify-godot-release-ready`; see `docs/GODOT_RELEASE.md`. The last
  command requires candidate-bound human/hardware QA and owner approval.
  The old `alpha-candidate` and `verify-alpha-release-ready` remain historical
  raylib tools and cannot approve the Godot build.
- The unqualified `make`, `make run` and `make run-app` targets still select
  raylib until their entry points are explicitly migrated. Do not confuse
  those historical defaults with the chosen development direction.

This decision supersedes older guidance to maintain both rendering frontends
or treat Godot as an optional experiment. It does not claim a new release has
been published or the unqualified development Make defaults have been migrated.

## Project Structure & Module Organization

Tanks 3D uses Godot with an engine-independent C++17 game core on macOS.
The active frontend is in `godot/sample/`, with GDExtension glue in
`godot/native/` and `src/godot/`. `src/main.cpp` is the frozen raylib integration.
Follow `docs/ARCHITECTURE.md` and `docs/REFACTORING_PLAN.md` when moving code.
Tests live in `tests/`, assets in `resources/`, legacy app metadata in `macos/`,
and generated files only in `build/`.

## Build, Test, and Development Commands

Godot commands above are the active frontend entry points. The following
existing commands also cover shared rules and the retained raylib release;
their presence is not a requirement to duplicate new Godot art or UI work.

- `brew install raylib` installs the macOS dependency.
- `make clean` removes disposable build/test outputs but preserves immutable
  candidates in `build/release/` and QA evidence in `build/release-evidence/`;
  `make test` rebuilds and runs all headless checks.
- `make test-unit`, `make test-session`, and `make test-assets` run focused
  groups; only the asset group needs runtime files.
- `make test-bundle` verifies the exact `.app` resource manifest.
- `make test-sanitize` checks memory and undefined behavior; `make coverage`
  reports production-only coverage.
- `make test-release-status` checks the Python release-gate contract.
- `make check-alpha-release-evidence DIST_CHANNEL=alpha.3` validates an
  incomplete status; `make verify-alpha-release-ready DIST_CHANNEL=alpha.3`
  is the required no-blocker publication gate.
- `make run` launches the executable; `make run-app` opens the app bundle.

## Coding Style & Naming Conventions

### Tank art workflow

Before changing tank geometry, read `docs/TANK_MODELING_STANDARD.md` and the
affected vehicle's linked references. Start from the selected real variant's
dimensions and structural landmarks, then stylize them for game-size clarity.
Record sourced measurements, visual estimates, and game adjustments separately.
Preserve model-specific features (especially the forward T-34 turret and tall
Sherman hull); do not derive all vehicles from a common cartoon silhouette.
Check fixed-camera side/front gray renders and normal gameplay views against
the vehicle checklist. Existing tests and generic national shape differences
do not establish historical or visual correctness by themselves.

### Code conventions

Use four-space indentation, braces on new lines, PascalCase types, camelCase
functions, and `kPascalCase` constants. Keep headers self-contained, match
nearby code, avoid unrelated formatting, and keep `-Wall -Wextra -Wpedantic`
clean. Record the source, author, license, and exact mapping for new assets.

Keep new gameplay rules independent of rendering. Prefer command input, seeded
random sources, and game events over raylib input or audio calls in rule code.

## Testing Guidelines

Judge optimization by whole-frame latency, stalls, memory, startup/build time
or total shipped size. A large percentage in a microsecond-scale helper does
not establish a meaningful game improvement. Add caching, extra state or a
larger test/maintenance burden only when measured overall benefit justifies
that complexity; prefer simpler changes and remove experiments that do not.

Run `make clean`, then `make test`, after rule, collision, resource, or build
changes; run `make test-sanitize` before submitting refactors. Treat
`docs/GAMEPLAY_INVARIANTS.md` as a behavior contract. Use
`build/Tanks3D --dump-stage-signatures` to inspect generated layouts; never
replace golden hashes merely to silence a failing refactor.
Manually check menus, one/two-player controls, pause, pickups, stage completion,
and the battle report after visible changes.
Never treat the release evidence target's `--allow-blocked` mode as approval.
Post-tag candidates must be rechecked with `verify-tagged-alpha-candidate` from
a clean worktree; do not move an attested tag to accommodate later docs.

## Commit & Pull Request Guidelines

Use short imperative subjects, for example `Fix shell collision near walls`.
Pull requests should explain gameplay impact, list validation commands, and
include screenshots or a short recording for visual changes. Call out altered
rules or assets explicitly and update `ASSET_LICENSES.md` and
`THIRD_PARTY_NOTICES.md` whenever provenance or licensing changes.

## Licensing

New project-owned contributions must be compatible with PolyForm
Noncommercial 1.0.0. Third-party MIT, MIT-0, zlib, Apache-2.0, WTFPL,
public-domain, and CC0 portions retain their own terms; never relabel them as
PolyForm-only or remove required notices.
