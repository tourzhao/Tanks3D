# Optional pixel style — 2026-09-09

Advanced Settings now includes **Pixel Style: OFF / ON**, defaulting to OFF.
Left, right or confirm toggles it; Reset restores OFF. The setting remains in
memory while returning to setup, restarting and advancing stages. Gameplay
rules, camera span, model geometry and stage layouts are unchanged.

## Compare the same scene

- [Pixel Style OFF](gameplay-coop.png): full logical scene detail.
- [Pixel Style ON](gameplay-pixel.png): crisp two-pixel scene blocks with
  subtle palette steps and independently sampled light bloom.
- [Advanced Settings](advanced-settings.png): the selected option set to ON.
- [Single-player view](gameplay-solo.png): normal stage 35 spawn, Pixel Style OFF.

The two co-op images are direct 1280×720 production-renderer captures of the
same paused simulation state, with the visual clock held at 12 seconds. They
use original stage 26, yaw −25° and elevation 50°. Both tanks were staged on
legal open cells; the original map grid is intact. No image pixels were edited.

The earlier two-pixel block centers landed between four source texels while
the source texture used bilinear filtering. That averaged adjacent colors and
softened silhouettes. Both modes now fetch an explicit texel. Pixel Style ON
selects one texel per block; OFF retains every texel. Bloom remains separate
and is weaker in both modes. HUD and menu rendering follows scene processing.

[Octopath Traveler's official HD-2D example](https://www.jp.square-enix.com/octopathtraveler/about/)
provided a visual reference for crisp pixel subjects combined with 3D lighting.
The local reference image is not included in the repository. These changes use
our existing procedural geometry and do not import third-party sprites or art.

## Validation

- `make -j4 test`: 239 suites / 15,261 checks passed, including menu input,
  reset/default state, navigation, bundle and architecture checks.
- 39 native production post-process renders at three odd/even target sizes
  passed single-texel color checks with the source deliberately set to bilinear.
  ON → OFF → ON was tested using the same instance; no stale state remained.
- Native Retina verification used a 257×193 logical window and 514×386
  framebuffer. Pixel colors, opaque output, HUD strips and depth state passed.
  See [sampling results](sampling-qa.json).
- Full runtime review covered two-player movement/fire, pause, pickups,
  settlement, next-stage entry, solo/co-op and menus. Advanced Settings was
  visually checked at 1280×720, 900×600, 640×900 and 800×500, in both modes.

[Source and artifact fingerprints](review-manifest.json) identify this review.
