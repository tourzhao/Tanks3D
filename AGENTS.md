# Repository Guidelines

## Project Structure & Module Organization

Tanks 3D is a C++17/raylib macOS game. `src/main.cpp` remains the integration
hotspot for rules, input, audio, and rendering; follow `docs/ARCHITECTURE.md` and
`docs/REFACTORING_PLAN.md` when moving code. Feature headers live beside it,
while the transitional self-test lives in `tests/`. Runtime assets are under
`resources/`, app metadata under `macos/`, and generated files only in `build/`.

## Build, Test, and Development Commands

- `brew install raylib` installs the macOS dependency.
- `make clean` removes generated outputs; `make test` rebuilds and runs all
  headless checks.
- `make test-unit`, `make test-session`, and `make test-assets` run focused
  groups; only the asset group needs runtime files.
- `make test-bundle` verifies the exact `.app` resource manifest.
- `make test-sanitize` checks memory and undefined behavior; `make coverage`
  reports production-only coverage.
- `make run` launches the executable from its expected working directory.
- `make run-app` opens the macOS application bundle.

## Coding Style & Naming Conventions

Use four-space indentation, braces on new lines, PascalCase types, camelCase
functions, and `kPascalCase` constants. Keep headers self-contained, match
nearby code, avoid unrelated formatting, and keep `-Wall -Wextra -Wpedantic`
clean. Record the source, author, license, and exact mapping for new assets.

Keep new gameplay rules independent of rendering. Prefer command input, seeded
random sources, and game events over raylib input or audio calls in rule code.
Move behavior unchanged before redesigning visual code.

## Testing Guidelines

Run `make clean`, then `make test`, after rule, collision, resource, or build
changes; run `make test-sanitize` before submitting refactors. Treat
`docs/GAMEPLAY_INVARIANTS.md` as a behavior contract. Use
`build/Tanks3D --dump-stage-signatures` to inspect generated layouts; never
replace golden hashes merely to silence a failing refactor.
Manually check menus, one/two-player controls, pause, pickups, stage completion,
and the battle report after visible changes.

## Commit & Pull Request Guidelines

Use short imperative subjects, for example `Fix shell collision near walls`.
Pull requests should explain gameplay impact, list validation commands, and
include screenshots or a short recording for visual changes. Call out altered
rules or assets explicitly and update `ASSET_LICENSES.md` and
`THIRD_PARTY_NOTICES.md` whenever provenance or licensing changes.

## Licensing

New project-owned contributions must be compatible with PolyForm
Noncommercial 1.0.0. Third-party MIT, zlib, and CC0 portions retain their own
terms; never relabel them as PolyForm-only or remove required notices.
