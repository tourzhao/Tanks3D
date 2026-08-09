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

The following neutral albedo textures were generated for this project with
OpenAI image generation and no supplied reference image:

- `resources/textures/battlefield_grass.png`
- `resources/textures/urban_masonry.png`

To the extent any protectable project-authored rights subsist in these files,
they are offered under the root project license. No representation is made
about rights that applicable law does not recognize in generated output.

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
