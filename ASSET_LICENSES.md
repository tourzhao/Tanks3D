# Asset Sources and License Scope

The root PolyForm Noncommercial 1.0.0 license governs only project-owned
portions. Third-party licenses continue to govern their files, and the project
license cannot revoke rights already granted by MIT, zlib, or CC0.

## Project-authored content

The procedural vehicle, building, terrain, pickup, effect, and interface art in
`src/`, plus the project's creative modifications and arrangement, are covered
by the root PolyForm Noncommercial license to the extent they are protectable.
Historical names and vehicle facts are descriptive; no trademark or design
endorsement is claimed.

The September 2026 arcade art revision is original procedural geometry and
shader work by the Tanks3D project contributors, under the same root license:

| Source | In-game mapping |
| --- | --- |
| `src/wwii_tank_model.h` | Twelve national player vehicles and four enemy roles; cast armor, continuous tracks, running gear, attachments and wear |
| `src/environment_assets.h` | Residential blocks, shops, workshops, damage fragments, trees and backdrop architecture |
| `src/base_model.h` | Three national field headquarters, their command cores, wall damage and steel-protection appearance |
| `src/main.cpp`, `src/post_process.h` | Armored redoubts, water edges, contact shadows and the shared painted lighting treatment |

SNK's *Metal Slug* is an art-direction reference for compact military
machinery and richly layered arcade scenery. No SNK sprites, textures, meshes,
logos, sound or extracted game content were imported. Reference:
[SNK's Metal Slug page](https://www.snk-corp.co.jp/official/akeaka/titles/metalslug/).
See `docs/ART_DIRECTION.md` for the visual design and gameplay boundaries.

The following neutral albedo textures were generated for this project with
OpenAI image generation and no supplied reference image:

- `resources/textures/battlefield_grass.png`
- `resources/textures/urban_masonry.png`

To the extent any protectable project-authored rights subsist in these files,
they are offered under the root project license. No representation is made
about rights that applicable law does not recognize in generated output.

## Battle City stage layouts

The fixed terrain data in `src/game/classic_stage_layouts.h` reproduces the
35 starting layouts of Namco's NES/Famicom *Battle City*, in displayed stage
order. The original level designs belong to their respective rights holders;
they are not project-authored art and are not relabeled as PolyForm content.

The implementation was checked against the MIT-licensed
[`krystiankaluzny/Tanks` stage files](https://github.com/krystiankaluzny/Tanks/tree/f59aea31638117e20bc03276026bdbb9f8828b47/resources/stages),
reverse-engineered map data, and the original-game maps credited to
Ricardo Sallin in [VGMaps' NES atlas](https://www.vgmaps.com/atlas/NES/index.htm).
Adapted upstream portions retain `LICENSES/MIT-upstream.txt`; that notice
does not purport to grant rights in Namco's original level designs.
See [map sources and verification](docs/BATTLE_CITY_MAPS.md) for the exact
tile mapping, ordering and corrections. Reference screenshots, ROMs, original
sprites and extracted textures are not included in the repository or app.

## Runtime audio inherited from the MIT upstream

All 22 files under `resources/sounds/` were carried byte-for-byte from the
MIT-licensed `krystiankaluzny/Tanks` repository. Its public history credits
Redas Jefisovas (`holoflash`) with the original effect-sound set and its later
refinements; the OGG conversion was merged through upstream pull request 35.
The upstream MIT notice is in `LICENSES/MIT-upstream.txt`.

- Initial authored set: <https://github.com/krystiankaluzny/Tanks/commit/326c2935>
- OGG conversion: <https://github.com/krystiankaluzny/Tanks/pull/35>

The upstream snapshot does not include a separate signed declaration for each
recording. This repository therefore records the strongest available public
provenance but does not independently warrant third-party rights. A
distributor requiring chain-of-title documentation should obtain confirmation
from the contributor or replace the cues.

## CC0 importer probe

| File | Source | License | Runtime role |
| --- | --- | --- | --- |
| `resources/models/tank_basic.glb` | Quaternius, *Animated Tank Pack* (`FA5daiyZQq`) | CC0 1.0 Universal | Optional importer/animation/lighting QA only |

Source: <https://poly.pizza/bundle/Animated-Tank-Pack-0tfvbeAJkU>

See `LICENSES/CC0-1.0.txt`. The model's CC0 terms permit commercial reuse
independently of the non-commercial license on project-owned code.
