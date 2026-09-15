# Arcade machinery and battlefield art

The September 2026 visual revision moves Tanks 3D toward the compact,
characterful military machinery and layered scenery of 1990s arcade games.
SNK's **Metal Slug** informs the project's compact mechanical proportions,
layered scenery and painted materials. The implementation is original
procedural 3D geometry, with no imported SNK game assets.

This artwork is available in the current development source. The published
**Alpha 4** package predates this revision and the adjustable gameplay camera.
See the [development preview](../README.md#current-development-preview) for rendered
examples and the [development guide](DEVELOPMENT.md) to build and run it.


## Chaffee study and coherent tank roster (September 2026)

The USA level-zero player (`Vehicle::M24Chaffee`, HUD `M24 CHAFFEE`) now uses
`src/chaffee_sample_model.h`. Following the single-tank review, the requested
roster expansion applies that casting and material language to all 14 distinct
vehicles, including every national enemy role. The USA Fast enemy now shares
the Chaffee geometry with enemy armor colors; the accepted player rendering is
preserved. `src/arcade_tank_roster.h` reuses the study's section lofts, continuous
belts and beveled wheel faces. `rosterDesign()` in `src/wwii_tank_model.h` assigns
each vehicle its own proportions without changing selection or attachments.

| Family | Level 0 | Level 1 | Level 2 | Level 3 | Shape landmarks |
| --- | --- | --- | --- | --- | --- |
| USA | M24 Chaffee | M4A3 Sherman | M26 Pershing | T28/T95 | Rounded shoulders; broad, low T95 casemate and paired belts |
| USSR | T-70 | T-34/85 | IS-2 | KV-5 | Offset light cabin, rearward crown lean, tall KV-5 and auxiliary turret |
| Germany | Panzer II F | Panzer IV H | Tiger I E | Maus | Squarer cheeks, stronger cupolas, skirts, wide Maus and secondary cannon |

The German Fast enemy retains six road tires (`Sdkfz231SixRad`), and its
Power enemy retains the separate `PanzerIIIL` casting. Existing enemy role to
vehicle mappings remain intact. Armor damage and bonus-carrier pulses still
color the large armor panels; rubber, steel and optics keep independent colors.
P1 gold and P2 green appear on roof and side markings in all player tiers.

The chosen round-shouldered cabin has a narrower base and roof, a broad cheek,
a backward-sloping brow, a recessed visor and a short hollow cannon. A hatch,
service door and one rear exhaust provide a small number of mechanical landmarks.
The track footprint remains centered at X ±0.375 with width 0.29 and length 1.42;
shoe relief touches Y=0. The neutral muzzle remains `(0, 0.77, -0.5624)`, and
no attachment helpers, collision, upgrade rules, camera or gameplay RNG changed.

These models use local material tags 14–17, mapped to the established armor,
steel, rubber and optic responses. Only these tags skip the old screen-space
checker grain, palette rounding and armor highlight patch; their shadow receiver
bias is doubled to avoid dotted self-shadow artifacts on the new casting.
Other materials retain the previous shader path. Pixel Style remains the existing
optional post-process pass, with HUD drawn afterward. No new texture, mesh loader,
resource file, dependency or global model override is introduced.

The original source snapshot, same-camera gray studies, visual iterations,
full native screenshots, 1:1 crops, masks, scripted motion frames and validation
logs are under `build/release-evidence/sample-tank-20260910/`. Normal comparisons
use a 1280×720 window, stage 1 spawn, seed 2048, clock 12, yaw 0°, elevation 50°
and the original 18.5-unit orthographic span. Diagnostic closeups are labeled
separately; they do not alter the gameplay camera. Reference images are documented
in that evidence directory and are excluded from the distributable game.

Build with `make all`. Run the basic sample directly with
`./build/Tanks3D --quick-start`, or use `make run-app` and start solo as USA.
The two alternate gray proportions remain review artifacts, not selectable
production skins. The roster expansion's baseline snapshot, same-camera sheets,
native game crops, all-tier motion sequences and test logs are under
`build/release-evidence/roster-art-20260912/`. Its review cameras are diagnostics;
the default game camera, physical footprint, muzzle helpers and gameplay random
stream remain unchanged. All geometry is compiled into the game; no extra runtime
mesh, texture, asset-loader path or resource manifest entry is required.

## Shape and material language

Vehicles take their visual reference from the compact crew cabin, curved nose
and exposed machinery of the SV-001. [WAVE's official SV-001/I model page](https://www.hobby-wave.com/products/gm033/)
provides front, side and oblique views for judging the silhouette. Proportions
are visual estimates from those views, not measured kit dimensions. No WAVE
photographs or SNK game assets are imported into the project.

A compact crew cabin has full cheeks and a rounded crown, seated over a deep
hull whose nose slopes down between the tracks. The longer running gear leaves
room for the front transmission cover and rear engine deck. Continuous belts
wrap around large end wheels, with steel tread shoes, dark recessed carriers
and exposed hubs. Short fenders and side skirts leave the curved ends visible.
The short cannon has a painted recoil sleeve, a dark brake and a recessed bore;
its visual muzzle tip and flash attachment retain their existing coordinates.
The cabin and hull proportions are adjusted independently of that attachment.

Light, medium, heavy and super-heavy roles retain distinct national shapes:
offset light cabins, rounded American castings, Soviet crowns that lean
rearward, squarer German cheeks and a low, broad T95 casemate with paired
belts on each side. The KV-5 keeps its auxiliary turret, the Maus its secondary
cannon, and the fast armored car its three axles. Offset hatches, viewing slits,
roof equipment, exhausts and stowage follow the new body surfaces. Moving
vehicles use restrained suspension motion without stretching the entire model.
Authored panel normals preserve the shoulder sections without diagonal lighting
seams; tread shoes and wheel faces provide their own normals. Contact shadows follow each vehicle's actual
track or wheel footprint.

Architecture uses warm plaster, exposed brick, oxidized green metal, terracotta
roofs and deep window recesses. Residential shutters, industrial doors,
striped shop awnings, gutters and roof equipment distinguish the three
building families. Roof pieces join where the original terrain fills a
two-by-two lot; partial lots and destroyed cells keep their actual footprints.
Damaged buildings expose broken masonry and their interiors within surviving
brick quadrants. The second pass adds folded residential roofs, brick
chimneys, raised workshop rooflights and seeded broken-wall profiles. Exposed
sections beside partially destroyed cells show floor bands and room partitions.
Forest crowns keep their original cover translucency and seeded tree positions.
Rounded crowns, uneven leaf groups and cooler undersides break up the former
uniform oval shapes. River tiles use deep blue water, stepped shallow banks
and sparse moving highlights whose positions vary across the tile grid.

National bases fit the original Battle City enclosure. Eight low wall tiles
form a Π around the two-by-two command core. The American core carries radio
equipment, the Soviet core has a watch turret, and the German core has a small
copper cupola. Surviving walls show brick courses or steel plates; damage adds
surface cracks without opening a false passage. A destroyed wall leaves only
low rubble. Core destruction and temporary steel protection have separate
appearances, all contained within the original base cells.
Steel obstacles use cool blue-gray thick plates, corner fasteners and crossed
reinforcement ribs. Their bright rims distinguish metal from the warmer brick
buildings. Permanent steel additionally carries a pale gold frame; ordinary
steel retains its existing vulnerability to sufficiently powerful shells.

Lighting supports the models with warm broad highlights and cool green shadow
planes. Masonry, plaster and earth share a painted response; tank paint, steel,
rubber, optics and canvas keep separate material responses. Reduced haze and
bloom preserve color and mechanical details at the actual gameplay camera.

**Pixel Style** in Advanced Settings is optional and defaults to **OFF**.
The normal view retains every source texel at the full logical scene resolution.
When enabled, a two-logical-pixel grid fetches one explicit source texel per
block. This avoids the previous bilinear average at block boundaries, which
softened silhouettes and mixed adjacent colors. Nearest presentation preserves
the edges on Retina screens. The pixel mode adds only restrained palette steps;
light bloom and the vignette remain independent, smooth effects. Bloom strength
is reduced, especially in pixel mode. HUD text, menus, radar and prompts remain
at their original resolution in both modes.

[Octopath Traveler's official HD-2D example](https://www.jp.square-enix.com/octopathtraveler/about/)
informs the separation of crisp pixel subjects and layered 3D lighting. This
game retains its procedural 3D models and does not reproduce Octopath's sprite
assets or depth-of-field treatment. The post-process resolves an opaque world;
foliage and smoke alpha are not multiplied into the scene a second time.

Cannon rounds have a pointed metal body and a short warm tracer. Hits separate
compact steel sparks from brick dust and angular masonry chips. Tank explosions
progress from a brief hot core to orange-red flame lobes, tumbling fragments
and smaller rolling smoke clusters. Their shapes face the current camera and
use layered painted colors. Transparent effects are sorted back-to-front,
retain world depth testing, and do not write depth; only cores and sparks use
additive glow. The fixed particle pool and effect-local random generator keep
these changes independent of simulation timing and gameplay randomness.
Protection uses a thin segmented cyan ring rendered unlit, leaving the tank
visible throughout the spawn shield.

The visible terrain pass rejects only cells entirely outside the current
orthographic image, including its zoom, aspect ratio and camera shake. Padded
bounds retain roof edges, projecting awnings and forest crowns. The complete
arena still casts shadows. Translucent canopies use the mainline camera's
far-to-near ordering before visible cells are submitted, so rotating left or
right does not reverse their overlap.
Tree trunks, roots and branches emit their own surface normals so their
lighting stays stable when preceding offscreen geometry is omitted.

Static forest plans and canopy geometry are cached by map cell, stage and
exposed-edge mask. Steel caches its original world-space triangles and normals
in a bounded set of 256 entries, shared by the visible and shadow passes.
Warm frames replay these shapes instead of reconstructing them. This reduces
CPU work in dense views while retaining the original colors, silhouettes,
transparent ordering and shadow geometry. Forest caches clear on asset unload
or stage changes; the steel cache releases its retained data at application exit.

## Pickup badges and Boat

The nine procedural 64×64 pickup badges share a chamfered enamel frame, dark
ink, warm highlights and broad cool shadows. Their established accent colors
remain on the rims; rubber, cloth, steel and glass retain distinct colors.
The pictograms prioritize their silhouette at the roughly 35-pixel badge size
in a 1280×720 solo view. Point sampling keeps the authored pixels crisp with
Pixel Style both off and on; no HUD or global post-process change is required.

Boat is a compact original river tug in both the badge and the rotating 3D
pickup: a deep blue displacement hull, cream slanted wheelhouse, orange funnel
and open life ring. Connected hull sections provide a narrow keel, full
shoulders and a raised bow. All of its vertices share the same yaw transform,
including the deck fitting; the old crossing rail/box construction is removed.
Since pickups render after scene lighting, broad face colors supply the visual
depth. There is no new shader or external asset. The other eight 3D pickup
models retain their existing geometry.

The 1.53 model scale, 0.90 badge size, contact shadow, float, rotation, blink,
pickup lifetime, effects and probability are unchanged. Review both the
generated icons and actual `--quick-start --bonus-showcase` rendering. Include
all four Boat orientations, normal map occlusion, Pixel Style and co-op.
Evidence and comparison harnesses belong under `build/release-evidence/`.

## Implementation boundaries

- Tank geometry lives in `src/wwii_tank_model.h`: twelve player models across
  three nations and four enemy roles use fourteen vehicle definitions.
  Enemy nations exclude the participating players' selections. American and
  Soviet basic/fast/power/armored enemies use their medium/light/heavy/super-heavy
  models respectively. German enemies retain the Panzer II, Sd.Kfz.231,
  Panzer III and Tiger. Enemy armor colors and bonus-carrier flashes retain
  their gameplay meaning. Models, contact and sun shadows, and muzzle flashes
  all select the same national vehicle; damage does not switch models.
- `ArcadeVehicleSpec`, vehicle selection and visual muzzle attachments retain
  their existing values. The muzzle helpers place the rendered barrel tip and
  flash; simulation shell spawning remains separately defined by
  `shellSpawnPosition` in `src/game/combat_system.cpp`. `ArcadeVisualProfile`
  controls cabin proportions, hull depth, running gear and gun thickness.
  These drawing dimensions do not change collision bounds, projectile paths
  or gameplay randomness.
- Architecture and trees live in `src/environment_assets.h`; building profiles,
  stage seeds, height caps, forest alpha bounds and brick masks retain their
  contracts. Roofs and ruins share major geometry with the shadow pass.
- Headquarters live in `src/base_model.h`. Both passes read the same eight
  one-by-one `StageMap` wall segments and health. In zero-based grid coordinates,
  the walls occupy row 23, columns 11–14, and rows 24–25, columns 11 and 14.
  The core center is `(13, 25)` and its geometry stays inside world coordinates
  `x=12..14, z=24..26`. Its square foundation is `1.84 × 1.84`
  (`kFoundationRadius=0.92`); the courtyard is `1.52 × 1.52`. No base decoration
  extends into surrounding terrain. Health 1–4 keeps a complete wall collider;
  health 0 removes that wall and leaves rubble no higher than 0.12.
- Terrain follows the original 35 Battle City stages. Their source-controlled
  layouts determine roads, obstacles and base space; art must conform to those
  cells rather than clear room for headquarters or generate replacement routes.
- No runtime asset paths or bundle manifests changed. All generated review
  images and binaries belong under `build/`.

## Review

Review enlarged models as well as the adjustable gameplay view. Check all nations
and tiers, enemies, two-player identity, intact and damaged lots, forest cover,
steel protection and the destroyed core. Keep the original 35 stage layouts
and their deterministic hashes; visual work must not rewrite golden gameplay
layouts. Run `make test` after
integration and `make test-sanitize` for the renderer extraction.
Terrain visibility checks compare against raylib's screen projection across
the -45° to +45° horizontal and 40° to 70° elevation ranges, solo/co-op zoom,
map-edge camera positions, and landscape/square/portrait windows. Co-op spans
start at the same 18.5-unit vertical minimum as solo play and may exceed the
former zoom cap in narrow windows. The wider view retains the existing camera
angles and centers on the tank or player midpoint at map edges. Review clipped
and complete renders of identical scenes to catch missing edge geometry.
Large co-op spans move the camera back along its viewing axis to keep foreground
geometry ahead of the near plane. The battle report retains its separate oblique
camera and matching horizontal model spacing, with the complete models fitted
inside the preview panel.
