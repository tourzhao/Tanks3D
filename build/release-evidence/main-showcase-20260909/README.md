# Tank silhouette rebuild — 2026-09-09

Direct captures of the rebuilt procedural tank models. These depict the source
build, not the downloadable Alpha 4 package. The three game screenshots are
1280×720 exports from the built-in screenshot path. The comparison sheet joins
six unscaled 512×420 renderer captures and adds labels; tank pixels are not
retouched. No WAVE photographs or SNK game assets are included.

The gameplay images use normal starting positions and simulation on original
Battle City stages 1 and 2; the solo view includes the spawn-protection effect.
The battle report uses the built-in settlement
showcase; its scores are demonstration values.

## Proportion comparison

`tank-proportions.png` shows the M24 before the rebuild (top) and after it
(bottom), viewed from the side, front and three-quarter angle. All six panels
use the same orthographic scale of 200 pixels per world unit. Camera height
centers each full model; there is no per-model zoom or resizing.

The old renderer is from `f28cfcd7bbe45843cad0f041e828d62560b6929e`.
The [before](tank-dimensions-before.csv) and [after](tank-dimensions-after.csv)
CSV files record four vehicles' extents and component dimensions. Extents
include the antenna and all other visible geometry, with raster measurement
precision of approximately 0.01 world unit. These are visual dimensions,
not collision bounds or measurements of the reference kit.

| M24 | Length | Width | Height | Length/width |
| --- | ---: | ---: | ---: | ---: |
| Before | 1.075 | 1.470 | 1.320 | 0.73 |
| After | 1.460 | 1.080 | 1.135 | 1.35 |

The rebuild narrows and lengthens the running gear, lowers the crew cabin,
adds a deep sloping nose and exposed rear engine deck, separates the end wheels,
and uses steel tread shoes around a recessed carrier. Contact shadows follow
the new footprints. See [art direction](../../../docs/ART_DIRECTION.md).

## Reproduce game screenshots

Build with `make all`, then run from the repository root. Use output paths
that do not already exist, and keep the computer awake during capture.

```sh
./build/Tanks3D --stage=1 --camera-yaw=-45 --camera-elevation=45 --quick-start-2p --release-screenshot=build/gameplay-coop.png --release-screenshot-frame=1440
./build/Tanks3D --stage=2 --camera-yaw=30 --camera-elevation=50 --quick-start --release-screenshot=build/gameplay-solo.png --release-screenshot-frame=1800
./build/Tanks3D --stage=10 --quick-start-2p --settlement-showcase --release-screenshot=build/battle-report.png --release-screenshot-frame=360
```

Simulation time depends on frame pacing, so enemy positions and other live
state can vary across captures even with the screenshot mode's fixed seed.

## Validation

- `make -j4 test`: 239 suites and 14,969 checks passed, plus bundle and
  architecture checks. The 35 original map layouts and gameplay rules retain
  their existing expectations.
- `make test-sanitize`: all 13,435 integrated checks passed under ASan/UBSan.
- All 12 player appearances and 4 enemy roles were captured at one world scale
  in side and three-quarter views. All 32 geometry masks remained inside the
  image, with at least 50 pixels of margin. A left-front KV-5 view additionally
  checked its auxiliary turret attachment.
- Scripted runtime review exercised two-player movement/fire, pause, pickups,
  battle reports, next-stage entry, solo/co-op rendering and menus.
- Running-gear geometry was checked across 168 vehicle/motion samples; crew
  meshes were checked for closed edges, valid normals and nondegenerate faces.


## Image fingerprints

| Image | SHA-256 |
| --- | --- |
| `gameplay-coop.png` | `4603adb5824dfac425c4c0c2fa377c2fbcc81a20495f667e88350fe3f1afaca6` |
| `gameplay-solo.png` | `f13e5897d3ef8e327b5c6080585cb5fc70032c0787eff667fcfc0396fbd33db7` |
| `battle-report.png` | `68818ae9f32a3966fc9919b8ef1541307e074994e763a785393e8488413e9f1a` |
| `tank-proportions.png` | `97f9829927222a18fbe68dd9806ebc6467887ed5c74f36f9e5c8116ff267f237` |
