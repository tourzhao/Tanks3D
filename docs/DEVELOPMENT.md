# Development Guide

This page is for contributors. Players should start with the
[project README](../README.md).

## Build and run

Tanks 3D requires macOS, a C++17 compiler, Homebrew, and raylib 6.0.

```sh
brew install raylib
make clean all
make run
```

`make all` creates `build/Tanks3D` and `build/Tanks3D.app`. Use `make run-app`
to open the application bundle. If raylib is outside Homebrew's active prefix,
pass `RAYLIB_PREFIX=/path/to/raylib` to `make`.

## Test

```sh
make test
make test-sanitize
make coverage
```

Focused targets include `test-core`, `test-game`, `test-app`, `test-assets`,
`test-bundle`, and `test-release-status`. Run the full suite after gameplay,
collision, resource, or build changes. Visual changes also require a manual
one-player and two-player check.

## Project map

- `src/core/` contains shared, raylib-free values and settings.
- `src/game/` contains gameplay rules and systems.
- `src/app/` and `src/audio/` contain presentation and audio boundaries.
- `resources/` contains runtime models, textures, sounds, and stage data.
- `tests/` contains deterministic unit, integration, and release-tooling checks.
- `macos/` contains application-bundle metadata.

## Reference documents

- [Repository guidelines](../AGENTS.md) — style, naming, tests, and pull requests.
- [Architecture](ARCHITECTURE.md) — module boundaries and current risks.
- [Gameplay invariants](GAMEPLAY_INVARIANTS.md) — rules that refactors must keep.
- [Refactoring plan](REFACTORING_PLAN.md) — incremental modernization sequence.
- [Coverage baseline](COVERAGE_BASELINE.md) — deterministic coverage priorities.
- [Release checklist](RELEASE_CHECKLIST.md) — packaging, QA, evidence, and approval.

`make dist` creates and verifies a self-contained macOS Alpha archive. Follow
the release checklist before publishing, and never move an attested release tag
to accommodate later documentation changes.

Project-owned contributions must remain compatible with the
[PolyForm Noncommercial License 1.0.0](../LICENSE). Record the source, author,
license, and exact in-game mapping for every new asset; see
[asset licenses](../ASSET_LICENSES.md) and
[third-party notices](../THIRD_PARTY_NOTICES.md).
