#!/bin/sh

# Exercise production recipes in a spaced path without compiling or launching
# the game. The executable fixture records all four capability invocations;
# the bundle fixture retains the resource and plist assertions.
set -eu
LC_ALL=C
export LC_ALL

project_root=${1:-.}
project_root=$(CDPATH= cd "$project_root" && pwd -P)
test_root=$(mktemp -d "${TMPDIR:-/tmp}/tanks3d-make-paths.XXXXXX")
test_root=$(CDPATH= cd "$test_root" && pwd -P)
trap 'rm -rf "$test_root"' 0 1 2 15
fixture_root="$test_root/Project With Spaces"
app="$fixture_root/build/Tanks3D.app"
mkdir -p "$fixture_root/tests" "$fixture_root/bin" "$app/Contents/MacOS"
cp "$project_root/Makefile" "$fixture_root/Makefile"
cp "$project_root/tests/expected_release_performance_capabilities.json" \
    "$project_root/tests/expected_bundle_resources.txt" "$fixture_root/tests/"

cat > "$fixture_root/build/Tanks3D" <<'EOF'
#!/bin/sh
set -eu
printf '%s\n' "$*" >> "${0%/*}/probe-arguments.txt"
[ "$#" -eq 1 ] && [ "$1" = --self-test=release-performance-capabilities ] || exit 64
cat "${0%/*}/../tests/expected_release_performance_capabilities.json"
EOF
chmod +x "$fixture_root/build/Tanks3D"
cp "$fixture_root/build/Tanks3D" "$app/Contents/MacOS/Tanks3D"
cp "$project_root/macos/Info.plist" "$app/Contents/Info.plist"
/usr/libexec/PlistBuddy -c 'Set :LSMinimumSystemVersion 13.0' "$app/Contents/Info.plist"

while IFS= read -r resource; do
    destination="$app/Contents/Resources/${resource#./}"
    mkdir -p "${destination%/*}"
    : > "$destination"
done < "$fixture_root/tests/expected_bundle_resources.txt"

# This checks argument boundaries rather than substituting for the real
# codesign validation, which remains in test-bundle on the production app.
cat > "$fixture_root/bin/codesign" <<'EOF'
#!/bin/sh
set -eu
[ "$#" -eq 5 ]
[ "$1" = --verify ] && [ "$2" = --deep ] && [ "$3" = --strict ]
[ "$4" = --verbose=4 ]
app_root=$(CDPATH= cd "$5" && pwd -P)
[ -f "$app_root/Contents/Info.plist" ]
printf '%s\n' "$app_root" > "$0.log"
EOF
chmod +x "$fixture_root/bin/codesign"

(
    PATH="$fixture_root/bin:$PATH"
    export PATH
    make -s -C "$fixture_root" -o build/Tanks3D \
        -o build/Tanks3D.app/Contents/MacOS/Tanks3D \
        test-release-performance-capabilities test-bundle \
        RAYLIB_PREFIX=/nonexistent/tanks3d-path-test-raylib \
        MACOS_MIN=13.0 APP_VERSION=0.1.0 DIST_ARCH=arm64
)

cat > "$test_root/expected-probe-arguments.txt" <<'EOF'
--self-test=release-performance-capabilities
--self-test=release-performance-capabilities --quick-start
--quick-start --self-test=release-performance-capabilities
--self-test=release-performance-capabilities --self-test=release-performance-capabilities
EOF
cmp "$test_root/expected-probe-arguments.txt" "$fixture_root/build/probe-arguments.txt"
[ "$(cat "$fixture_root/bin/codesign.log")" = "$app" ]
printf '%s\n' 'Make path-space checks passed: capability output, all three rejection probes and bundle checks.'
