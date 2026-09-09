# Arcade machinery and battlefield art

The September 2026 visual revision moves Tanks 3D toward the compact,
characterful military machinery and layered scenery of 1990s arcade games.
SNK's **Metal Slug** informs the project's compact mechanical proportions,
layered scenery and painted materials. The implementation is original
procedural 3D geometry, with no imported SNK game assets.

This artwork is available in the current `main` source. The published
**Alpha 4** package predates this revision and the adjustable gameplay camera.
See the [current main preview](../README.md#current-main-preview) for rendered
examples and the [development guide](DEVELOPMENT.md) to build and run it.

## Shape and material language

Vehicles follow the SV-001 reference's substantial upper body, short thick
cannon and rounded track ends. A tall, full turret occupies approximately the
central hull's width and most of its length. The short, low chassis leaves a
small front deck beneath the gun. Continuous capsule-shaped belts have high
curved ends, exposed road wheels and short rounded fenders. The turret grows
forward around the established gun attachment; the muzzle tip stays fixed.
Painted recoil sleeves, dark vented brakes and open bores make the short cannon
read clearly at gameplay scale.

Light, medium, heavy and super-heavy roles retain distinct national shapes:
offset light turrets, rounded American castings, rear-crowned Soviet castings,
flat-roof German armor and a broad, substantial T95 casemate. Offset hatches,
exhausts, stowage and vents follow the new roof and fender positions. Moving
vehicles use restrained suspension motion without stretching the entire model.
Casting rings reuse their plan directions to reduce repeated trigonometry;
shorter belts use fewer straight tread shoes without increasing mesh detail.

Architecture uses warm plaster, exposed brick, oxidized green metal, terracotta
roofs and deep window recesses. Residential shutters, industrial doors,
striped shop awnings, gutters and roof equipment distinguish the three
building families. Roof pieces join where the original terrain fills a
two-by-two lot; partial lots and destroyed cells keep their actual footprints.
Damaged buildings expose broken masonry and their interiors within surviving
brick quadrants. The second pass adds folded residential roofs, brick
chimneys, raised workshop rooflights and seeded broken-wall profiles. Exposed
sections beside partially destroyed cells show floor bands and room partitions.
Forest crowns retain the original cover translucency but have
less regular silhouettes and more deliberate olive and yellow-green planes.

National bases fit the original Battle City enclosure. Eight low wall tiles
form a Π around the two-by-two command core. The American core carries radio
equipment, the Soviet core has a watch turret, and the German core has a small
copper cupola. Surviving walls show brick courses or steel plates; damage adds
surface cracks without opening a false passage. A destroyed wall leaves only
low rubble. Core destruction and temporary steel protection have separate
appearances, all contained within the original base cells.
Ordinary steel tiles use chamfered armored redoubts with raised hatches,
embrasures, bolts and ochre identification panels.

Lighting supports the models with warm broad highlights and cool green shadow
planes. Masonry, plaster and earth share a painted response; tank paint, steel,
rubber, optics and canvas keep separate material responses. Reduced haze and
bloom preserve color and mechanical details at the actual gameplay camera.

The visible terrain pass rejects only cells entirely outside the current
orthographic image, including its zoom, aspect ratio and camera shake. Padded
bounds retain roof edges, projecting awnings and forest crowns. The complete
arena still casts shadows. Translucent canopies use the mainline camera's
far-to-near ordering before visible cells are submitted, so rotating left or
right does not reverse their overlap.
Tree trunks, roots and branches emit their own surface normals so their
lighting stays stable when preceding offscreen geometry is omitted.

## Implementation boundaries

- Tank geometry lives in `src/wwii_tank_model.h`: twelve player models across
  three nations and four enemy roles use fourteen vehicle definitions.
  The basic and heavy enemies reuse the Panzer II and Tiger definitions;
  the fast armored car and long-gun enemy have separate definitions.
- `ArcadeVehicleSpec`, vehicle selection and muzzle attachments keep their
  existing gameplay values. `ArcadeVisualProfile` controls the larger turrets,
  short chassis, high tracks and thicker guns independently. Model details do
  not consume gameplay randomness.
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
may exceed the former zoom cap in narrow windows. Review clipped and complete
renders of identical scenes
to catch missing edge geometry. The battle report retains its separate oblique
camera and matching horizontal model spacing, with the complete models fitted
inside the preview panel.
