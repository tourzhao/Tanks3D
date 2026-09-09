# Original Battle City maps

`src/game/classic_stage_layouts.h` contains the 35 fixed **Battle City (J),
NES/Famicom** layouts in their displayed stage order. `StageGenerator` copies
these 26×26 grids without reshuffling materials, rotating maps, clearing spawn
areas, carving routes or removing terrain for the headquarters. Half walls,
water edges, forest/ice at traversable spawns and the eight base-guard bricks
remain in the data. This replaces the former procedural layouts after stage 1.

## Sources and scope

- The initial text transcription comes from
  [krystiankaluzny/Tanks, commit `f59aea3`](https://github.com/krystiankaluzny/Tanks/tree/f59aea31638117e20bc03276026bdbb9f8828b47/resources/stages),
  files `1` through `35`. Its MIT notice remains in
  [MIT-upstream.txt](../LICENSES/MIT-upstream.txt).
- Display order and disputed cells were checked against all 35
  [VGMaps Battle City (J) maps](https://www.vgmaps.com/Atlas/NES/index.htm#BattleCity),
  captured by **Ricardo Sallin**. The index credits the original game to
  **Namco Ltd.** Individual image URLs follow the
  [Stage 1 reference](https://www.vgmaps.com/atlas/NES/BattleCity(J)-Stage1.png)
  pattern through `Stage35.png`.
- An independent cross-check used the decoded 13×13 metatile tables in
  [vgrichina/battlecity `web/levels.js`](https://github.com/vgrichina/battlecity/blob/main/web/levels.js).
  The reviewed blob is `e9c1197a3fb0100e893f3988144c0b2a827d1279`, SHA-256
  `83a70acd4efb6e14124537de9644fe67855d649ebd08a065cad5b36dde84b227`.
  It is a decoded table, not a reliable displayed-stage index by itself.

The target is the **initial terrain visible in those original-game stage
captures**, rather than unprocessed ROM-table entries. Original map designs
remain attributable to their original rights holders; the project does not
claim that the entire map set is original project art or PolyForm-only.
See [asset licenses](../ASSET_LICENSES.md) and
[third-party notices](../THIRD_PARTY_NOTICES.md). No ROM or executable was
obtained for this import. Reference PNGs are review material under `build/`
and are not distributed as game assets or committed with this change.

## Cell verification

Each reference image is 256×240 pixels. The battlefield occupies
`x = 16..223`, `y = 16..223`: 208×208 pixels, or 26×26 cells of 8×8 pixels.
Rows and columns below are zero-based. Each 16×16 metatile expands into four
cells, preserving its upper/lower/left/right half walls.

| Symbol | Terrain |
| --- | --- |
| `.` | Empty ground |
| `#` | Brick |
| `@` | Steel |
| `%` | Forest |
| `~` | Water |
| `-` | Ice |

Across 23,660 cells, the captures contain 37 distinct 8×8 RGB patterns. The
pattern atlas was visually classified: seven background/terrain patterns
(including two water frames) cover 23,238 cells; 30 actor, creation-star and
eagle patterns cover the remaining 422. Actor/eagle pixels are not terrain.
Those overlays use the clear ground visible around them, cross-checked against
the text sources. The spawning exceptions in stages 5, 12, 15 and 32 are listed
below; visibly retained forest or ice elsewhere is not cleared. In particular,
stage 16's second-player spawn is forest, and stage 32's outer enemy spawns
are ice. Neither is changed to empty ground.

The final embedded table was compared cell-for-cell against this classification
for all 35 images. There are no unclassified patterns or silent route repairs.
This verifies the selected reference captures; it is not an assertion that
every regional game version or every raw table byte has identical semantics.

## Display order and independent-table differences

Displayed stages 1–14 correspond to these **one-based** indices in the reviewed
`levels.js` array:

```text
Display stage:  1  2  3  4  5  6  7  8  9 10 11 12 13 14
Table index:    1  3  2  4  5  8  6 13 14  7 10  9 12 11
```

Stages 15–35 use the same index. This permutation comes from full-grid matching
against the numbered captures, not an assumed array order. After expanding the
metatiles and adding the original eight base-guard bricks, 29 maps match
exactly. The other six differ in 38 cells; the captures decide those cells:

| Displayed stage | Cells | Difference from decoded table; final choice |
| --- | ---: | --- |
| 5 | 12 | Center enemy spawn `(0..1,12..13)` is empty, not brick. At rows 6–7, the right-edge water is at columns 24–25, not 22–23. |
| 10 | 6 | Columns 10 and 15 at rows 23–25 are empty, outside the eight-brick guard. |
| 12 | 4 | Center enemy spawn `(0..1,12..13)` is empty, not brick. |
| 15 | 4 | First-player spawn `(24..25,8..9)` is clear in the capture, not forest. |
| 21 | 8 | Row 22, columns 10–15 and row 23, columns 10/15 are empty around the guard. |
| 32 | 4 | Center enemy spawn `(0..1,12..13)` is empty, not ice. |

The final order agrees with the MIT upstream file numbering. Its transcription
needed **44 cell corrections in seven stages**; the other 28 are unchanged:

| Stage | Corrected cells | Corrections from upstream to verified capture |
| --- | ---: | --- |
| 2 | 12 | Add brick at row 4, columns 12–13 and row 21, columns 10–15; add steel at rows 18–19, columns 20–21. |
| 3 | 4 | Add brick at row 20, columns 14–17. |
| 13 | 4 | Rows 6–7, columns 2–3 are steel instead of brick. |
| 15 | 11 | Row 17 columns 10–11 become steel; row 18 columns 9–11 become brick; row 21 columns 10–11 become empty; row 22 columns 8–9 become forest; row 24 columns 8–9 become empty. |
| 17 | 8 | Clear `(1,7)`; add brick at rows 1–3, column 9; rows 14–15 column 17 are ice and column 18 is steel. |
| 32 | 4 | Row 9, columns 20–23 are empty instead of ice. |
| 34 | 1 | Add brick at `(19,18)`. |

## Stable digests

The map SHA-256 uses exactly 26 ASCII rows, each followed by LF, including a
final LF (702 bytes). The FNV-1a value is the repository's versioned layout
signature: bytes `1, 26`, followed in row-major order by the ASCII tile and its
initial brick mask (`0x0f` for `#`, zero otherwise). No stage number is mixed
into either map digest. The reference-image SHA-256 identifies the reviewed
PNG bytes independently of the text maps.

Stage 1's layout signature is now `5c2ce11a49ccd52a`. The former
`16f7273f7f1f9e7a` described the project-modified layout after removing the
original base bricks; restoring them intentionally changes that signature.

| Stage | FNV-1a layout | Map SHA-256 | Reference PNG SHA-256 |
| --- | --- | --- | --- |
| 1 | `5c2ce11a49ccd52a` | `9ef2de1c7561b625cec893ccb7eefac51bf56ce4622e3836303d8739d28f5e6b` | `87ebd8b786d1a3f203ddb59a8910bdfed36efd0c6b67075e129d48cb8434c95a` |
| 2 | `0dcbe3883e982622` | `5e06b5e024a609e12b598680741bbee15c683a35aef9ce3a1bb88fd45235397a` | `24df29291abb8bb249ee1da6d91fcdeab0fc53e10be056ccab727d5c673347b8` |
| 3 | `01180e57eab577fa` | `f72f31e14b5ea5865fd20e476e424b5be3458f4f468e36768ecd95b3507cfa6b` | `116a36ce9a70da749949eccc646c1510c4b5d430fab337f060a6cf4d2e8ef0e9` |
| 4 | `2378c573fad5522a` | `ed163e52bb37be73e9a246e5e3a37fd614b3c89fc1e1133ff2e113803b1a7eff` | `5af35aa4bc124da9c663b998e0dadd313e05d9b992ad6454d4676ef2409674de` |
| 5 | `c20c8100afd013d6` | `f835f29395f34a1f5f026e8af636123732fe436b742d5993b09becbb8e0493f7` | `51f380847ddf02de6b47edf56d4f2e3a31d3125153d474b3de89eeb19e19e753` |
| 6 | `5be7f031849aa806` | `9282d77c8d50e9e65b798231afd4672523a47053ab4521c64f01bb0bed30ea77` | `cb088587f9f5265abf095c0235a986ce4470fb9d04a6ceacf8a1eebd124b71d6` |
| 7 | `659a20e3ba472ec2` | `7ed125c6613b7de245f3c9c9cd9fec75ec6d35946457937c74d884fe5b4398ae` | `1c4aa48f4bab7f390d901db2c177e90d232b76713989553367fae61195b318bd` |
| 8 | `7ebe201e58e5bcee` | `830d4567e665c6ec00a9234ccc2924701a1cc86dd69a25330036e7c2bc30aeb0` | `07cb906adffb056e9b61b1028fd5f46c5d3564332b66b12342478b0c3ac45616` |
| 9 | `fcce2712ed06d1a2` | `6d3099820585e6f486b99aab8efdd8c157f944eed835af8cb4490448f2e4f27f` | `98a761b2f9d9a555a8a76be15a17d6298cb7c400e908a401d81a553f1701200f` |
| 10 | `ff0d3262c9a30822` | `a603c9c9b2537cade0273140911233f0c3440f388a3c0d93c45bcb5737491707` | `257972bdf2bf7c226af7797a973afdc01acc0a405728a4a8d2922b6f6c518a74` |
| 11 | `36628f0b3f81dde6` | `5ff4af6bcc0eb1cde9e6afad320f4a890631fa75aad608e8a0cfe938cee72609` | `075ac6ce7586513ec6005199b2766651a35e5ccef45587967cd583d555b595f2` |
| 12 | `a8ad2363fd49bf2e` | `5df323de9be6c78ac4f1d8866624bde5446100fc5a2bf63b8c41781e1d70acb4` | `2d64852ff727ff2a2e79e1ec9f8f72e0d9a510fef2a76b225c6e498a153767b3` |
| 13 | `82dd400a270ed8b2` | `93bcaab82501f1dc96c8f153cb38aaedd1c896993174ff7fc82e47350033df0e` | `34c0010162de8f1d9c9057572bb537ac1ed088c93a6bcc3d6a71d3b5c2c1bc0f` |
| 14 | `b62a7e8d792fbdea` | `e0297ad2a64920f87d6af39459b32f507a36ebc39f8fcaca74513168db85bca9` | `893854046f015def419155cbd984c590d6e68d43a501c8e28abc3d479e1fa4ec` |
| 15 | `6a24e40977e816d2` | `27d97fb8b471f6bf2813c45469e56dd10877af5a87d394b665a71ecd180859f7` | `d186d7a7784e1d99b451c3c7e12821999629d2e681b6ed9f7fdc944dc453fe3b` |
| 16 | `a7c00587eae9371e` | `f22f03230e1af80310872c82a2a18ad1567527eedb114da84b6d20c85ae00dd3` | `99b3843e504cd4ff035698f497022e580262441b8b14d3da9b048b949871d2a6` |
| 17 | `b16fe5885a25c4c6` | `3a14bceaaa9407f61e79484b2efec7f88c7da47abd5391a6bd3c8f2316f273b4` | `29ff90b1c5f6e4873788d1bf248338829f21f5756a92ba3d3bde453d47a4cb06` |
| 18 | `9ae3cd9bcf80378a` | `fb07e3a931b892d3b9db2e3f26f7b7b4401b607207b124f9336225576313f3e9` | `7cb542d11032c935eeac735eed43b10613bb347893bb17d506d70baeae7b3c7a` |
| 19 | `d2e9791d1a1f3a5a` | `cc45bd713e5a97de2e05961f4ea836f975972fc1910cf7d1db3632394979886b` | `006cb8a899cafec0557a358aeb19e75054b1acb98d4e39ed7c2279a1a941252a` |
| 20 | `ac897e98f50952fa` | `c2b5ae3ec5ba0298ba430aa5e5379c9edffc50357e37a07d8772054f13b135ee` | `78418712df0b8f998189eb926955cb65473f2b7414f847729ffd02c7b0bd2007` |
| 21 | `67553e4bb9056f6a` | `8a71bbb150051dcce41585a3d452bcb881123f9027d8f12eee0b682c84f132a6` | `f876d007f822cceb74d0df79820a275c714a06a98e5a28a845ccda7069d20d28` |
| 22 | `250f61a43f836c1a` | `d9572e7b2e782b81f6705ed52b158e89f39582cc4203de736a551fdefd30db2a` | `22f7cc9a4b4e39347b0a971371c08a7470d94edbd5727f81dd94835d4fc9f009` |
| 23 | `3d6ae8c6d12056e2` | `e22a40a9469641266cc7bb79e53c2376b49d43c55e9db934bb9a978690b0d82f` | `a3853104d069749ad35c3d2c7d77e93aa25f2cd5a4ffdcf4167751fbd7d666d7` |
| 24 | `efbc6e40d588c7f6` | `0790dfcfe1678459c4a05c5ebd43909cc2bd4d2c4b32e1593babe5282ecd29e6` | `72fae3110893f0db7694d60bad6cd37804b3aa2b622df82ece3f9435c610ceaa` |
| 25 | `f59e7afaa6ec416a` | `5236dc8bc45cc9719ff4d2b40cbaf658d925292d1cffd4a4a5aaa7a0b24e6874` | `0830eb7bb7d3c96bb1e0bc29ce13ea3147b5dd0e13c1851ff67d55bf37647e6d` |
| 26 | `17c48a7ff6ea378a` | `60a6eb01db231f1bc50ae91cdbef8d7c7f2064d93c759fc3a4f1b7b9e6d8dd85` | `ee30f237fd322e57c393ae7a17db87b8af4ef68f8deb1ccfb0457a61aa0d758a` |
| 27 | `2d0afecb03d9c22a` | `4c978caf087a8a61b1170e85cf5624e089d70060a896e5b12ec2773ec20c13e4` | `db5f766982ab9f3d745f66141e0d297d8776dfba3a7def373aeda8b643ae1995` |
| 28 | `9d31ac8a62dea216` | `928eb4aeddfe7959c3ae280e55959a92d1f487ee1750804c1506a99d537ab3da` | `fe6e50ab5a7abcfaf9d90c025784f6f09507513a340708260dbefb636859bd0c` |
| 29 | `aee07e491352e0a2` | `975b0b8387e2e6c58f7f92fc9458e3f2a6a8e9e469f5debcfb29fbf67d330f38` | `684d56cd2987f1934e03f2f324639bba341a0f7848efda43fbbd5084a6e67b6c` |
| 30 | `0dc6ee424385d54a` | `66b9f1bc8c2a6ead2b213936d5d8eb8ac20a22f815f446531b78522758633962` | `4d13fbcb203fa06cc5d840c5f895ffc583751a3a48831c2c2391f46b7688bf88` |
| 31 | `97a0c90325a5c17a` | `60427a60ae3af9c3a7f32f72e40eb35f659650335e90da54ffa7634f476a2b4a` | `391ef87d2adb1e1af4d0f03201f7913c44039b01c19bc508128cc91f6ab314b9` |
| 32 | `3f81b94785a7d58e` | `1abf6474c5446d6dc2cc7b664e8dc9050be0d6a877305e5ecd401be6f524152b` | `6636ab0010cb6b3f4559e4e690f506a70f622ea1ea9d57efed807cfc36228f0f` |
| 33 | `45fca3d88892eda2` | `588e35216f0284189c73260a3b65c8eb697391ec81ea34ecaa34f5f8b88d503b` | `82ab82d4127d1f89e0fd6213086423375f8162808d01b1a5cba8cbf2867fbd94` |
| 34 | `c18619816ee7edfe` | `319f5b0cfc8b2be1e87fd21c5947d01a013e6f84ca38cc5f7a243d1eb50106cd` | `e706efbd65f854212a7833f583bc7defefb9bf135dbe1e97b334610e7ac78935` |
| 35 | `46cf515593993312` | `a52b1d05ae7bbd19b5c0852600f675b33c311f912e6997f44a800dd74e22102f` | `dd0e9f2cb3aa3206922c94fecfd52c39cf3e9542627cffffc6e64e29d9a4764c` |

The source audit and intermediate comparisons are retained locally in
`build/battle-city-source-20260908/`. Automated map tests verify the checked-in
layout signatures, original base geometry, collision-safe spawn positions and
stage progression. Route validation must account for destructible brick;
requiring every original map to be traversable before firing would force an
unfaithful layout change.
