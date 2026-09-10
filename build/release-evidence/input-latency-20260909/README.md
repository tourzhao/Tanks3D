# Controller-response investigation — 2026-09-09

The user reported a slight delay with a Bluetooth controller. macOS identified
one Switch Pro Controller. Code review found callback delivery on a dedicated
USER_INTERACTIVE queue, latched button edges and same-frame input/update/render.
The four-second device probe received no changed-input callbacks, so it did not
measure Bluetooth transport or button-to-screen latency.

Native profiling found a game-side contributor: dense forest/steel views took
longer than the 8.33ms target frame budget. The change caches static forest plans,
canopy vertices/normals/colors and steel triangles. Drawing order and geometry
remain intact. Camera smoothing, stick thresholds, control mappings, firing
rules and the original 35 stage layouts are unchanged.

## Native frame comparison

Both builds use 1280×720 logical resolution, a 2560×1440 Retina framebuffer,
High shadows, Pixel Style OFF and the existing 120Hz pacer. Each case has 60
warm-up frames followed by approximately three seconds of measurement. The
original stages and enemies remain; simulation is held at a fixed state to
compare rendering. Input and window presentation still run. There is no glFinish
or altered GPU synchronization. These are local scene measurements, not a
Bluetooth latency measurement or a sustained 120FPS guarantee.

| Stage / view | Before FPS | After FPS | Mean render time (ms) |
| --- | ---: | ---: | ---: |
| 1 / spawn | 107.2 | 108.0 | 7.34 → 7.30 |
| 1 / center | 110.0 | 109.4 | 8.70 → 8.65 |
| 26 / spawn | 91.2 | 100.1 | 10.41 → 9.25 |
| 26 / center | 66.4 | 75.6 | 15.07 → 13.22 |

The stage 26 center is (13,13), with 88 forest cells and 64 steel cells inside
the visible bounds. Its earlier diagnostic sample at (12.5,16.5) is excluded
from this paired comparison. The empty-forest stage 1 cases provide a control;
their small variation is within the frame/presentation jitter of this short run.

See [summary](frame-comparison.json), [before frames](before-frames.csv),
[after frames](after-frames.csv), [before scenes](before-scenes.csv) and
[after scenes](after-scenes.csv). CPU stage timings include asynchronous GPU
submission; EndDrawing also includes presentation and event polling.

## Cache boundaries and verification

- Forest: one shared, heap-owned cache, at most 676 original map cells. Stage
  changes replace it; changed edge masks rebuild a cell; asset load/unload
  releases it. Each lit/shadow crown allocates only when used. The theoretical
  all-forest upper bound is about 15.3MiB; stage 26 uses about 2MiB.
- Steel: 256 bounded entries with constant-time cell/type lookup and LRU cold
  replacement. The original maps have at most 176 steel cells, so a warmed
  whole-map shadow pass does not continually evict visible steel. Normal and
  permanent variants remain distinct; shadow reuses the same geometry prefix.
  Retained payload is at most about 13.7MiB, with explicit exit cleanup.
- 239 test suites / 15,266 checks passed via `make -j4 test`.
  `make test-sanitize` passed all 13,732 integrated checks. Independent forest
  and steel cache exercises also completed under ASan/UBSan.
- The optimized O2 forest stream was bit-identical across 18,252 cases;
  the steel stream matched across 2,704 cases / 5,743,296 vertices.
- All 19 fixed-time runtime images exactly match the prior source build,
  including controls, pause, pickups, settlement, next stage, solo/co-op,
  pixel modes and four Advanced Settings sizes. No replacement screenshots
  are needed for this visually equivalent change.

[Source and screenshot fingerprints](review-manifest.json) identify the review.
