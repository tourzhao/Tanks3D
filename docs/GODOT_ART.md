# Godot arcade art

`godot/sample/art.gd` builds original, editable 3D meshes in Godot. The geometry
is compiled from authored profiles at first use, cached, and shared by later
instances. This is a visual layer; the C++ simulation owns movement, collision,
damage, upgrades, map cells and random numbers.

## Cast shoulder continuity — September 27

Only the six cast turret bodies use `Geometry.cast_surface_rings`: twenty skin
points retain all twelve structural landmarks and the six authored height rings.
Each fore/aft corner keeps its own rational arc through its original midpoint.
`cast_shoulder_ring` separates the inward lower neck from the upper shoulder at
the first maximum half-width; vertices share one angle-weighted normal within
each group. Roof and underside normals remain hard. This removes triangle seams
without forcing smooth shading across a real abrupt change of slope.

The fixed-camera evidence is in `build/release-evidence/cast-surfaces-20260927/`.
Checks require continuous submitted normals/tints inside each casting group,
unchanged landmarks/envelopes, hard caps, and the existing geometry budgets and
surface-angle limits. Hulls, six welded turret paths, materials and lighting are
unchanged; reference photographs are not imported as assets.

## Vehicle-specific armor planes — September 27

`_armor_plan` now separates fore/aft corner cuts and rear shoulder breadth for
each of the twelve selected variants. Hull plans and turret plans are distinct.
`Geometry.armor_rings` intersects fixed edge planes for welded armor; this keeps
the long cheek/side plates planar as their height and breadth change. Castings
keep independently authored curved sections and local smooth normals. The
general world geometry and material paths are unchanged.

Actual triangle normals and welded-quad coplanarity are checked in
`art_checks.gd`, alongside the existing envelope, material, identity and muzzle
contracts. T90 cheek armor and the M60 optic follow the new supporting rings;
T62 roof fixtures were reseated inside its deliberately small dome cap. Sources,
authored estimates and variant exclusions are in [the surface guide](TANK_ARMOR_SURFACES.md).
Actual before/after and two-iteration evidence belongs to
`build/release-evidence/armor-shapes-20260927/`, separately from prior receipts.

## Current illustrated-realism balance — September 27

Follow the [real-vehicle-based modeling standard](TANK_MODELING_STANDARD.md)
before changing a model. Historical structure determines the layout; illustration
style strengthens its recognizable features. The forward T-34 turret and tall
Sherman hull are protected landmarks, not optional style parameters.

The following chassis refinement uses per-model `chassis_x` / `chassis_z` to
author hull sections, track width and wheel positions with the same tier plan
area. General `chassis` still determines wheel radius and height, so circular
running gear is not squeezed with the hull. Turret stations follow the chassis
length; gun tips remain fixed. Fixtures are seated on their revised supporting
plates, including IS-2 drum saddles and Sherman bogie arms. This adds no mesh
bank or global shader path; the rendering and shadow geometry remain identical.
The [footprint table](TANK_PROPORTIONS.md) documents original game adjustments
separately from the real-vehicle measurement references.

The latest pass keeps the historical chassis layout and gun reach while making
barrels about 1.5× thicker, turret widths 4% broader and turret heights about
9–16% fuller. Belt width grows inward, preserving the outside track footprint.
The main barrel and evacuator use armor paint; small muzzle lips remain exposed
metal. Soviet gray-white uses a dedicated matte Lambert material and a muted
procedural texture base so roofs keep shading headroom. Other national paints,
world materials and global light/exposure are unchanged. The Abrams front fender
and leading skirt have a stronger swept cut to retain separation from Leopard 2.

See `build/release-evidence/stylized-balance-20260927/` for fixed-camera before,
round1, round2 and final actual images, including USSR at normal game size.
No new external asset, material group, extra mesh bank or per-frame generation
is added. Visual muzzle positions and all gameplay are unchanged.

## Previous realistic proportion study — September 27 (historical)

That earlier trial replaced the cartoon proportions for all twelve
vehicles. Model-specific lower hull/turret profiles, narrower longer chassis,
and separate gun overhangs are authored in `_vehicle_profile`; the existing
historical wheel arrangements remain. Gun tips are visual mounts, distinct
from native shell spawn coordinates. Camouflage uses original 256×256 polygon
patterns with filtered edges; the existing Pixel Style option still controls
scene quantization. No third-party image, mesh or texture was imported.
See [proportion research](REALISTIC_TANK_PROPORTIONS.md) and the current evidence
under `build/release-evidence/realistic-proportions-20260927/`.

## Previous cartoon roster — September 27 (historical)

The active player roster is M4A3(75) / M26 / M60A3 / M1A1, T-34/76 / IS-2 /
T-62 / T-90A, and Panther A / Tiger II / Leopard 1 / Leopard 2A4. Every nation
uses enemy role tiers 1 / 0 / 2 / 3 for Basic / Fast / Power / Armored, without
changing the roles' combat behavior. National paint is olive woodland, winter
white over dark green, and matte Panzer gray respectively.

The current direction supersedes the compact Metal Slug crew-pod study with
more recognizable military-cartoon structures: distinct American castings and
an Abrams wedge; angular T-34, oval IS-2, domed T-62 and armored T-90; trapezoidal
Panther, slab-sided Tiger II, rounded Leopard 1 and box-shaped Leopard 2.
The supplied illustration guides broad paint and clear construction, while
the selected historical references guide each type's structure.

The later model-identity pass corrects the rear-biased turret stations of the
eleven non-T34 vehicles individually. Abrams now uses planar front cheek cuts
and a rectangular bustle, Pershing a broad convex mantlet, M60A3 an elongated
casting and right-hand rangefinder, early Leopard 1 a centered mantlet-top
searchlight, and T90A paired dark optical housings beside its welded cheeks.
Commander hatches and identity stripes follow model-specific sides. Historical
variant exclusions and actual baseline problems are recorded in
[the identity audit](TANK_IDENTITY_AUDIT.md); captures use
`build/release-evidence/vehicle-identity-20260927/`. The already-corrected T34,
running gear, muzzle attachments, gameplay and default camera remain unchanged.

`_vehicle_profile` supplies four common chassis sizes and turret envelopes;
`_cartoon_crown` authors twelve separate connected section profiles. The
September 27 pass reshapes those profiles and moves them rearward to expose
slimmer cannon tubes. Belts become narrower within the unchanged outer
footprint. `_cartoon_fender` retains curved, turned-down sheets above the belt.
The later chassis pass replaces the four-tier/eight-side-bank layout with
twelve model profiles in `_chassis_hull_sections` and `_chassis_running_gear`.
Those profiles author hull shoulders, roadwheel stations, return rollers,
drive ends and wheel sizes; `_chassis_details` adds model-specific deck hatches,
cooling/exhaust structures and skirts. Sherman has three paired VVSS bogies;
Panther and Tiger II have separate eight/nine-station overlapping wheel layouts;
IS-2 and Tiger II use steel wheel rims, and T-62 retains its two larger rear gaps.
The type/source matrix is in [chassis references](TANK_CHASSIS_REFERENCES.md).

There are now **24 cached side banks: 12 models × 2 sides × 16 phases = 384
shared running-gear meshes**. Banks are keyed by model and side, warmed before
battle, and reused by player/enemy instances of that model. Updates select a
cached phase; they do not build a new mesh each frame. This replaces the earlier
eight side banks without changing the displacement-driven motion system.
Existing rigid suspension and breathing motion are unchanged. Per-nation/tier and
per-enemy-role muzzle points retain their old values.
`_camouflage_armor_material` generates two original local-space patterns;
mechanical parts and identity/status surfaces remain independent. Normal shadow
rendering uses these same meshes and transforms.

The turret/casting pass is recorded under
`build/release-evidence/grounded-cartoon-20260927/`; the newer chassis pass uses
`build/release-evidence/chassis-detail-20260927/`. Final art checks inspect 729
meshes with at most 3,956 triangles per vehicle; running-gear checks pass 493
cases and 768 geometry frames. The full C++ regression, native parity, Godot
import/UI checks, focused drawing sanitizers and both resource/package paths
pass for these final sources. Source hashes, frozen comparison renders and a
twelve-second motion capture are retained with the evidence. The expanded cache
costs about 2.539 seconds and 17.3 MiB of engine static allocation in one local
CPU/headless setup sample, versus 0.918 seconds and 5.0 MiB before. Re-warming
costs 0.242 ms; these are not GPU, process RSS or frame-rate measurements.
Earlier receipts below attest only their recorded versions. No simulation,
camera, map, UI or global lighting change is part of this tank pass.

See [implementation, sources and verification](CARTOON_TANKS.md). The previous
study below is historical; its old roster, palettes, shared wheel layouts and
six-chassis cache count are superseded by the current twelve-model revision.

## Earlier vehicle studies (historical)

The migration designs use a low, pinched turret seat, broad cast cheeks, a
receding brow and contracted roof. A sloping transmission nose separates the
crew cabin from continuous heavy tracks. One service cover, an offset hatch,
a panoramic optic and an asymmetric exhaust establish mechanical character.
These are new profiles, rather than meshes copied from the raylib renderer.
The existing vehicle names, national role selection and visual muzzle positions
remain compatible with the game.

| Family | Level 0 | Level 1 | Level 2 | Level 3 | Distinguishing silhouette |
| --- | --- | --- | --- | --- | --- |
| USA | M24 Chaffee | M4A3 Sherman | M26 Pershing | T28/T95 | Full rounded cheeks; low wide T95 cabin and paired tracks |
| USSR | T-70 | T-34/85 | IS-2 | KV-5 | Offset light cabin, rearward roof rake, tall KV-5 with auxiliary pod |
| Germany | Panzer II F | Panzer IV H | Tiger I E | Maus | Strong planar bevels, raised cupola, selected side skirts, Maus coaxial gun |

The medium tiers have independent upper hull and turret sections. Sherman has
a higher transmission shoulder and a full, rounded cast cabin. Pershing carries
a longer, wider, lower casting over a flatter deck. T-34 keeps a swept rearward
wedge; IS-2 has broad lower cheeks and a heavier, less raked crown. Panzer IV
has long divided skirt boxes around its smaller turret, while Tiger exposes
the running gear below a broad, nearly vertical square turret. These differences
do not change the shared visual muzzle coordinates or simulation dimensions.

The national silhouette pass separates the underlying castings as well as their
colors. American turrets retain rounded, full cheeks. Soviet light/medium cabins
use a long swept wedge, rearward narrow roof and a shorter optic; T-70 keeps an
offset cabin, while IS-2 has a higher, fuller shoulder than T-34. German cabins
use upright plate faces and broad flat roofs: Panzer II is narrow and tall,
Panzer IV adds skirts, Tiger is wide and lower, and Maus has a long heavy cabin.
Soviet bow armor has a stronger slope; German upper hulls have a broad stepped
plate. The shared tracks, cached running-gear frames, movement and cannon-tip
coordinates are unchanged. Same-camera color, uniform-gray and native-size
silhouette studies are recorded in
`build/release-evidence/national-silhouettes-20260919/`; gray renders are review
artifacts, not additional player skins.

USA and USSR enemy roles Basic/Fast/Power/Armored select levels 1/0/2/3.
German enemies select Panzer II F, the separate six-wheel Sd.Kfz.231,
the separate Panzer III L, and Tiger I E. Both player and enemy armor use USA
army olive (`#55613d`), USSR flag red (`#cd0000`) and German gray/white snow
camouflage over a cool white base (`#d8dedc`), matching
the raylib roster. Armor damage changes neutral brightness and the existing
roof/side status markings; it does not replace national hue or the selected
vehicle. Bonus carriers pulse those markings and neutral armor brightness.
P1 gold and P2 green markings stay on
roof and shoulder panels. Rubber, exposed metal and optics have separate
materials. Bonus-carrier color pulses must use the native snapshot flag.

German armor uses one original 64×64 blotch texture, generated once from
authored polygons and shared between vehicles. A separate armor material
projects it in model-local coordinates, so driving, turning and breathing do
not slide the paint over the surface. Nearest sampling with mipmaps and sharp
projection blends keep the large patches readable without distant shimmer.
Enemy instances own their brightness material while sharing the texture;
other national armor and all identity/mechanical materials remain untextured.
The texture is recreated from `art.gd` in the packaged app; no external asset
file or model geometry is added.

`make_tank(nation, enemy, role, player_id, level)` returns a Node3D with
four cached rigid meshes: `Body` contains the turret, cannon and cabin fittings;
`Hull` contains the lower casting, shoulders and exhaust; `TrackLeft` and
`TrackRight` contain the running gear. The initially hidden `Boat` child contains
flotation equipment. All original casting, wheel, optic and national details
remain present. The broad tread shoes now have thickness at their leading and
trailing edges, and hatch seating rims, one hinge and a framed driver port add
readable mechanical depth. No scratches, texture noise or external models were
introduced.

The casting refinement gives hull and cabin shoulders angle-weighted vertex
normals with a bounded blend; roof and underside retain hard seams. USA castings
are rounder, USSR keeps firmer changes of plane, and Germany keeps the strongest
planar bevels. Broad warm/cool vertex colors belong only to these vehicle
castings. The shared world geometry helpers, materials and shaders are unchanged.
A thinner turret bearing, beveled transmission lid, raised hatch handle, and
shape-fitted asymmetric side access/cooling panels replace blank broad surfaces.
Their supporting profiles, cannon attachment, running gear and rigid motion stay
unchanged. The work is original project-owned procedural geometry; there are no
new external models, textures or asset licenses.

Actual Metal before/after images, two review rounds and normal gameplay captures
are in `build/release-evidence/hud-art-20260918/`. Gallery camera, model scale and
lighting are identical across versions; gameplay receipts include four headings,
Pixel Style on/off and user-selectable angle limits. The improvement at native
game size is smaller than in closeups. The lower casting still has a broad dark
region, and the raised service covers remain deliberately chunky.

The following readability pass is preserved under
`build/release-evidence/tank-readability-20260918/`. A short, near-upright lower
cheek replaces the strongly undercut seat without enlarging the silhouette.
Only the crew-pod vertex colors receive a restrained cool painted bounce;
scene lighting, shaders and shared materials stay unchanged. Wide roofs gain
a shallow beveled front plate, while steeply raked roofs keep their existing
space for the crew hatch. Oval cannon mantlets and a wider optic opening keep
the sight line readable. Low turrets use a supported roof periscope; its rear
edge is limited by the hatch gasket, including the short T-70 roof. The circular
muzzle lip and native attachment are unchanged. All additions use the existing
armor/rubber/optic surfaces, and retain the original hatch/cupola height envelope.
The evidence includes intermediate rejected/adjusted studies and identical
camera before/after views. Improvements are strongest in closeups and front
headings; the backlit cheek remains darker than desired, and some roof layers
still merge in the roughly 60-pixel gameplay silhouettes. These are continuing
art limitations, not reasons to enlarge the game camera or change its framing.

The tank-only shading study under
`build/release-evidence/tank-shading-20260918/` addresses that broad dark band
without adding geometry. The existing `armor` StandardMaterial3D now uses
Lambert Wrap diffuse shading, roughness 0.55 and specular 0.10. A 0.72-roughness
candidate was rendered first, then rejected because its light-facing shoulders
and roof became too flat. The selected response gives backlit cheeks a readable
middle tone while retaining a warmer lit top. It continues to receive and cast
ordinary shadows; it is not emission or a new fill light. Only vehicle armor
uses this material group. Rubber, exposed metal, optics, identity paint, world
materials, Pixel Style and the global lighting remain unchanged. Enemy damage
and bonus-carrier tints still use their existing isolated StandardMaterial3D
instances. Meshes, draw surfaces, bounds, attachments and motion are unchanged.
The built-in response is documented in the
[Godot material guide](https://docs.godotengine.org/en/4.6/tutorials/3d/standard_material_3d.html).
At normal game size the improvement is in more continuous armor values, not
additional detail. Green armor still has limited color separation from earth
backgrounds, and tiny roof edges remain subtle. The archived fixed-camera
images include both candidates rather than concealing this tradeoff.

The player identity study under
`build/release-evidence/tank-identity-20260918/` shifts only USA/USSR player
armor slightly toward cooler greens. Enemy paint and damage colors retain
their previous values. Player roof markings fit the actual clipped casting
between the hatch gasket and periscope: a dark backing carries one continuous
gold strip for P1 or two green segments for P2. The second iteration narrows
the outer border and widens the center gap after native-size inspection.
Both players use identical triangles and normals; color boundaries can split
indexed vertices, so identity contracts compare expanded triangle attributes
instead of requiring identical vertex-buffer deduplication. Rubber, metal
and optics still require identical colors. The new marking adds six triangles
per player, retaining the 13-surface ceiling and 3,818-triangle maximum.
The cold/warm separation is deliberately subtle. Small roof markings remain
subordinate cues at gameplay size, particularly in Pixel Style and under
forest cover; they do not replace the existing player colors and HUD identity.

`set_vehicle_state(vehicle, data, dt)` updates flotation, armor paint, a cyan
clock-freeze cue and suspension motion. Idle breathing gently separates the
hull and delayed cabin response; travel rocks the two track assemblies out of
phase, compresses the hull, and lets the turret follow. Bounded damped responses
settle after start, stop and turn inputs. Armor stays rigid and every part keeps
unit scale. The vehicle root and native yaw are untouched; no turret yaw delay
changes the perceived gun direction. Zero `dt` pauses the pose, and the native
freeze state stops its clock. Recreating a vehicle resets all motion state.

The root exposes `vehicle_name`, `neutral_muzzle` and the combined animated
`visual_bounds` for presentation and diagnostic cropping. Transform the local
-Z forward direction with negative native yaw; use
`Body.to_global(neutral_muzzle)` for the visual flash attachment. Projectile
spawn, ballistics, collision and simulation positions stay native and unchanged.
Enemy state updates only the armor and identity-paint surfaces of each rigid part; rubber and
metal retain their own colors. Geometry and immutable materials stay shared,
while each vehicle owns its suspension state and mutable armor tint. The
extra rigid parts increase the maximum vehicle surface count from 5 to 13;
combined geometry remains below 4,000 triangles (currently 3,818 maximum).

The motion comparison under
`build/release-evidence/godot-polish-20260918/art/motion/` includes the original
source, two actual Metal review rounds, a normal stage 1 capture with Pixel Style
off/on, and a six-second before/after video. It shows idle, start, travel, turn
and stop with identical input, camera, light and scale in both rows. The second
column has exactly the default game's world-units-per-pixel; other columns are
labeled closeups. This is an articulation diagnostic, not a human-input or
frame-rate benchmark. That recorded version used rigid suspension and relative
part motion; it predates the circulating shoes and wheel-face motion below.
Idle movement stays subtle at normal game size.

The subsequent running-gear pass under
`build/release-evidence/running-gear-20260918/` adds circulating raised tread
shoes and rotating broad wheel-face spokes, including the wheeled scout.
Each chassis shares 16 prebuilt mesh phases per side. The phase banks use the
existing rubber/metal materials and track nodes; each update only selects mesh
references, so visible geometry, shadows and building-occlusion hints use the
same frame. The original idle breathing and rigid suspension remain in place.
That historical version's six chassis profiles required 192 shared gear meshes in total,
independent of vehicle instance count. The first running-gear version used up
to 3,788 triangles with at most 13 surfaces; the refinement below reduces this.
Its frontend warmed all six chassis profiles before creating the scene, keeping first-use
mesh construction out of combat. The first version measured roughly a quarter
second of CPU setup on the development M2. Prewarming does not prove that every
first-use GPU upload or pipeline preparation is eliminated.

Wheel/tread phase follows actual snapshot displacement projected along the
current heading, plus opposite left/right travel for a turn. Pressing against a
wall without movement does not advance it; sideways ice drift and lane snapping
do not become forward travel. Pause and clock freeze hold the frame and rebase
the position anchor. Explicit deployment/restart, stage/intro transitions and
creation reset gear history without changing the existing suspension response.
Large position discontinuities have a separate guard that permits normal LAN
catch-up movement. The wheel-to-tread ratio remains an arcade presentation,
not an independent physical simulation of each differently sized road wheel.

The readability refinement under
`build/release-evidence/gear-readability-20260918/` compares twelve-shoe/four-spoke
and ten-shoe/three-spoke candidates against the original twenty-shoe/six-spoke
pattern. The selected ten/three version uses broader shoes with three shallow
top panels that follow the end arcs, plus wider wheel spokes. It preserves the
original wheel angular travel per world unit while halving the repeated pattern
frequency. No speed clamp, time-driven slipping, added shader, material surface
or animation cache frame is used. The appearance improvement is modest in a
still image and principally concerns clarity during movement at normal scale.

The running-gear refinement used at most 3,624 triangles and 13 surfaces.
The casting and service-panel pass used at most 3,720 triangles; the following
roof/optic readability pass uses at most 3,812, still within the 4,000-triangle
ceiling and the same 13 surfaces. Neither changes the gear frame payload. The 192 shared mesh
frames contain 14,515,840 bytes (13.84 MiB) of uncompressed position/normal/color
and index data, down from 14.86 MiB. These payload counts are neither process
RSS nor measured GPU allocation. Geometry checks now verify full-cycle closure,
all sixteen adjacent steps including the seam, matching normals/colors, and
absence of collapsed or duplicate faces. Captures use actual native movement
with identical cameras and 60 Hz frame sampling. Discrete phases, long frames
and small pixel coverage can still alias; the tracked pattern is not guaranteed
to preserve apparent direction in 30 fps footage.

## Battlefield and pickups

Intact buildings use connected two-cell roof profiles: pitched residential,
flat parapet workshops, hipped corner houses and sloping shops. Two bounded
eave/pitch variants and alternating entrance/window bays break the repeated
lot rhythm. Broad roof courses and staggered joints, hip-ridge caps, eave shadow
bands, raised window jambs, recessed glazing and a capped chimney restore
architectural layers that were missing from the first migration. They share the
existing paint surface and remain inside each intact cell; destroyed masks never
retain these roof or facade details. Each hipped roof pitch has its own face normal,
and adjoining cell edges meet exactly. Facade details remain broad enough to read from the game
camera. Destruction builds only surviving brick-mask
quadrants; no complete roofs cover a missing quadrant. Coordinate-derived lot
variation is deterministic and never consumes the gameplay random stream.

The battlefield hierarchy pass uses quieter terracotta, slate and olive roof
colors, keeping the same geometry, ridge highlights and facade detail. Only
the four intact-roof palette values change; damaged bricks, steel, foliage,
terrain, vehicles and global lighting retain their existing responses.
Actual same-state views and the roof-only resource comparison are saved under
`build/release-evidence/battlefield-hud-20260918/`. This improves the visual
balance of large roof areas without making tanks larger or changing forest
concealment.

Steel uses blue-gray plates, bright raised X ribs and a contrasting cap. Forest
now uses three asymmetric crowns at different heights over a visible warm trunk
and two branches. A taller, narrower upper crown and two side tiers restore the
wood/leaf distinction lost in the earlier low, broad clusters. Sixteen cached,
coordinate-derived variants rotate these complete crowns and vary heights and
olive-green paint without consuming gameplay randomness. Foliage alpha remains
0.70; all geometry stays inside the existing 0.499-unit horizontal envelope and
below 1.4 world units. The 204 triangles and two surfaces per cell remain under
the existing 208-triangle budget. The map grid is still visible, and foliage
still intentionally conceals tank detail under cover.
The September 19 palette pass separates warmer crown tops from cooler lower
folds and slightly darker side crowns. Only foliage RGB changes: alpha, vertex
positions, normals, material settings, shadows and occlusion bounds stay the
same. This improves the grouping of visible foliage; it does not expose tanks
through forest cover. Fixed-camera evidence and mesh comparisons are under
`build/release-evidence/readability-20260919/`.

Pixel Style keeps the two-pixel grid on quiet color fields while preserving
contrasting source pixels at fine edges. This retains small cannon, track and
identity features that a single sample per block could discard. It uses the
existing color quantization, no added outline or detail texture, and leaves
Pixel OFF and the separately drawn HUD unchanged. Edges can therefore contain
one-pixel detail: this is a restrained pixel treatment, not a uniform low
resolution render. Forest occlusion and small model features hidden in the
original 3D image remain limitations.

For players obscured by brick buildings, `player_visibility.gd` and its shader
reuse the rigid tank meshes for a subtle identity-colored hint. Only occluded
fragments whose sampled depth falls inside a surviving brick half-cell and its
rendered height range qualify. This is a conservative terrain classification,
not an object-ID buffer. Visible tank surfaces, enemies, steel and headquarters
walls receive no hint. Any overlap with forest cover or a foreground canopy
disables the entire hint immediately; it does not make forest transparent.
The copies are outside the vehicle art tree, cast no shadows and do not expand
the model bounds used for camera fitting.
The hint now separates a muted cool interior from a narrower, lighter gold/cyan
rim. This keeps P1 readable against orange roofs without increasing the warm
fill across the whole hidden tank. It uses the same depth and brick tests,
meshes and two texture samples; no extra rendering pass is added. Two actual
Metal review rounds, unchanged native-state captures, Pixel Style comparisons
and forest/steel/clear negative controls are preserved under
`build/release-evidence/readability-polish-20260918/visibility/`. It remains a
translucent mesh silhouette, not a constant-width outline; overlapping rigid
parts are still visible in closeups.
Water uses an original inexpensive world-space ripple shader. The adapter calls
`set_visual_time(elapsed)` with its presentation clock, so pause and paired
captures freeze the river as well as vehicles. Headquarters
keep their existing footprint and have national roof equipment. Damaged base
walls remain whole colliders until their health reaches zero; visual rubble is
below 0.10 world units. Intact brick and steel tiles each use one material
surface, reducing per-cell draw submissions.

All nine pickups have different geometry: grenade, helmet, clock, shovel,
miniature tank, star, gun, river tug and first-aid box. The Boat symbol uses a
continuous keel-to-shoulder hull, leaning glazed bridge, orange funnel and an
open life ring. No textures or copied game sprites are required.

`set_effect_state(effect, age_fraction, size)` rapidly expands a connected,
irregular cream/orange burst, then transitions to drifting, growing smoke on
large impacts. The bright core cools as the flame fades; smoke fades in during
the overlap and fades out before removal. The burst is 96 triangles and its
three overlapping smoke billows total 348. Their angle-weighted shared vertex
normals remove hard triangular lighting seams, while continuous painted values
and a cooler, darker body preserve contrast against olive ground. The later
fade keeps the cloud visible after the flame disappears. Per-effect material opacity supports
both Mobile and Forward+; both meshes stay shared and cast no shadows. The
adapter owns lifetime and position (0.10 seconds for small flashes and 0.70 for
explosions). `make_explosion(false)` returns only the shared flame mesh and its
mutable material for small flashes; the default `true` retains large-impact
smoke. This avoids allocating a hidden smoke node/material for every shot. No particle system, gameplay randomness or event changes are
needed.

The presentation adapter now retains these instances in `effect_pool.gd`.
It allocates lazily, preserves the 48-active limit, and retains at most 48
small plus 48 large effects. Each instance keeps its own mutable fade materials;
released roots are hidden and fully reset before reuse. This also keeps the
final alpha material variants alive between bursts. Simulation event order,
effect sizes and lifetimes are unchanged; generic impacts retain their original
upward drift. The import gate verifies
fresh/reused visual state, material isolation, saturation, teardown and RNG.

Firing uses a separate, connected cream/amber jet along the gun's local -Z.
`set_muzzle_flash(effect, enabled)` switches the existing small slot's cached
mesh and reuses its fade material. `ShellFired` copies the visible animated
`Body` orientation and muzzle position once; the jet then stays at that world
position for its existing 0.10-second lifetime. It does not drift upward or
follow a tank that turns afterward. If the source is unavailable, the native
event's cardinal direction supplies the orientation. Acquiring an impact slot
or clearing a session restores the original explosion mesh and identity pose.
Large explosions and smoke use their unchanged geometry and animation.

The shell has a broader cream nose and a short amber tail extending backward.
Its local front tip stays at -0.14, and native shell position, speed, collisions
and damage remain unchanged. Both new meshes use one existing hot-material
surface each: 64 triangles for the muzzle jet and 88 for the shell, each below
its 96-triangle budget. They add no lights, particles, textures,
shader passes or per-shot mesh construction. Pixel Style uses its existing
scene treatment, and HUD rendering is unaffected.

The fire review is under `build/release-evidence/fire-readability-20260918/`.
Two actual Metal rounds compare the retained baseline at matching scale in
four directions, with close and normal-pixel-size views and Pixel Style off/on.
The second round shortens the overly white, pointed first jet and strengthens
its amber shoulder. Its two 72-frame clips replay genuine firing events at
60 Hz; the diagnostic rotates that same event data for directional comparison.
Separate full-map captures use the unmodified native directions and camera.
At normal size, the brief flash still occupies only a few pixels, especially
in Pixel Style; the enlarged sheet does not establish distant readability.
The native firing integration check covers both players, all three nations,
enemy shots, gun-tip transforms, world anchoring after a turn, pause, generic
impact placement, cross-kind pool reuse and complete snapshot preservation.

## Interface

The interface now takes its layout and typography from the existing raylib
frontend. Setup has a centered TANKS 3D title over a full-screen dark-blue
gradient, selectable rows with gold chevrons, and a four-tier technology line
for each player's nation. Its order is PLAYERS, STAGE, LIVES EACH, P1 NATION,
P2 NATION when applicable, LOCAL NETWORK, and ADVANCED SETTINGS. Advanced and
LAN have separate pages instead of unfolding forms inside setup. Volume remains
an extra Godot setting. `arcade_menu_row.gd` and `arcade_backdrop.gd` draw the
rows and background using original code-native 2D geometry.

The font is the existing raylib 6.0 default bitmap font, exported locally to
`resources/fonts/arcade.fnt` and `arcade.png` by
`scripts/export_arcade_font.cpp`. Its 128×128 atlas and 224 glyph metrics are
used with nearest texture filtering; no smoothing or synthetic emboldening is
applied. The original raylib glyphs retain their zlib license, separately from
the project's PolyForm interface code. See
[font provenance](../ASSET_LICENSES.md#shared-arcade-bitmap-font). No downloaded
font, SNK asset or generated bitmap artwork is used.

The combat HUD deliberately retains the open upper center from the previous
Godot revision. Separate P1 gold and P2 green plates show identity, AI control,
vehicle, tier number/name, hit points, lives, score and temporary states.
Health severity has a colored cue, and streak display starts at one as in
raylib. Stage, enemy count and headquarters status share the existing
lower-right radar footprint. Pixel Style is shown in settings and pause;
the HUD is outside the scene pixel treatment. The radar reads native terrain
and damage masks, headquarters-wall health and steel state, pickups, enemy
creation and bonus-carrier flags rather than constructing alternate rules.

Both renderers now use 320-by-80-pixel player cards at the standard 1280-by-720
canvas, expanding to 102 pixels high only for temporary player state or a
streak. Three paired rows retain nation/AI and lives, the full vehicle name and
numeric HP, then the tier name and exact score. Shared stage/enemy/base-steel
information appears once above the lower-right radar instead of being repeated
in both player cards. The raylib radar has moved into the same 168-by-196-pixel
corner footprint as Godot; its terrain grid and Godot's now both use integer
five-pixel cells. Damaged half-cells split into two and three pixels with no
fractional seams. Battle camera transforms, the Godot 74-pixel camera safety
reservation and all native map/actor data remain unchanged. Controls still
appear at the bottom, with space reserved for the radar.

`arcade_plate.gd` retains the original cut-corner player and report plates.
The transparent `hud_root` keeps the original 74-pixel top band as a
conservative camera safety margin. Clearing its middle does not move or zoom
the battlefield camera. `hud_occluder_rects()` returns the actual visible
player/mission/radar rectangles for layout verification.

Settlement uses a full-screen report with per-player TYPE/K.O./POINTS rows,
animated native kill counts, totals, bonuses, stage points, scores and lives.
`report_preview.gd` reuses the existing tank meshes in an isolated World3D and
SubViewport; its own shallow report camera does not change gameplay framing.
The new-record screen is separate from the report. Native pickup messages are
shown as a bottom toast without reconstructing their gameplay meaning.

The earlier compact-HUD baseline and iterations remain under
`build/release-evidence/arcade-ui-20260918/`. The raylib/Godot alignment uses
`build/release-evidence/ui-alignment-20260918/` for source snapshots, actual
matched captures and validation receipts. Final acceptance for this alignment
requires new visual and gate checks; the earlier receipts attest only their
recorded source. Layout checks supplement actual menu and gameplay review.

## Review

`art_review.gd` is a diagnostic scene, separate from the normal game camera.
It renders a labeled sheet with the same light, camera, viewport size and scale
for every subject. Its JSON receipt records the window, camera and projected
vehicle bounds. These enlarged diagnostic images do not replace normal-view
acceptance.

After preparing the Godot project, run the selected engine executable with:

```sh
"$GODOT_BIN" --path build/godot/project --script art_review.gd -- \
  --review=roster --output="$PWD/build/release-evidence/godot-migration-20260917/art/roster.png"
```

Other review modes are `enemies`, `pickups`, `terrain`, `roster-game`,
`forest`, `forest-game`, `effects` and `effects-game`.
`roster-game` uses 50° elevation, 0° camera yaw, east-facing vehicles and a
6.988889-unit orthographic span in each 320×272 cell. This gives the same
world-units-per-pixel as the normal 1280×720 game at an 18.5-unit span; models
are not enlarged. Labels record projected bounds and the exact camera setup.
It uses the normal game light colors and energies, as do the closeup sheets.
The `-game` forest and effect sheets use the same gameplay pixel scale. Forest
patches include unshielded P1/P2 models under cover. Effect sheets render eight
large-impact ages and four small-flash ages, labeled with actual milliseconds,
mesh bounds and camera span. `--animate-frames=60` runs a fixed 60 Hz diagnostic
loop, which can be recorded with Godot's MovieWriter. This is an effect review,
not gameplay or a performance benchmark.
The baseline source,
headless mesh checks and actual render evidence belong under
`build/release-evidence/godot-migration-20260917/`. Check normal-view silhouettes,
all movement directions, visible muzzle placement, player markings, boat,
armor colors, damaged cells and both Pixel Style settings before acceptance.

The strict Godot import gate also runs `art_checks.gd`. It verifies all 24
player/enemy configurations, finite nondegenerate meshes and outward normals,
shared geometry with isolated mutable materials, muzzle attachment support,
24 start/stop/turn/idle sequences with unit-scale rigid parts, ground contact,
paused/frozen poses, combined moving bounds and unchanged root/muzzle metadata,
all nine pickups, all base states and 512 brick-mask cases across roof variants.
It also checks all 676 foliage coordinates for finite cached meshes, footprint,
height, and a 208-triangle/two-surface budget; explosions have a 512-triangle
budget across their two shared surfaces.
The check script loads its art dependency explicitly and fails if it does not
compile, if a check group aborts or if expected case counts are incomplete;
a deliberate broken-script probe verifies that failure exits with status 1.
It checks that art creation does not advance the global random stream. These
structural checks supplement actual render review; they cannot judge whether
a silhouette is attractive or readable at the gameplay camera.

The final bounded art pass and its original baseline are under
`build/release-evidence/godot-finish-20260917/`: actual Metal Mobile effect
sheets, a gameplay-scale forest sheet, 68 recorded review frames (60 animated
plus setup/capture frames), and independent geometry checks. That pass used 156 triangles per tree. The subsequent forest/smoke refinement
is preserved under `build/release-evidence/godot-refinement-20260917/art/`.
Two actual Metal rendering rounds use the unchanged normal stage 10 camera;
both native snapshots and camera receipts equal the archived baseline. The
final clumps use 208 triangles and two surfaces per tile. Closeup and native
pixel-scale effect sheets, paired Pixel Style captures, under-cover player
checks, source snapshots, and the independent contract results are included.
The remaining visual limits are the readable cell spacing, softened detail
in Pixel Style, and recognizably stylized smoke lobes in closeup.

The next forest study is preserved under
`build/release-evidence/godot-continuation-20260918/art/`. The first actual
render was rejected because a continuous crown field resembled a green fabric
sheet. The second uses two fitted branch masses and retains irregular tree
silhouettes. A third palette check reduces foliage brightness to distinguish warm green
tank bodies without changing leaf opacity. All studies include full stage 10
captures with Pixel Style off and on at the same normal camera; the selected
study also has a native-scale unshielded P1/P2 cover sheet. No camera, post-process, vehicle or smoke change
belongs to this art pass.

## Environment, roster and combat feedback pass

The September 19 arcade pass is recorded under
`build/release-evidence/arcade-upgrade-20260919/`. Light vehicles have shorter,
narrower crew pods and exposed track shoulders; medium vehicles keep a taller
cab, while heavy vehicles use broader, lower castings. The T95 wedge, KV-5 high
cabin and long Maus retain their separate roles. Light shoulder identity paint
lies on the upper armor where the running gear cannot hide it. Muzzle mounts,
running gear, rigid motion and model scale remain independent of these profiles.

Forest variants redistribute the three authored crown masses into umbrella,
tapered, leaning and broad shapes inside the original whole-tree bounds. The
trunks, foliage opacity, terrain membership and concealment rules remain intact.
Folded and offset roof profiles introduce building-group variety inside the
existing occupied cells. One ground mesh adds muted shoulders and short stone
segments along existing open-cell edges; it has no collision or shadow and is
rebuilt only when the map rows change, not when a vehicle moves.

The existing corner HUD now uses team paint and health segments. A short footer
follows the most recent accepted keyboard/controller input, while full controls
live in the pause panel. Damage, pickup and upgrade flashes use native semantic
events, deduplicate repeated ticks, freeze on pause and reset with the session.
An upgrade requires both a collected Star/Gun event and a real tier increase.
Input observation does not consume commands or change controller ownership.
The HUD reservation, camera, original rules and Pixel shader are unchanged.

## Source and license

Source/author: project-owned procedural artwork and shaders, implemented for
Tanks 3D with Codex assistance. License: PolyForm Noncommercial 1.0.0, matching
the project's new contributions. Exact runtime mapping: every mesh and its
material comes from `godot/sample/art.gd`; `art_review.gd` only arranges those
meshes for review. The reusable section-building approach derives from the
project's earlier sample code; the migration profiles and terrain/pickup
designs are authored here. No external mesh, texture, purchased asset, SNK
sprite, model photograph or generated bitmap artwork is distributed by the
procedural 3D changes. The interface's exported raylib default font is a separate
zlib-licensed asset, described above; it is not relabeled as PolyForm-only.
Godot itself retains the separately recorded MIT engine license.
