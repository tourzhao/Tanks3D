# Battlefield art review — 2026-09-09

The current source adds rounded forest crowns, blue river shallows, clearly
marked steel, layered fire and smoke, short cannon tracers and a subtle pixel
finish. The minimum gameplay view grows from 15.5 to 18.5 world units (19.35%
more width and height at the same aspect). Large co-op spans also move the
camera back to avoid cutting off foreground geometry in portrait windows.
These images depict the development source, not the downloadable Alpha 4.

## Captures

- [Two-player battlefield](gameplay-coop.png): original stage 26, 1280×720,
  yaw −25°, elevation 50°. Both tanks were placed on legal open cells for
  this art view. The map grid is unchanged; the HUD shows live game state.
- [Single-player battlefield](gameplay-solo.png): original stage 35, 1280×720,
  yaw +30°, elevation 50°, normal starting position and simulated enemies.
- [Battle report](battle-report.png): built-in settlement showcase, with
  demonstration scores. Its separate model-preview camera is unchanged.
- [Terrain details](terrain-detail.png): six enlarged panels from the game's
  geometry, lighting, shadows and post-process, assembled with labels. They
  show forest overlap, both steel variants and original stage 26 water.
  Permanent steel has an additional pale gold frame. Ordinary steel keeps
  its existing vulnerability to sufficiently powerful shells.
- [Battle effects](battle-effects.png): six isolated effects, each at 0.025,
  0.075, 0.16, 0.30, 0.55, 1.0 and 1.6 seconds. This comparison omits the
  scene post-process to expose the effect shapes and transparency directly.

The three gameplay/report files are direct native renderer exports. The two
labelled sheets combine renderer captures; game pixels are not retouched.
No third-party artwork or game sprites were imported. Water, shielding and
other real-time details can differ between repeated captures.

## Validation

- `make -j4 test`: 239 suites / 15,258 checks passed, with bundle and
  architecture checks. Original stage hashes, collision and damage rules
  keep their existing expectations.
- `make test-sanitize`: all 13,724 integrated checks passed under ASan/UBSan.
- Native runtime review exercised two-player movement/fire, pause, pickups,
  settlement, next-stage entry, solo/co-op rendering and both menu screens.
- 72 complete-versus-culled scene comparisons were pixel-identical, covering
  stages 1/10/35, landscape/portrait windows, camera extremes, map edges,
  separated players and shake. 61 cases actually culled cells. All padded
  player bounds remained visible; the minimum image margin was 3.43 pixels.
  The nearest tested foreground plane retained 1.42 world units of clearance.
  See [all camera cases](camera-cases.csv).
- Forest/river/steel bounds were checked across 8,928 / 2,628 / 4 samples.
  Seeded tree positions and the forest alpha/height/overhang limits remain.
- Reversing identical FX particles in storage changed zero rendered pixels
  at three camera yaws. Transparent passes restore depth state. The fixed
  960-particle pool still expires and wraps correctly; tank explosion particle
  count drops from 193 to 75 while separating flash, fire and smoke phases.

[Source and image fingerprints](review-manifest.json) identify this review.
This visual review is not a release attestation or a frame-rate benchmark.
