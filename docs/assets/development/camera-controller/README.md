# Adjustable camera development screenshots

These images were captured from the source worktree on 2026-09-08 using its
rebuilt macOS executable. They illustrate the pull request and do not attest a
tagged release candidate, hardware-controller behavior, or completed human QA.
They are documentation images; the game does not load them as runtime assets.

| Image | View |
| --- | --- |
| [Solo](solo-left-low.png) | One-player tank showcase, horizontal -45°, elevation 40° |
| [Co-op](coop-right-high.png) | Two-player stage 1, horizontal +45°, elevation 70° |

After `make all`, reproduce them with new output paths (the exporter refuses
to replace existing files):

```sh
./build/Tanks3D --camera-yaw=-45 --camera-elevation=40 --quick-start \
  --tank-showcase --release-screenshot=/absolute/new/solo.png \
  --release-screenshot-frame=120
./build/Tanks3D --camera-yaw=45 --camera-elevation=70 --quick-start-2p \
  --release-screenshot=/absolute/new/coop.png --release-screenshot-frame=360
```

The screenshot exporter fixes the viewport at 1280×720. Narrow-window HUD and
short-window menu checks require the normal resizable application; these two
images do not substitute for those checks. The recorded source-build visual
checks also inspected an 800-wide portrait co-op window and 960×540 settings.
