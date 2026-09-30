# Arcade machinery and battlefield art

The latest user-requested adjustment widens all twelve tank models by 20% along
their local lateral axis in both renderers. Length, height, individual layouts,
paint and native collision rules stay unchanged. This is a presentation choice
on top of the per-vehicle profiles, not a revised historical measurement.

The current armor-surface pass separates each vehicle's front cheek, sidewall,
roof shoulder and rear bustle construction. A4 keeps a broad flat front and
narrow edge bevel; Abrams has long oblique cheeks; Panther and production
Tiger II have different inward-sloping plate towers. Cast bodies retain their
individual shoulder and lower-neck curves. See [armor surface construction](TANK_ARMOR_SURFACES.md).
Welded faces use actual coplanar geometry and hard normals, not a shared rounded
box or a normal trick. Existing paint, world lighting and game camera remain.
The six cast turret skins now retain their individual section landmarks while
sampling each rounded corner more closely. Continuous shoulder normals remove
triangle seams; the roof and lower-neck junction remain structural hard edges.
This treatment is restricted to cast turrets, leaving welded plates and hulls intact.

The current tank direction, updated September 27, 2026 after the realistic trial,
is illustrated military machinery between cartoon and realism. The twelve selected
vehicle layouts stay recognizable: modestly fuller turrets, substantial painted
gun tubes, and thicker belts provide game-size clarity. Sherman stays tall, M60
remains taller than Pershing, and Abrams/Leopard 2 keep low angular plate forms.
Soviet winter paint is muted gray-white with cool shadows rather than near-white;
USA woodland and German Panzer gray remain separate national palettes.

The working rule is **real-vehicle structure first, stylization second**.
Before each model edit, follow the [modeling standard](TANK_MODELING_STANDARD.md):
identify the exact variant, separate measured data from estimates, and check its
protected shape landmarks. T-34 keeps its forward turret and long rear deck;
Sherman keeps its high, substantial hull. Illustration style must preserve those
relationships, not normalize every vehicle into the same cartoon proportions.

The latest chassis pass replaces the common track aspect with per-model width
and length profiles while retaining equal tier plan area. Sherman, IS-2 and early
Leopard 1 are relatively slender; Pershing and T-90 remain broader. Hull heights,
round wheel radii and turret dimensions stay independent of those axes. See
[the authored footprint table](TANK_PROPORTIONS.md) and the fixed-camera evidence
in `build/release-evidence/vehicle-proportion-pass-20260927/`.

The user-supplied illustrations guide volume and painted plane separation only;
none are imported as game assets. Original procedural meshes retain the previous
historical wheel/station/details. Gun-tip coordinates, physics, maps, default
camera, world lighting and interface are unchanged. See the latest implementation
and actual-render evidence in [the tank study record](CARTOON_TANKS.md).

This artwork is available in the current development source. The published
**Alpha 4** package predates this revision and the adjustable gameplay camera.
See the [development preview](../README.md#current-development-preview) for rendered
examples and the [development guide](DEVELOPMENT.md) to build and run it.


## Previous cartoon roster and direction (historical)

The September 26 cartoon trial implements the requested M4 → M26 → M60 → M1,
T-34 → IS-2 → T-62 → T-90, and Panther → Tiger II → Leopard 1 → Leopard 2
rosters in both renderers. USA uses olive woodland camouflage, the Soviet family
uses warm winter white over green, and Germany uses matte dark Panzer gray.
The September 27 refinement gives each type a distinct turret construction,
separates turret and engine deck, narrows the belts within the same outer
footprint, and makes the cannon read as a slimmer tube. Broad highlights,
curved fenders and a few readable mechanical features keep the cartoon style.
No pixels or mesh from the reference are included in the game. See
[the current work and earlier trial records](CARTOON_TANKS.md),
[shared size standard](TANK_PROPORTIONS.md), and the
[US](US_TANK_REFERENCES.md), [Soviet](SOVIET_TANK_REFERENCES.md) and
[German](GERMAN_TANK_REFERENCES.md) historical references.

### Earlier September study (historical)

The following account documents the previous roster and palette. It is retained
as a record of the geometry, attachment and animation work on which the current
trial builds; old model names and paints below do not describe the active roster.

The USA level-zero player (`Vehicle::M24Chaffee`, HUD `M24 CHAFFEE`) now uses
`src/chaffee_sample_model.h`. Following the single-tank review, the requested
roster expansion applies that casting and material language to all 14 distinct
vehicles, including every national enemy role. The USA Fast enemy shares
the Chaffee geometry; both use the American national armor palette.
`src/arcade_tank_roster.h` reuses the study's section lofts, continuous
belts and beveled wheel faces. `rosterDesign()` in `src/wwii_tank_model.h` assigns
each vehicle its own proportions without changing selection or attachments.

| Family | Level 0 | Level 1 | Level 2 | Level 3 | Shape landmarks |
| --- | --- | --- | --- | --- | --- |
| USA | M24 Chaffee | M4A3 Sherman | M26 Pershing | T28/T95 | Rounded shoulders; broad, low T95 casemate and paired belts |
| USSR | T-70 | T-34/85 | IS-2 | KV-5 | Offset light cabin, rearward crown lean, tall KV-5 and auxiliary turret |
| Germany | Panzer II F | Panzer IV H | Tiger I E | Maus | Squarer cheeks, stronger cupolas, skirts, wide Maus and secondary cannon |

National identity now changes the major armor sections as well as the paint.
American cabins retain rounded cast shoulders. Soviet cabins use a swept wedge
with a receding roof and a sloped hull bow; the T-70 stays offset and the IS-2
has a taller shoulder than the flatter Pershing. German cabins have upright
plate sides, broad flat roofs and small bevels, with a narrow Panzer II,
skirted Panzer IV and broader heavy cabins. Optics and cupolas follow these
surfaces instead of sharing one wide visor. Both frontends preserve the
existing running gear, neutral muzzle positions and national vehicle mappings.
Same-camera colored and gray renders, native-size crops and runtime checks for
this revision are in `build/release-evidence/national-silhouettes-20260919/`.

The German Fast enemy retains six road tires (`Sdkfz231SixRad`), and its
Power enemy retains the separate `PanzerIIIL` casting. Existing enemy role to
vehicle mappings remain intact. Both frontends use American army olive
(`#55613d`), Soviet flag red (`#cd0000`) and German snow camouflage with a
cool white base (`#d8dedc`) and broad gray patches. These are authored game
palettes, not claims of exact historical paint specifications.
Warm highlights and cool recesses retain the
painted relief. Enemy armor loss modestly lightens the national paint; the
existing armor-status and bonus-carrier pulse colors stay on roof and side
markings. Rubber, exposed steel and optics keep independent colors.
P1 gold and P2 green appear on roof and side markings in all player tiers.

The Soviet base follows this [flag color reference](https://www.schemecolor.com/soviet-union-flag-colors.php).
The snow camouflage is original procedural art: broad patches follow the
vehicle's local surfaces and remain separate from metal, rubber, optics and
identity markings. No external reference image is distributed.
Actual before/after palette captures and validation are under
`build/release-evidence/olive-snow-20260919/`.

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

Current vehicles prioritize recognizable construction at the normal game
camera. M4, M26 and M60 use different cast profiles; M1 has a low wedge and
rear bustle. T-34 has an angular turret, IS-2 an oval casting, T-62 a low dome,
and T-90 grouped angular armor. Panther, Tiger II, Leopard 1 and Leopard 2 use
trapezoidal, long slab-sided, rounded and broad box-shaped turrets respectively.
The same national palette must not be their only identifying feature.

Hull, turret, belts and cannon are derived from each selected vehicle's structure
and stylized deliberately. Turret stations follow that vehicle's fighting
compartment; never move every turret rearward just to expose more gun tube.
Check the existing visual muzzle and flash connection explicitly. Belt width and
length can vary within the existing maximum envelope, preserving ground contact
and four-tier plan-area growth.
Keep the existing rigid suspension and breathing motion; do not stretch the
whole model or alter the gameplay camera to sell new proportions. Authored
normals and warm/cool painted planes should explain the large surfaces before
small fixtures are added. Rubber, exposed metal, optics and P1/P2 markings remain
separate from national armor paint.

The earlier SV-001 studies above remain historical development records; their
compact pod, short-nozzle and uniformly swollen turret targets are no longer
the current tank standard. The surrounding battlefield retains its existing
arcade art direction:

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
  their gameplay meaning in local markings, without replacing the national
  body color. Models, contact and sun shadows, and muzzle flashes
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
