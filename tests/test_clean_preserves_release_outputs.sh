#!/bin/sh

set -eu
LC_ALL=C
export LC_ALL

if [ "$#" -gt 1 ]; then
    echo "usage: $0 [PROJECT_ROOT]" >&2
    exit 2
fi

project_root=${1:-.}
project_root=$(CDPATH= cd "$project_root" 2>/dev/null && pwd -P) || {
    echo "Clean preservation test failed: project root is not accessible" >&2
    exit 1
}
[ -f "$project_root/Makefile" ] || {
    echo "Clean preservation test failed: Makefile is missing" >&2
    exit 1
}

fail()
{
    echo "Clean preservation test failed: $1" >&2
    exit 1
}

test_root=$(mktemp -d "${TMPDIR:-/tmp}/tanks3d-clean-test.XXXXXX") || \
    fail "temporary directory cannot be created"
trap 'rm -rf "$test_root"' 0 1 2 15
cp "$project_root/Makefile" "$test_root/Makefile"

candidate="$test_root/build/release/v0.1.0-alpha.previous/attestation.txt"
evidence="$test_root/build/release-evidence/v0.1.0-alpha.previous/session.json"
mkdir -p "${candidate%/*}" "${evidence%/*}"
printf '%s\n' 'immutable candidate sentinel' > "$candidate"
printf '%s\n' 'candidate-bound evidence sentinel' > "$evidence"
candidate_before=$(shasum -a 256 "$candidate")
evidence_before=$(shasum -a 256 "$evidence")

for disposable in \
        Tanks3D Tanks3D.app/Contents/MacOS/Tanks3D obj/main.o \
        debug/Tanks3D-debug sanitize/Tanks3D-sanitize \
        coverage/Tanks3D-coverage dist/archive.zip tests/rule-test \
        release-screenshot-smoke/shot.png \
        release-performance-smoke/performance.json \
        verifier-path-escape.interrupted/sentinel; do
    disposable_parent=${disposable%/*}
    if [ "$disposable_parent" != "$disposable" ]; then
        mkdir -p "$test_root/build/$disposable_parent"
    else
        mkdir -p "$test_root/build"
    fi
    printf '%s\n' disposable > "$test_root/build/$disposable"
done

make -s -C "$test_root" clean APP_VERSION=0.1.0 DIST_ARCH=arm64 \
    DIST_MACOS_MIN=26.0 MACOS_MIN=26.0 \
    RAYLIB_PREFIX=/nonexistent/tanks3d-clean-test-raylib || \
    fail "make clean failed"

[ "$(shasum -a 256 "$candidate")" = "$candidate_before" ] || \
    fail "make clean changed a prior candidate"
[ "$(shasum -a 256 "$evidence")" = "$evidence_before" ] || \
    fail "make clean changed prior candidate-bound evidence"

for disposable in \
        Tanks3D Tanks3D.app obj debug sanitize coverage dist tests \
        release-screenshot-smoke release-performance-smoke \
        verifier-path-escape.interrupted; do
    [ ! -e "$test_root/build/$disposable" ] || \
        fail "make clean retained disposable output '$disposable'"
done

printf '%s\n' \
    'Clean preservation test passed: candidates and QA evidence survived.'
