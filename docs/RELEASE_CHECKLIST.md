# macOS Alpha Release Checklist

The first public target is a non-commercial, Apple Silicon GitHub Alpha. A
source checkout may still use the Homebrew-linked development app; files shared
with players must come from a verified `make alpha-candidate` directory. A
`make dist` ZIP is useful during development but is not a publishable candidate.

## Automated package gate

Commit the intended source with a clean worktree and create the exact tag
`v<VERSION>-<CHANNEL>` (for example, `v0.1.0-alpha.1`) at `HEAD`. Then run:

```sh
make test-alpha-candidate
make alpha-candidate DIST_CHANNEL=alpha.1
```

The candidate command starts from `make clean`, runs `debug`, architecture,
the Alpha builder/verifier negative-contract fixtures, test, sanitizer,
coverage, and `test-dist` gates, and rechecks the clean worktree, `HEAD`, tag,
bundle version, and channel before publishing anything. The standalone
`test-alpha-candidate` command is a quick preflight; the candidate builder runs
and attests it again, so direct script invocation cannot bypass that gate.
It writes atomically to `build/release/<tag>/`; a failed run leaves no candidate
for that tag. Recheck an existing directory with:

```sh
make verify-alpha-candidate \
    ALPHA_CANDIDATE_DIR=build/release/v0.1.0-alpha.1
```

The directory contains the ZIP, `.sha256`, build configuration, gate log, and
`attestation.txt`. The v2 attestation binds all of them to one clean commit and
records all eight required gates as `PASS`. Verification also reruns the full
macOS distribution verifier against the candidate ZIP. Publish this exact file
set together.

`test-dist` statically links the installed raylib and rejects an architecture or
deployment-target mismatch. It verifies the ZIP checksum and structure,
exact resource and license manifest, bundle metadata, system-only dynamic
dependencies, ad-hoc signature integrity, absence of quarantine/DLP metadata or
local paths, and all integrated self-tests after extraction. Eleven isolated
negative fixtures cover checksum-name/content mismatches, a checksum symlink,
input and resolved newline-bearing paths, lexical and symlink-parent path
escapes, an archive symlink, an extra top-level payload, an archived symbolic
link, and a checksum-updated mutation of a signature-sealed runtime resource.
The last case proves accidental-tamper detection by the ad-hoc resource seal; it
does not authenticate the publisher or replace Developer ID signing.

The release object cache carries a configuration fingerprint. Changing the
compiler identity, raylib prefix or archive contents, architecture, deployment
target, or distribution flags automatically invalidates cached objects. A clean
build is still required for the final candidate because it exercises the same
path used by CI from first principles.

The ZIP name is authoritative: `macos-arm64-macos26.0` means Apple Silicon and
macOS 26.0 or newer. Do not rename an artifact to claim broader compatibility.
To target an older release, first provide a raylib build compiled for that same
deployment target and rebuild through `RAYLIB_PREFIX`.

## Human release gate

Copy and complete the [Alpha QA report template](ALPHA_QA_REPORT_TEMPLATE.md),
then prepare the public notes from the
[release notes template](RELEASE_NOTES_TEMPLATE.md). Blank or `NOT RUN` fields,
missing evidence, and an absent source commit or release tag are blockers, not
passing results.

Capture publication images from the executable inside the extracted candidate,
not from a development build. For example:

```sh
Tanks3D.app/Contents/MacOS/Tanks3D \
    --quick-start \
    --release-screenshot=/absolute/path/one-player.png \
    --release-screenshot-frame=240
```

Frame 240 is the default and normally clears the 3.2-second stage introduction
at 60 Hz. Capture mode uses a fixed gameplay seed, locks the logical canvas,
ignores the borderless toggle, normalizes Retina output to 1280x720, refuses to
overwrite an existing path, saves one PNG, and exits. Use
`--quick-start-2p`, the national quick-start options, and the settlement
showcase to cover the required release views. Record the candidate ZIP hash,
exact command, Mac model, macOS version, capture time, and PNG hash in the QA
report. `make test-release-screenshot` is an explicit local GPU smoke test; it
opens a window and intentionally remains outside CI and candidate gates.

- Test the extracted ZIP on a clean Mac matching the declared minimum system.
- Complete one-player and two-player stages, including pause/Esc, every pickup,
  all three national bases, player death/respawn, base loss, stage settlement,
  fullscreen, resize, sound, and the final report.
- Run a longer session and record frame-rate, thermal, rendering, and audio
  problems as known issues.
- Review the archive's `licenses/` directory and decide whether to accept,
  confirm, or replace the inherited sound set described in
  `ASSET_LICENSES.md`.
- Add release screenshots, concise notes, known limitations, and all five files
  from the verified candidate directory to the GitHub release.
- Publish only an archive produced on `arm64`. The current CI job validates the
  native architecture of `macos-latest` but does not upload its generated ZIP;
  pin an Apple Silicon runner before turning CI output into release artifacts.

## Signing boundary

The current ad-hoc signature detects bundle modification and is checked by
`codesign --verify`. It does not provide a Team ID, trusted timestamp, Hardened
Runtime approval, notarization, or a warning-free Gatekeeper download. Do not
describe it as Apple-signed or notarized. Developer ID signing, notarization,
stapling, and a quarantined-download test remain required for a polished beta.
