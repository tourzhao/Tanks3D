# Arcade machinery and battlefield art

The September 2026 visual revision moves Tanks 3D toward the compact,
characterful military machinery and layered scenery of 1990s arcade games.
The user's principal reference is SNK's **Metal Slug**. The implementation is
original procedural 3D geometry, with no imported reference-game assets.

## Shape and material language

Vehicles use continuous capsule-shaped track belts, large exposed road wheels,
rounded cast hulls, compact turrets, thick gun collars and open muzzle bores.
Offset hatches, exhausts, stowage, vents and bolted repair plates break up
symmetry. Light, medium, heavy and super-heavy roles retain distinct national
silhouettes and equipment. Moving vehicles use restrained suspension motion;
the old whole-body stretching and oversized head silhouette have been removed.
The second pass separates offset light turrets, sloping rear-crowned Soviet
castings, flat-roof German rolled-plate turrets and the low T95 casemate.
Painted recoil sleeves and dark vented muzzle brakes replace pale plain tubes.
Casting rings reuse their plan directions to reduce repeated trigonometry.

Architecture uses warm plaster, exposed brick, oxidized green metal, terracotta
roofs and deep window recesses. Residential shutters, industrial doors,
striped shop awnings, gutters and roof equipment distinguish the three
building families. Roof slopes meet across their deterministic two-by-two lots.
Damaged buildings expose broken masonry and their interiors within surviving
brick quadrants. The second pass adds folded residential roofs, brick
chimneys, raised workshop rooflights and seeded broken-wall profiles. Exposed
sections beside partially destroyed cells show floor bands and room partitions.
Forest crowns retain the original cover translucency but have
less regular silhouettes and more deliberate olive and yellow-green planes.

National bases are compact field headquarters. Low pitched wings, a redoubt
parapet or copper roofs surround the command core, replacing the oversized
central sculptures. The American core carries radio equipment, the Soviet core
has a watch turret, and the German core has a small copper cupola. Wall damage,
breaches, core destruction and temporary steel armor have their own geometry.
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

- Tank geometry lives in `src/wwii_tank_model.h`; all fourteen vehicle
  definitions remain mapped to the same twelve player and four enemy slots.
- `ArcadeVehicleSpec`, vehicle selection and muzzle attachments keep their
  existing gameplay values. Model details do not consume gameplay randomness.
- Architecture and trees live in `src/environment_assets.h`; building profiles,
  stage seeds, height caps, forest alpha bounds and brick masks retain their
  contracts. Roofs and ruins share major geometry with the shadow pass.
- Headquarters live in `src/base_model.h`. Both passes read the same five
  `StageMap` wall segments and health. The foundation radius remains 2.62 and
  the courtyard radius 1.02; the command core fits within the 0.92 core radius.
- No runtime asset paths or bundle manifests changed. All generated review
  images and binaries belong under `build/`.

## Review

Review enlarged models as well as the adjustable gameplay view. Check all nations
and tiers, enemies, two-player identity, intact and damaged lots, forest cover,
steel protection and the destroyed core. Keep the deterministic stage hashes;
visual work must not rewrite golden gameplay layouts. Run `make test` after
integration and `make test-sanitize` for the renderer extraction.
Terrain visibility checks compare against raylib's screen projection across
the mainline yaw/elevation range, solo/co-op zoom, map-edge camera positions,
and landscape/square/portrait windows. Co-op spans may exceed the former zoom
cap in narrow windows. Review clipped and complete renders of identical scenes
to catch missing edge geometry. The battle report retains its separate oblique
camera and matching horizontal model spacing, with the complete models fitted
inside the preview panel.
