# Opposing national enemy vehicles

Staged native renders for the national enemy selection change. The player
nation is shown in the HUD; the eight enemies use all four roles from both
opposing nations. Columns are basic, fast, power, and armor. The upper row is
the first unselected nation in USA/USSR/Germany order, and the lower row is the
second. These are inspection scenes, not naturally generated formations.

- [USA player: USSR and Germany](opponents-for-USA.png)
- [USSR player: USA and Germany](opponents-for-USSR.png)
- [Germany player: USA and USSR](opponents-for-GERMANY.png)
- [National muzzle-flash attachment check](opponents-for-GERMANY-firing.png)

`make clean` followed by `make -j4 test` passed 244 suites / 15,620 checks.
`make -j4 test-sanitize` passed 19 suites / 13,978 integrated checks.
Native scripted review covered solo/co-op, movement/fire, pause, pickups,
settlement, next stage, menu and all national enemy roles. A temporary firing
fixture initially failed because the armored enemies' default target was behind
them; setting a target ahead satisfied the existing aim gate. Production firing
rules were unchanged. See [review.json](review.json) for source and image hashes.
