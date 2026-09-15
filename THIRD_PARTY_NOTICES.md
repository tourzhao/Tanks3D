# Third-Party Notices

The root `LICENSE` applies only to project-owned portions. The materials below
retain their original terms, which take precedence for those materials.

## Arcade art direction

The September 2026 procedural model revision takes stylistic inspiration from
SNK's *Metal Slug*. The new vehicle, architecture and headquarters geometry is
project-authored; no SNK game assets are distributed. *Metal Slug* and its
associated marks belong to their respective rights holders. This project has
no affiliation with or endorsement from SNK. The source-to-model mapping is
recorded in `ASSET_LICENSES.md`.

The Chaffee study and its tank roster extension additionally study WAVE's SV-001/I model photographs
for three-dimensional structure. Those photographs and SNK's gameplay screenshots
remain reference material belonging to their rights holders; neither is included
in the game assets. The geometry and paint colors are original project work,
licensed under the repository's PolyForm Noncommercial 1.0.0 terms. No new
third-party runtime asset or dependency is introduced by the roster extension.

The nine pickup badges and the Boat pickup's tug geometry in
`src/bonus_assets.h` are original work by the Tanks3D project contributors,
under PolyForm Noncommercial 1.0.0. They use no imported icon pack, boat model,
game sprite or additional runtime dependency.

## Upstream Tanks project

Parts of the implementation and runtime audio were adapted from or developed
against [`krystiankaluzny/Tanks`](https://github.com/krystiankaluzny/Tanks),
copyright 2025 Krystian Kałużny and distributed under the MIT License. The
complete upstream notice is reproduced in `LICENSES/MIT-upstream.txt`.

The 35 fixed 26x26 terrain layouts embedded in `src/game/classic_stage_layouts.h`
reproduce the original NES/Famicom *Battle City* starting maps, checked against
the upstream stage files and original-game map references. Adapted upstream
portions retain the MIT notice. The original Namco level designs remain the
property of their respective rights holders; they are not claimed as
project-owned or relabeled under PolyForm. The procedural 3D presentation is
project-authored. See [map sources and verification](docs/BATTLE_CITY_MAPS.md)
and `ASSET_LICENSES.md` for ordering, corrections and reference credits.

Twenty runtime OGG files retain their original names and bytes from
[`krystiankaluzny/Tanks@f59aea31638117e20bc03276026bdbb9f8828b47`](https://github.com/krystiankaluzny/Tanks/tree/f59aea31638117e20bc03276026bdbb9f8828b47/resources/sounds).
The upstream sound-set history credits Redas Jefisovas (`holoflash`), beginning
with the commit explicitly described as
[`add original effect sounds`](https://github.com/krystiankaluzny/Tanks/commit/326c2935).
The OGG conversion used by this repository was also contributed by `holoflash`
in
[`krystiankaluzny/Tanks` pull request 35](https://github.com/krystiankaluzny/Tanks/pull/35).
See `ASSET_LICENSES.md` for the audio provenance limitation.

## Battle City musical cues

The two remaining runtime cues come from
[`JustoSenka/BattleCity@3a07004ba8e53baea74ff70d2ecc22b017eb9b20`](https://github.com/JustoSenka/BattleCity/tree/3a07004ba8e53baea74ff70d2ecc22b017eb9b20/Assets/Audio):
`Assets/Audio/levelstarting.ogg` is copied without byte changes to
`resources/sounds/stage_start_up.ogg`, and `Assets/Audio/gameover.ogg` to
`resources/sounds/game_over.ogg`.

The source repository declares the MIT License, copyright 2019 Justas Glodenis.
Its full notice is in
[`LICENSES/MIT-JustoSenka-BattleCity.txt`](LICENSES/MIT-JustoSenka-BattleCity.txt).
That repository declaration is not independent confirmation of rights in the
original *Battle City* music or these recordings. No project authorship or
PolyForm-only ownership is claimed. See the
[audio provenance and hashes](ASSET_LICENSES.md#runtime-audio).

## raylib 6.0, embedded dependencies, and shadow-map example

This project links against [raylib](https://www.raylib.com/) and adapts parts of
raylib's official shadow-map example. raylib and its examples use the
unmodified zlib/libpng license. Copyright (c) 2013-2026 Ramon Santamaria
(@raysan5). The exact raylib 6.0 notice is reproduced in
`LICENSES/Zlib-raylib.txt`.

Source builds use a separately installed raylib; its sources are not vendored
in this repository. macOS release artifacts statically link raylib 6.0, so the
raylib code and its compiled-in `src/external` components are part of the
executable and are not covered by the project's PolyForm terms. Their
version-locked notices are reproduced in
`LICENSES/Raylib-6.0-dependencies.txt`; the Apache 2.0 text referenced by the
GLAD/Khronos notice is in `LICENSES/Apache-2.0.txt`.

## Quaternius QA model

`resources/models/tank_basic.glb` comes from **Animated Tank Pack** by
Quaternius and is released under CC0 1.0 Universal. It is included only as the
default `--gltf-tank-qa` importer probe and is not used for normal vehicles.

Source: <https://poly.pizza/bundle/Animated-Tank-Pack-0tfvbeAJkU>

The CC0 legal code is reproduced in `LICENSES/CC0-1.0.txt`. Attribution is
provided as a courtesy and is not a CC0 condition.

## Generated textures

`resources/textures/battlefield_grass.png` and
`resources/textures/urban_masonry.png` were generated specifically for this
project with OpenAI image generation. No downloaded image was supplied as an
input. See `ASSET_LICENSES.md` for their project-license treatment.

## Optional AI development tools

The `training/requirements.txt` environment is installed under `build/ai-venv`.
It is not bundled with Tanks3D, and no upstream pretrained weights are imported.
The packages and all bundled portions retain their upstream terms:

- Gymnasium — Farama Foundation, <https://github.com/Farama-Foundation/Gymnasium>,
  MIT license.
- Stable-Baselines3 — its contributors,
  <https://github.com/DLR-RM/stable-baselines3>, MIT license.
- PyTorch — PyTorch contributors, <https://github.com/pytorch/pytorch>.
  The verified wheel declares Apache-2.0, Apache-2.0 WITH LLVM-exception,
  BSD-2-Clause, BSD-3-Clause, BSL-1.0 and MIT portions; retain its notices.
- NumPy — NumPy developers, <https://github.com/numpy/numpy>.
  The verified wheel declares BSD-3-Clause, 0BSD, MIT, zlib and CC0-1.0 portions.
- imageio-ffmpeg — ImageIO contributors,
  <https://github.com/imageio/imageio-ffmpeg>, BSD-2-Clause wrapper. Its FFmpeg
  executable has separate upstream licensing, reported by the executable's
  `-L` command. It is used only to encode local QA recordings and is not
  redistributed in the game.

Exact installed metadata, versions and the FFmpeg license report are retained
with local AI evidence. These terms are independent of the project's PolyForm
license for its original code and generated training artifacts.
