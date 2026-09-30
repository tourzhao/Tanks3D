# Local sample toolchain

`python3 scripts/setup_godot_sample.py` downloads the pinned, free Godot editor,
godot-cpp source and SCons wheel recorded in `DEPENDENCIES.json`. Every download
is checked against its SHA256 before extraction or installation. The script
builds the small macOS arm64 binding archive; it does not install into
`/Applications` or change global Python packages. A cached archive with a wrong
hash fails instead of being used. `--skip-build` only prepares the tools.

Godot 4.7.2 uses the separately versioned godot-cpp 10.0.0 with `api_version=4.7`.
`build_profile.json` includes RefCounted, OS and godot-cpp's required support
classes. OS is required by the binding's printing implementation.
Both the binding archive and extension target macOS 13.0, matching the
[native Metal requirement](https://docs.godotengine.org/en/stable/about/system_requirements.html).
The tested host is the development M2; this is not certification across OS/device
versions. Godot-only compilation uses dedicated `build/godot/obj/` objects and
needs neither raylib headers nor libraries.

## Files and caches

Tools, archives, Python packages, generated C++ bindings and the static archive
are under `build/godot-tools/`. `make godot-sample` stages the source project,
native extension, sounds and notices into `build/godot/project/`. Import files
and `.godot/` therefore stay in the staged project, not `godot/sample/`.

Setup creates `build/godot-tools/_sc_` next to `Godot.app`. Godot's
[self-contained editor mode](https://docs.godotengine.org/en/stable/classes/class_editorpaths.html)
then keeps editor settings, help cache and templates in
`build/godot-tools/editor_data/`.

This does **not** redirect the game's `user://` directory. On macOS that remains
the platform's application-support path. Godot's editor may create its ObjectDB
snapshot directory there, even during a headless import. The sample saves its
own captures, reports and logs explicitly under `build/`. Interactive play saves
deployment and presentation settings to `user://deployment.cfg`; deterministic
demo and UI self-test modes do not save that profile. In a restrictive execution sandbox, Godot may report certificate
access or `user://` permission errors. The verification script treats those as
errors and requires a run with normal macOS access; it never hides them by
accepting the editor's exit status alone.

## Verification

```sh
python3 -B tests/test_godot_import.py
python3 -B scripts/test_godot_import.py
```

The second command assumes the native extension has already been built. It
stages the project, checks dependency hashes and staged source/native/audio/
license bytes, imports resources, parses every GDScript, loads every declared
scene and runs the actual sample for 180 frames with a fixed input tape. It also
executes the menu's real control signals and checks the resulting native settings,
two-player setup, camera, quick pause and Pixel Style state. Its 22 required
markers also cover keyboard/focus/cache contracts, genuine game-over reports,
an earned record, record timeout, held-confirm safety and record-preserving
redeployment/restart. Playback is suppressed; audio assertions concern resources
and ordered native requests. Structural art, audio-resource and bounded timing
collector checks also run in `--import-only` mode.
It rejects engine, script and
native-loader errors even if Godot exits with status 0, and requires a completed
report containing the native simulation digest. `--import-only` omits the native
and UI runs. `--skip-stage` verifies an already staged project.

Logs and the validation receipt are written to `build/godot/validation/`.
Headless execution uses dummy rendering: renderer names in the report describe
project configuration, not proof of Metal rendering. These checks do not assess
visual quality, GPU performance or physical controls.

The original third-party license texts are in `licenses/`. Godot and godot-cpp
remain MIT-licensed; project-owned adapters retain the repository's PolyForm
Noncommercial license. The SCons wheel is a build dependency only.

## Standalone renderer benchmark

After building/importing the sample, run:

```sh
python3 -B scripts/benchmark_godot_sample.py
```

The runner opens Mobile and then Forward+ as separate standalone Metal game
processes, one at a time. Each uses a 1280×720 window, 1920×1080 internal 3D
rendering, stage 10, seed 20260916 and the same 3,600-frame input tape. There is
no editor, artificial `--fixed-fps` or screenshot capture during measurement.
Normal vsync remains enabled. Benchmark mode restarts after a terminal state
and only collects active gameplay timing samples; the first 120 samples are
discarded. This differs deliberately from the fixed-seed screenshot demo.

`--frames`, `--stage`, `--seed`, `--renderer`, `--timeout` and `--log-dir` can
select another experiment. A completed per-renderer receipt is never overwritten;
choose a new log directory for another run. The default receipts live in
`build/release-evidence/godot-sample-20260916/benchmarks/`.

For a sustained diagnostic, use one renderer and a fresh output directory:

```sh
python3 -B scripts/benchmark_godot_sample.py --renderer mobile \
  --seconds 1200 --stress --thermal --log-dir build/godot/soak-new
```

`--seconds` measures wall duration instead of a fixed frame count and requires
one renderer. Stress uses legal +30% enemy speed/fire/spawn, six HP, 99 lives,
P1 held fire and AI P2 without changing normal defaults. Every valid active
sample after warmup contributes to bounded 0.01 ms histograms, alongside exact
per-minute distributions. Percentiles below 500 ms round upward by at most one
bin; an overflowing percentile reports the actual maximum. Counts, maxima and
16.667/33.333 ms exceedances remain in the receipt. `--thermal` compiles a small
Foundation observer for macOS thermal-pressure classes without privileged
access. Those classes are not temperature or power measurements and do not
establish the cause of frame-time changes. No performance pass is implied.

`--trace-slow-frames` enables a separate all-phase diagnostic in a fresh run.
It covers intro/restart/terminal intervals without warmup exclusion and leaves
the combat-only series unchanged. The first 16 and worst 64 intervals above
33.333 ms are retained alongside total counts, initialization duration and the
full-run maximum context. An interval is attributed to the previous callback's
phase, native tick, restart count, body time and measured segments, never to the
current callback's update. The final callback has no following interval; the
validator requires completed callbacks and exactly one fewer measured intervals.

The trace also records terrain scan counts, distinct cached resource counts and
pipeline counter observations. Counts do not establish a cache miss, compilation
duration or cause of a hitch; monitor updates may lag. Residual time outside the
measured callback includes other nodes, deferred work, rendering, pacing and
diagnostics, and is not GPU-only time. Retained events are bounded and written
with the final report. Use separate receipts because tracing itself adds cost.

Receipts include the exact command, source and staged-file hashes, native game
digest, observed timing report, elapsed wall time and the child process's resident
memory sampled once per second. The runner checks a real Metal startup banner,
rendered geometry, active samples, error-free logs and matching final simulation
digests across the two renderers. The primary frame measurements are monotonic
`Time.get_ticks_usec()` intervals between process callbacks; engine delta is
retained separately because engine pacing/smoothing can make it differ from
actual elapsed time. Callback intervals include vsync and CPU work; they are
neither GPU-only timing nor display-present timestamps. Sampled RSS can miss brief memory spikes. A short run
does not replace sustained or dense-combat acceptance.

## Local standalone app

```sh
make godot-app
make run-godot-app
make test-godot-bundle
python3 -B scripts/package_godot_app.py --verify-only --smoke
```

The local app is `build/Tanks3D-Godot.app`; the original raylib app remains
separate. It uses the arm64 slice of the cached official editor binary as its
runtime, so no export-template download is required. A generated PCK contains
the imported project, sounds, textures and notices. The sole library in
`Contents/Frameworks` is the engine-independent native extension. Package checks
reject raylib and external non-system dependencies. The app
does not depend on the staged project or Homebrew paths at runtime.

Packaging checks exact resource hashes, dependency paths, arm64 architecture,
license records and code signatures. It also launches the bundle executable from
`/private/tmp`, without `--path`, for a 180-frame native smoke and the UI self-test.
Logs and final signed-file hashes are in `build/godot/package/`. An incomplete
new app does not replace an existing verified local bundle. The game still uses
the platform `user://` profile location, as above.

This is an ad-hoc signed development app, not a notarized release or an attested
candidate. The minimum macOS version is derived from both actual Mach-O binaries,
not inferred from the host OS; the extension deployment target is 13.0.
Packaging does not prove
physical-controller behavior, audible playback or two-computer LAN operation.

## Audio verification

The strict import gate loads all 22 unchanged recordings. Native session tests
check ordered play/stop/engine commands independently of simulation digests.
To check actual decoding, routing, priority, engine exclusivity, bounded overlap,
zero-volume mute and stop behavior, run after import:

```sh
build/godot-tools/Godot.app/Contents/MacOS/Godot \
  --path build/godot/project --rendering-method mobile --rendering-driver metal \
  --script res://audio_checks.gd -- --capture-audio
```

The capture bus sends to a muted sink. This proves mixed samples, not audible
speaker/headphone output or physical device latency.
