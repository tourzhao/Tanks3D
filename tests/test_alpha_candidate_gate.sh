#!/bin/sh

set -eu
LC_ALL=C
export LC_ALL

if [ "$#" -gt 1 ]; then
    echo "usage: $0 [PROJECT_ROOT]" >&2
    exit 2
fi

production_root=${1:-.}
production_root=$(CDPATH= cd "$production_root" 2>/dev/null && pwd -P) || {
    echo "Alpha candidate gate test failed: project root is not accessible" >&2
    exit 1
}
production_builder="$production_root/scripts/build_alpha_candidate.sh"
production_verifier="$production_root/scripts/verify_alpha_candidate.sh"

fail()
{
    echo "Alpha candidate gate test failed: $1" >&2
    exit 1
}

[ -f "$production_builder" ] || fail "candidate builder is missing"
[ -f "$production_verifier" ] || fail "candidate verifier is missing"
command -v git >/dev/null 2>&1 || fail "git is required"
command -v zip >/dev/null 2>&1 || fail "zip is required"
command -v shasum >/dev/null 2>&1 || fail "shasum is required"

test_root=$(mktemp -d "${TMPDIR:-/tmp}/tanks3d-alpha-gate-test.XXXXXX") || \
    fail "temporary test directory cannot be created"
test_root=$(CDPATH= cd "$test_root" 2>/dev/null && pwd -P) || \
    fail "temporary test directory cannot be normalized"
trap 'rm -rf "$test_root"' 0 1 2 15
fake_bin="$test_root/bin"
mkdir -p "$fake_bin"

apply_fixture_files()
{
    fixture_dir=$1
    mkdir -p "$fixture_dir/scripts" "$fixture_dir/macos"
    cp "$production_builder" "$fixture_dir/scripts/build_alpha_candidate.sh"
    cp "$production_verifier" "$fixture_dir/scripts/verify_alpha_candidate.sh"
    cat > "$fixture_dir/scripts/verify_macos_dist.sh" <<'EOF'
#!/bin/sh

set -eu

[ "$#" -eq 7 ] || exit 64
if [ "${FAKE_DIST_VERIFIER_FAIL:-}" = 1 ]; then
    echo "controlled distribution-verifier failure" >&2
    exit 75
fi
[ -f "$2" ] && [ -f "$3" ] || exit 65
unzip -tq "$2" >/dev/null
EOF
    printf '%s\n' 'build/' > "$fixture_dir/.gitignore"
    printf '%s\n' 'fixture source' > "$fixture_dir/source.txt"
    printf '%s\n' 'fixture Makefile (the test supplies a controlled make)' > \
        "$fixture_dir/Makefile"
    cat > "$fixture_dir/macos/Info.plist" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleIdentifier</key>
    <string>io.github.tourzhao.tanks3d</string>
    <key>CFBundleShortVersionString</key>
    <string>0.1.0</string>
</dict>
</plist>
EOF
}

prepare_fixture()
{
    fixture_name=$1
    fixture_tag_mode=$2
    fixture_dir="$test_root/$fixture_name"
    apply_fixture_files "$fixture_dir"
    git -C "$fixture_dir" init -q 2>/dev/null
    git -C "$fixture_dir" config user.name "Alpha Gate Test" 2>/dev/null
    git -C "$fixture_dir" config user.email \
        "alpha-gate@example.invalid" 2>/dev/null
    git -C "$fixture_dir" add . 2>/dev/null
    git -C "$fixture_dir" commit -qm "Create Alpha fixture" 2>/dev/null
    case "$fixture_tag_mode" in
        correct) git -C "$fixture_dir" tag v0.1.0-alpha.1 2>/dev/null ;;
        wrong) git -C "$fixture_dir" tag v0.1.0-alpha.2 2>/dev/null ;;
        none) ;;
        *) fail "unknown fixture tag mode '$fixture_tag_mode'" ;;
    esac
}

cat > "$fake_bin/make" <<'EOF'
#!/bin/sh

set -eu

[ "${1:-}" = -C ] || {
    echo "controlled make expected -C" >&2
    exit 64
}
fixture_root=$2
target=$3
shift 3

case "$fixture_root" in
    "$ALPHA_FIXTURE_ROOT"/*) ;;
    *) echo "controlled make rejected an unsafe fixture path" >&2; exit 65 ;;
esac

if [ "${FAKE_FAIL_GATE:-}" = "$target" ]; then
    echo "controlled failure for $target" >&2
    exit 73
fi

if [ "${FAKE_MUTATE_HEAD_GATE:-}" = "$target" ]; then
    printf '%s\n' "mutation at $target" >> "$fixture_root/source.txt"
    git -C "$fixture_root" add source.txt
    git -C "$fixture_root" commit -qm "Mutate HEAD during gates"
fi

case "$target" in
    clean)
        rm -rf "$fixture_root/build"
        ;;
    test-alpha-candidate|debug|test-architecture|test|test-sanitize|coverage)
        printf 'controlled make completed %s\n' "$target"
        ;;
    test-dist)
        dist_channel=alpha.1
        for make_argument do
            case "$make_argument" in
                DIST_CHANNEL=*) dist_channel=${make_argument#DIST_CHANNEL=} ;;
            esac
        done
        dist_dir="$fixture_root/build/dist"
        payload_dir="$dist_dir/payload"
        artifact_basename="Tanks3D-0.1.0-$dist_channel-macos-arm64-macos26.0"
        artifact="$dist_dir/$artifact_basename.zip"
        mkdir -p "$payload_dir"
        printf '%s\n' 'arch=arm64' 'macos-min=26.0' \
            'compiler=controlled-test-compiler' > "$dist_dir/.build-config"
        printf '%s\n' 'controlled ZIP payload' > "$payload_dir/payload.txt"
        (
            cd "$payload_dir"
            zip -q "$artifact" payload.txt
        )
        rm -rf "$payload_dir"
        if [ "${FAKE_BAD_CHECKSUM:-}" = 1 ]; then
            printf '%064d  %s\n' 0 "$(basename "$artifact")" > \
                "$artifact.sha256"
        else
            (
                cd "$dist_dir"
                shasum -a 256 "$(basename "$artifact")" > \
                    "$(basename "$artifact").sha256"
            )
        fi
        ;;
    *)
        echo "controlled make received unexpected target: $target" >&2
        exit 66
        ;;
esac
EOF
chmod +x "$fake_bin/make"

assert_no_candidate()
{
    checked_fixture=$1
    [ ! -e "$checked_fixture/build/release/v0.1.0-alpha.1" ] || \
        fail "a rejected build left the final candidate directory"
    for staging_entry in \
            "$checked_fixture"/build/release/.v0.1.0-alpha.1.candidate.*; do
        [ ! -e "$staging_entry" ] || \
            fail "a rejected build left a staging directory"
    done
}

expect_build_rejection()
{
    rejection_name=$1
    expected_message=$2
    rejection_fixture=$3
    shift 3
    rejection_log="$test_root/$rejection_name.log"
    if env PATH="$fake_bin:$PATH" ALPHA_FIXTURE_ROOT="$test_root" "$@" \
            sh "$rejection_fixture/scripts/build_alpha_candidate.sh" \
            "$rejection_fixture" alpha.1 > "$rejection_log" 2>&1; then
        fail "$rejection_name build was accepted"
    fi
    grep -F "$expected_message" "$rejection_log" >/dev/null || {
        sed -n '1,180p' "$rejection_log" >&2
        fail "$rejection_name build failed for the wrong reason"
    }
    assert_no_candidate "$rejection_fixture"
    printf 'PASS build rejection: %s\n' "$rejection_name"
}

expect_verifier_rejection()
{
    rejection_name=$1
    expected_message=$2
    rejection_fixture=$3
    rejection_candidate=$4
    shift 4
    rejection_log="$test_root/verifier-$rejection_name.log"
    if "$@" sh "$rejection_fixture/scripts/verify_alpha_candidate.sh" \
            "$rejection_fixture" "$rejection_candidate" \
            > "$rejection_log" 2>&1; then
        fail "$rejection_name candidate was accepted"
    fi
    grep -F "$expected_message" "$rejection_log" >/dev/null || {
        sed -n '1,180p' "$rejection_log" >&2
        fail "$rejection_name candidate failed for the wrong reason"
    }
    printf 'PASS verifier rejection: %s\n' "$rejection_name"
}

unversioned_fixture="$test_root/unversioned"
apply_fixture_files "$unversioned_fixture"
expect_build_rejection unversioned "project root is not a Git worktree" \
    "$unversioned_fixture"

prepare_fixture missing-tag none
expect_build_rejection missing-tag "required release tag 'v0.1.0-alpha.1' does not exist" \
    "$fixture_dir"

prepare_fixture wrong-tag wrong
expect_build_rejection wrong-tag "required release tag 'v0.1.0-alpha.1' does not exist" \
    "$fixture_dir"

prepare_fixture dirty-tree correct
printf '%s\n' 'untracked release input' > "$fixture_dir/untracked.txt"
expect_build_rejection dirty-tree "Git worktree is not clean" "$fixture_dir"

prepare_fixture tracked-dirty-tree correct
printf '%s\n' 'modified tracked source' > "$fixture_dir/source.txt"
expect_build_rejection tracked-dirty-tree "Git worktree is not clean" \
    "$fixture_dir"

prepare_fixture stale-tag correct
printf '%s\n' 'committed after tag' >> "$fixture_dir/source.txt"
git -C "$fixture_dir" add source.txt 2>/dev/null
git -C "$fixture_dir" commit -qm "Move HEAD after release tag" 2>/dev/null
expect_build_rejection stale-tag \
    "release tag 'v0.1.0-alpha.1' does not identify HEAD" "$fixture_dir"

prepare_fixture concurrent-build correct
mkdir "$fixture_dir/.git/tanks3d-alpha-candidate-v0.1.0-alpha.1.lock"
expect_build_rejection concurrent-build \
    "another Alpha candidate build is active for 'v0.1.0-alpha.1'" \
    "$fixture_dir"

prepare_fixture failed-gate correct
expect_build_rejection failed-gate "gate 'test-sanitize' failed" "$fixture_dir" \
    FAKE_FAIL_GATE=test-sanitize

prepare_fixture failed-tooling-gate correct
failed_tooling_fixture=$fixture_dir
expect_build_rejection failed-tooling-gate \
    "gate 'test-alpha-candidate' failed" "$failed_tooling_fixture" \
    FAKE_FAIL_GATE=test-alpha-candidate
[ ! -e "$failed_tooling_fixture/.git/tanks3d-alpha-candidate-v0.1.0-alpha.1.lock" ] || \
    fail "a failed tooling gate left its candidate lock"

prepare_fixture changed-head correct
expect_build_rejection changed-head \
    "HEAD changed while candidate gates were running" "$fixture_dir" \
    FAKE_MUTATE_HEAD_GATE=coverage

prepare_fixture bad-checksum correct
expect_build_rejection bad-checksum "checksum digest does not match the ZIP" \
    "$fixture_dir" FAKE_BAD_CHECKSUM=1

prepare_fixture valid-candidate correct
valid_fixture=$fixture_dir
valid_log="$test_root/valid-candidate.log"
env PATH="$fake_bin:$PATH" ALPHA_FIXTURE_ROOT="$test_root" \
    sh "$valid_fixture/scripts/build_alpha_candidate.sh" \
    "$valid_fixture" alpha.1 > "$valid_log" 2>&1 || {
        sed -n '1,220p' "$valid_log" >&2
        fail "valid candidate build was rejected"
    }
valid_candidate="$valid_fixture/build/release/v0.1.0-alpha.1"
[ -d "$valid_candidate" ] || fail "valid candidate directory is missing"
sh "$valid_fixture/scripts/verify_alpha_candidate.sh" \
    "$valid_fixture" "$valid_candidate" >/dev/null || \
    fail "valid published candidate did not verify"
printf 'PASS candidate creation and verification\n'

artifact="$valid_candidate/Tanks3D-0.1.0-alpha.1-macos-arm64-macos26.0.zip"
artifact_backup="$test_root/valid-artifact.zip"
cp "$artifact" "$artifact_backup"
printf '%s\n' 'tamper' >> "$artifact"
expect_verifier_rejection artifact-tamper \
    "artifact digest does not match the attestation" \
    "$valid_fixture" "$valid_candidate"
cp "$artifact_backup" "$artifact"

attestation="$valid_candidate/attestation.txt"
attestation_backup="$test_root/valid-attestation.txt"
cp "$attestation" "$attestation_backup"
sed 's/^gate_test_alpha_candidate=PASS$/gate_test_alpha_candidate=FAIL/' \
    "$attestation" > \
    "$attestation.tmp"
mv "$attestation.tmp" "$attestation"
expect_verifier_rejection gate-attestation \
    "attestation gate 'gate_test_alpha_candidate' is not PASS" \
    "$valid_fixture" "$valid_candidate"
cp "$attestation_backup" "$attestation"

gate_log="$valid_candidate/alpha-candidate-gates.log"
gate_log_backup="$test_root/valid-gate-log.txt"
cp "$gate_log" "$gate_log_backup"
sed '/^GATE test-alpha-candidate PASS$/d' "$gate_log" > "$gate_log.tmp"
mv "$gate_log.tmp" "$gate_log"
modified_gate_log_sha256=$(shasum -a 256 "$gate_log" | awk '{print $1}')
sed "s/^gate_log_sha256=.*/gate_log_sha256=$modified_gate_log_sha256/" \
    "$attestation" > "$attestation.tmp"
mv "$attestation.tmp" "$attestation"
expect_verifier_rejection missing-gate-record \
    "gate log does not contain eight PASS records" \
    "$valid_fixture" "$valid_candidate"
cp "$gate_log_backup" "$gate_log"
cp "$attestation_backup" "$attestation"

printf '%s\n' 'unexpected candidate file' > "$valid_candidate/unexpected.txt"
expect_verifier_rejection extra-candidate-file \
    "candidate contains unexpected file 'unexpected.txt'" \
    "$valid_fixture" "$valid_candidate"
rm -f "$valid_candidate/unexpected.txt"

printf '%s\n' 'dirty after release' > "$valid_fixture/dirty-after-release.txt"
expect_verifier_rejection dirty-verification-tree \
    "current Git worktree is not clean" "$valid_fixture" "$valid_candidate"
rm -f "$valid_fixture/dirty-after-release.txt"

expect_verifier_rejection distribution-verifier \
    "macOS distribution verifier rejected the candidate artifact" \
    "$valid_fixture" "$valid_candidate" env FAKE_DIST_VERIFIER_FAIL=1

sh "$valid_fixture/scripts/verify_alpha_candidate.sh" \
    "$valid_fixture" "$valid_candidate" >/dev/null || \
    fail "restored candidate did not verify"

echo "Alpha candidate gate tests passed: 11 build rejections, 6 verifier rejections, 1 success."
