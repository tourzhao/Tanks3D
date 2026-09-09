# Current-main showcase — 2026-09-08

Direct game captures from source commit
[`e2a861d4d8733e4bf3ec1cd1d44ca8e9cd54901a`](https://github.com/tourzhao/Tanks3D/commit/e2a861d4d8733e4bf3ec1cd1d44ca8e9cd54901a),
after restoring the original Battle City maps and adjusting the tank and
headquarters proportions. These depict the source build, not the downloadable
Alpha 4 package. All three PNGs are 1280×720 exports from the game's built-in
screenshot path; no external image editing was applied.

The two gameplay images use normal starting positions and simulation. The
battle report uses the explicit built-in settlement showcase to display the
T28/T95 and IS-2; its scores are demonstration values.

## Reproduce

Build with `make all`, then run each command from the repository root. The
exporter requires output paths that do not already exist. On macOS, keep the
computer awake during the capture.

```sh
./build/Tanks3D --stage=1 --camera-yaw=-45 --camera-elevation=45 --quick-start-2p --release-screenshot=build/gameplay-coop.png --release-screenshot-frame=1440
./build/Tanks3D --stage=2 --camera-yaw=30 --camera-elevation=50 --quick-start --release-screenshot=build/gameplay-solo.png --release-screenshot-frame=1440
./build/Tanks3D --stage=10 --quick-start-2p --settlement-showcase --release-screenshot=build/battle-report.png --release-screenshot-frame=360
```

The captured executable SHA-256 was
`9fb02eff747953a8766ff9ca7926c33bf7153bee44cf9df810369ed3cbd78bc0`.
Compiler/toolchain differences may produce a different executable hash; gameplay
frames also depend on timing and simulation randomness.

## Review completed

- `make clean`, then `make -j4 test`: 239 suites and 14,969 checks passed,
  plus the bundle and architecture checks.
- `make test-sanitize`: all 13,435 integrated checks passed under ASan/UBSan.
- All 35 original map grids matched the reference transcription cell-for-cell
  (23,660 cells). [Map sources and corrections](../../../docs/BATTLE_CITY_MAPS.md).
- 72 camera views on stages 1, 10 and 35 compared complete and culled rendering
  with zero differing pixels, including landscape and portrait windows.
- Scripted runtime review exercised two-player movement/fire, pause, pickups,
  battle reports, next-stage entry, solo/co-op rendering and menus. Enlarged
  player/enemy/headquarters galleries and these final captures were inspected.
  This review does not claim a new physical-controller session or FPS benchmark.

| Image | SHA-256 |
| --- | --- |
| `gameplay-coop.png` | `b0ee819c80188285c9e366ded0baffcfd110c543502d43088b517b476956a631` |
| `gameplay-solo.png` | `af0471c5317afe4c17cff2454a4e1c0fe4b8268a8c07c0ec7cb6f9c345208d22` |
| `battle-report.png` | `565c9c04258f10e5214fb86f6073ac9b71643a9468b09e6a6c8a078ca20f7e18` |
