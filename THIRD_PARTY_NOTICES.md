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

The sound set was authored and refined in the upstream project by Redas
Jefisovas (`holoflash`), beginning with the commit explicitly described as
[`add original effect sounds`](https://github.com/krystiankaluzny/Tanks/commit/326c2935).
The OGG conversion used by this repository was also contributed by `holoflash`
in
[`krystiankaluzny/Tanks` pull request 35](https://github.com/krystiankaluzny/Tanks/pull/35).
See `ASSET_LICENSES.md` for the audio provenance limitation.

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
