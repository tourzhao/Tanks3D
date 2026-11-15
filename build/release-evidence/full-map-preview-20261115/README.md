# Full Map development preview

Actual Mobile/Metal rendering from the November 15 development changes.
`overview.png` uses a 1280×800 window; `status-translucent.png` uses 1280×720
and shows the player cards at 50% opacity. Both use stage1, two human players,
yaw0° and elevation50°.

The source and image hashes are recorded in `capture-manifest.json`.
The rendering check exercised four window shapes, scene framing, radar hiding,
and scripted Tab press/repeat/release and paused toggles. It verifies rendering
and synthetic input, not physical controller use or release acceptance.
