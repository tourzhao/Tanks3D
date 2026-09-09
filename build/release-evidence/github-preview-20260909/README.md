# GitHub homepage previews

Fresh native exports from the unchanged Alpha 5 candidate, captured 2026-09-09.
The homepage images now use this set; earlier development review images remain
available as historical comparisons.

- Source: `cc5f2a7eec073accf37c2085c10bf155061acef2`
- Tag: `v0.1.0-alpha.5`
- ZIP SHA-256: `d82b1ca75e6b9503fdfbe84baf32f2f7a6e3ea7a042d8a7a438cb7be501e59d8`
- All four PNGs are 1280x720, exported without cropping, compositing or edits.

| Image | Capture context |
| --- | --- |
| [Forest and battlefield](forest-solo.png) | Original stage 26, camera -25° / 50°, Pixel Style OFF; built-in forest-cover showcase stages the player near the trees. |
| [Two-player battlefield](battlefield-coop.png) | Original stage 26, camera -25° / 50°, Pixel Style OFF; players at their normal spawn positions. |
| [National enemy vehicles](national-enemies.png) | German player with four American/Soviet enemy roles in the built-in tank showcase arena. The arena and positions are staged. |
| [Battle report](battle-report.png) | Built-in two-player settlement showcase at frame 600, after the count-up animation. |

The existing extracted candidate's binary digest and signature were verified
before export. [manifest.json](manifest.json) records the candidate identity,
exact native launch arguments and each image's SHA-256. [capture.py](capture.py)
retains the commands; the two-player image was captured separately with the
identical recorded command before the helper was written. It intentionally
refuses to overwrite the other images.

All four images were viewed after export. HUD text and the expected models are
present. These previews do not establish physical-controller testing or human
release approval. The [Alpha 5 status](../../../docs/releases/v0.1.0-alpha.5-status.json)
continues to record the outstanding formal acceptance work.
