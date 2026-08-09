#!/bin/sh

set -eu
LC_ALL=C
export LC_ALL

if [ "$#" -gt 2 ]; then
    echo "usage: $0 [PROJECT_ROOT [DIST_CHANNEL]]" >&2
    exit 2
fi

project_root=${1:-.}
dist_channel=${2:-alpha.1}
required_raylib_version=6.0
required_performance_capability_schema=tanks3d-release-performance-capabilities-v1
required_performance_telemetry_schema=tanks3d-performance-log-v2
required_performance_contract_sha256=5137950da46fa11ee6d5ff60fafe67e83c4c0aacfb5fc83f2b0ce5f74afcfe1c

fail()
{
    echo "alpha candidate build failed: $1" >&2
    exit 1
}

require_safe_token()
{
    token_label=$1
    token_value=$2
    case "$token_value" in
        [A-Za-z0-9]*) ;;
        *) fail "$token_label must begin with an ASCII letter or digit" ;;
    esac
    case "$token_value" in
        *[!A-Za-z0-9._-]*) fail "$token_label contains an unsafe character" ;;
    esac
}

read_app_version()
{
    /usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' \
        "$project_root/macos/Info.plist" 2>/dev/null
}

read_config_value()
{
    config_key=$1
    awk -v wanted="$config_key" '
        index($0, wanted "=") == 1 {
            count++
            value = substr($0, length(wanted) + 2)
        }
        END {
            if (count != 1)
                exit 1
            print value
        }
    ' "$dist_config"
}

run_gate()
{
    gate_name=$1
    shift

    printf 'Running Alpha candidate gate: %s\n' "$gate_name"
    {
        printf 'GATE %s BEGIN\n' "$gate_name"
        printf 'GATE %s COMMAND' "$gate_name"
        for gate_argument do
            printf ' [%s]' "$gate_argument"
        done
        printf '\n'
    } >> "$gate_log"

    if "$@" >> "$gate_log" 2>&1; then
        printf 'GATE %s PASS\n' "$gate_name" >> "$gate_log"
    else
        gate_result=$?
        printf 'GATE %s FAIL exit=%s\n' "$gate_name" "$gate_result" >> "$gate_log"
        sed -n '1,260p' "$gate_log" >&2
        fail "gate '$gate_name' failed with exit code $gate_result"
    fi
}

command -v git >/dev/null 2>&1 || fail "git is required"
command -v make >/dev/null 2>&1 || fail "make is required"
command -v shasum >/dev/null 2>&1 || fail "shasum is required"
command -v unzip >/dev/null 2>&1 || fail "unzip is required"
[ -x /usr/libexec/PlistBuddy ] || fail "/usr/libexec/PlistBuddy is required"

project_root=$(CDPATH= cd "$project_root" 2>/dev/null && pwd -P) || \
    fail "project root is not accessible"
[ -f "$project_root/macos/Info.plist" ] || fail "macos/Info.plist is missing"

git_root=$(git -C "$project_root" rev-parse --show-toplevel 2>/dev/null) || \
    fail "project root is not a Git worktree"
git_root=$(CDPATH= cd "$git_root" 2>/dev/null && pwd -P) || \
    fail "Git worktree root is not accessible"
[ "$git_root" = "$project_root" ] || \
    fail "PROJECT_ROOT must be the Git worktree root"

require_safe_token "distribution channel" "$dist_channel"
app_version=$(read_app_version) || fail "bundle version cannot be read"
require_safe_token "bundle version" "$app_version"
release_tag="v$app_version-$dist_channel"
require_safe_token "release tag" "$release_tag"

source_status=$(git -C "$project_root" status --porcelain=v1 \
    --untracked-files=all) || fail "Git worktree status cannot be read"
[ -z "$source_status" ] || fail "Git worktree is not clean"

source_commit=$(git -C "$project_root" rev-parse --verify HEAD^{commit} \
    2>/dev/null) || fail "HEAD does not resolve to a commit"
tag_commit=$(git -C "$project_root" rev-parse --verify \
    "refs/tags/$release_tag^{commit}" 2>/dev/null) || \
    fail "required release tag '$release_tag' does not exist"
[ "$tag_commit" = "$source_commit" ] || \
    fail "release tag '$release_tag' does not identify HEAD"

release_root="$project_root/build/release"
candidate_dir="$release_root/$release_tag"
[ ! -e "$candidate_dir" ] && [ ! -L "$candidate_dir" ] || \
    fail "candidate already exists: build/release/$release_tag"

git_dir=$(git -C "$project_root" rev-parse --absolute-git-dir 2>/dev/null) || \
    fail "Git directory cannot be resolved"
git_dir=$(CDPATH= cd "$git_dir" 2>/dev/null && pwd -P) || \
    fail "Git directory is not accessible"
# Every channel shares build/dist, compiler objects, and the development app.
# Serialize candidate construction for the whole worktree, not just one tag.
candidate_lock="$git_dir/tanks3d-alpha-candidate.lock"

gate_log=$(mktemp "${TMPDIR:-/tmp}/tanks3d-alpha-gates.XXXXXX") || \
    fail "temporary gate log cannot be created"
stage_dir=
lock_owned=no

cleanup()
{
    cleanup_status=$?
    trap - 0
    if [ -n "$stage_dir" ] && [ -d "$stage_dir" ]; then
        rm -rf "$stage_dir"
    fi
    if [ "$lock_owned" = yes ]; then
        rmdir "$candidate_lock" 2>/dev/null || :
    fi
    rm -f "$gate_log"
    exit "$cleanup_status"
}
trap cleanup 0
trap 'exit 1' 1 2 15

if mkdir "$candidate_lock" 2>/dev/null; then
    lock_owned=yes
else
    fail "another Alpha candidate build is active in this worktree"
fi
[ ! -e "$candidate_dir" ] && [ ! -L "$candidate_dir" ] || \
    fail "candidate appeared before the gates started"

{
    printf 'ALPHA CANDIDATE GATE LOG\n'
    printf 'SOURCE COMMIT %s\n' "$source_commit"
    printf 'SOURCE TAG %s\n' "$release_tag"
    printf 'APP VERSION %s\n' "$app_version"
    printf 'DIST CHANNEL %s\n' "$dist_channel"
} > "$gate_log"

run_gate clean make -C "$project_root" clean
run_gate test-alpha-candidate make -C "$project_root" test-alpha-candidate
run_gate debug make -C "$project_root" debug
run_gate test-architecture make -C "$project_root" test-architecture
run_gate test make -C "$project_root" test
run_gate test-sanitize make -C "$project_root" test-sanitize
run_gate coverage make -C "$project_root" coverage
run_gate test-dist make -C "$project_root" test-dist \
    "DIST_CHANNEL=$dist_channel" \
    "DIST_SOURCE_COMMIT=$source_commit" \
    "DIST_SOURCE_TAG=$release_tag"

dist_config="$project_root/build/dist/.build-config"
[ -f "$dist_config" ] && [ ! -L "$dist_config" ] || \
    fail "test-dist did not produce a regular build configuration"
dist_arch=$(read_config_value arch) || \
    fail "distribution architecture is missing or ambiguous"
dist_macos_min=$(read_config_value macos-min) || \
    fail "distribution deployment target is missing or ambiguous"
dist_source_commit=$(read_config_value source-commit) || \
    fail "distribution source commit is missing or ambiguous"
dist_source_tag=$(read_config_value source-tag) || \
    fail "distribution source tag is missing or ambiguous"
dist_raylib_version=$(read_config_value raylib-version) || \
    fail "distribution raylib version is missing or ambiguous"
dist_performance_capability_schema=$(read_config_value \
    performance-capability-schema) || \
    fail "distribution performance capability schema is missing or ambiguous"
dist_performance_telemetry_schema=$(read_config_value \
    performance-telemetry-schema) || \
    fail "distribution performance telemetry schema is missing or ambiguous"
dist_performance_contract_sha256=$(read_config_value \
    performance-capability-contract-sha256) || \
    fail "distribution performance capability contract is missing or ambiguous"
[ "$dist_arch" = arm64 ] || \
    fail "Alpha candidates must target arm64, not '$dist_arch'"
printf '%s\n' "$dist_macos_min" | \
    grep -Eq '^[0-9]+(\.[0-9]+)*$' || \
    fail "distribution deployment target is invalid"
[ "$dist_source_commit" = "$source_commit" ] || \
    fail "distribution build configuration names a different source commit"
[ "$dist_source_tag" = "$release_tag" ] || \
    fail "distribution build configuration names a different source tag"
[ "$dist_raylib_version" = "$required_raylib_version" ] || \
    fail "distribution build configuration names an unsupported raylib version"
[ "$dist_performance_capability_schema" = \
    "$required_performance_capability_schema" ] || \
    fail "distribution build configuration names an unsupported performance capability schema"
[ "$dist_performance_telemetry_schema" = \
    "$required_performance_telemetry_schema" ] || \
    fail "distribution build configuration names an unsupported performance telemetry schema"
[ "$dist_performance_contract_sha256" = \
    "$required_performance_contract_sha256" ] || \
    fail "distribution build configuration names an unsupported performance capability contract"

artifact_basename="Tanks3D-$app_version-$dist_channel-macos-$dist_arch-macos$dist_macos_min"
artifact_filename="$artifact_basename.zip"
checksum_filename="$artifact_filename.sha256"
artifact="$project_root/build/dist/$artifact_filename"
checksum="$project_root/build/dist/$checksum_filename"
[ -f "$artifact" ] && [ ! -L "$artifact" ] || \
    fail "test-dist did not produce the expected ZIP"
[ -f "$checksum" ] && [ ! -L "$checksum" ] || \
    fail "test-dist did not produce the expected checksum"

checksum_records=$(awk 'NF { count++ } END { print count + 0 }' "$checksum")
[ "$checksum_records" -eq 1 ] || \
    fail "checksum must contain exactly one non-empty record"
checksum_fields=$(awk 'NF { print NF; exit }' "$checksum")
[ "$checksum_fields" -eq 2 ] || \
    fail "checksum record must contain a digest and filename"
recorded_hash=$(awk 'NF { print $1; exit }' "$checksum")
recorded_target=$(awk 'NF { print $2; exit }' "$checksum")
recorded_target=${recorded_target#\*}
artifact_sha256=$(shasum -a 256 "$artifact" | awk '{print $1}')
[ "$recorded_hash" = "$artifact_sha256" ] || \
    fail "checksum digest does not match the ZIP"
[ "$recorded_target" = "$artifact_filename" ] || \
    fail "checksum record names a different ZIP"
(
    cd "$project_root/build/dist"
    shasum -a 256 -c "$checksum_filename"
) >> "$gate_log" 2>&1 || fail "ZIP checksum verification failed"
unzip -tq "$artifact" >> "$gate_log" 2>&1 || fail "ZIP integrity check failed"

finish_commit=$(git -C "$project_root" rev-parse --verify HEAD^{commit} \
    2>/dev/null) || fail "HEAD cannot be re-read after the gates"
[ "$finish_commit" = "$source_commit" ] || \
    fail "HEAD changed while candidate gates were running"
finish_tag_commit=$(git -C "$project_root" rev-parse --verify \
    "refs/tags/$release_tag^{commit}" 2>/dev/null) || \
    fail "release tag disappeared while candidate gates were running"
[ "$finish_tag_commit" = "$source_commit" ] || \
    fail "release tag changed while candidate gates were running"
finish_version=$(read_app_version) || fail "bundle version cannot be re-read"
[ "$finish_version" = "$app_version" ] || \
    fail "bundle version changed while candidate gates were running"
finish_status=$(git -C "$project_root" status --porcelain=v1 \
    --untracked-files=all) || fail "Git worktree status cannot be re-read"
[ -z "$finish_status" ] || \
    fail "Git worktree became dirty while candidate gates were running"
{
    printf 'SOURCE HEAD VERIFIED %s\n' "$finish_commit"
    printf 'SOURCE TAG VERIFIED %s\n' "$release_tag"
    printf 'SOURCE TREE VERIFIED clean\n'
    printf 'ARTIFACT SHA256 VERIFIED %s\n' "$artifact_sha256"
} >> "$gate_log"

mkdir -p "$release_root"
stage_dir=$(mktemp -d "$release_root/.$release_tag.candidate.XXXXXX") || \
    fail "candidate staging directory cannot be created"
cp "$artifact" "$stage_dir/$artifact_filename"
cp "$checksum" "$stage_dir/$checksum_filename"
cp "$dist_config" "$stage_dir/build-config.txt"
cp "$gate_log" "$stage_dir/alpha-candidate-gates.log"

staged_artifact_sha256=$(shasum -a 256 \
    "$stage_dir/$artifact_filename" | awk '{print $1}')
[ "$staged_artifact_sha256" = "$artifact_sha256" ] || \
    fail "staged ZIP differs from the verified ZIP"
build_config_sha256=$(shasum -a 256 \
    "$stage_dir/build-config.txt" | awk '{print $1}')
gate_log_sha256=$(shasum -a 256 \
    "$stage_dir/alpha-candidate-gates.log" | awk '{print $1}')

{
    printf 'schema=tanks3d-alpha-candidate-v3\n'
    printf 'source_commit=%s\n' "$source_commit"
    printf 'source_head_at_start=%s\n' "$source_commit"
    printf 'source_head_at_finish=%s\n' "$finish_commit"
    printf 'source_tag=%s\n' "$release_tag"
    printf 'source_tag_commit=%s\n' "$finish_tag_commit"
    printf 'source_tree=clean\n'
    printf 'app_version=%s\n' "$app_version"
    printf 'dist_channel=%s\n' "$dist_channel"
    printf 'dist_arch=%s\n' "$dist_arch"
    printf 'dist_macos_min=%s\n' "$dist_macos_min"
    printf 'artifact_filename=%s\n' "$artifact_filename"
    printf 'artifact_sha256=%s\n' "$artifact_sha256"
    printf 'checksum_filename=%s\n' "$checksum_filename"
    printf 'build_config_filename=build-config.txt\n'
    printf 'build_config_sha256=%s\n' "$build_config_sha256"
    printf 'gate_log_filename=alpha-candidate-gates.log\n'
    printf 'gate_log_sha256=%s\n' "$gate_log_sha256"
    printf 'gate_clean=PASS\n'
    printf 'gate_test_alpha_candidate=PASS\n'
    printf 'gate_debug=PASS\n'
    printf 'gate_test_architecture=PASS\n'
    printf 'gate_test=PASS\n'
    printf 'gate_test_sanitize=PASS\n'
    printf 'gate_coverage=PASS\n'
    printf 'gate_test_dist=PASS\n'
} > "$stage_dir/attestation.txt"

sh "$project_root/scripts/verify_alpha_candidate.sh" --staging \
    "$project_root" "$stage_dir"
[ ! -e "$candidate_dir" ] && [ ! -L "$candidate_dir" ] || \
    fail "candidate destination appeared while gates were running"
mv "$stage_dir" "$candidate_dir"
stage_dir=

printf 'Alpha candidate created: %s\n' "$candidate_dir"
printf 'Artifact SHA-256: %s\n' "$artifact_sha256"
