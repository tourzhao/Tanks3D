#!/bin/sh

set -eu
LC_ALL=C
export LC_ALL

staging_mode=no
if [ "${1:-}" = --staging ]; then
    staging_mode=yes
    shift
fi

if [ "$#" -ne 2 ]; then
    echo "usage: $0 [--staging] PROJECT_ROOT CANDIDATE_DIR" >&2
    exit 2
fi

project_root=$1
candidate_dir=$2
required_raylib_version=6.0
required_performance_capability_schema=tanks3d-release-performance-capabilities-v1
required_performance_telemetry_schema=tanks3d-performance-log-v2
required_performance_contract_sha256=5137950da46fa11ee6d5ff60fafe67e83c4c0aacfb5fc83f2b0ce5f74afcfe1c

fail()
{
    echo "alpha candidate verification failed: $1" >&2
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

require_sha256()
{
    digest_label=$1
    digest_value=$2
    [ "${#digest_value}" -eq 64 ] || \
        fail "$digest_label is not a SHA-256 digest"
    case "$digest_value" in
        *[!0-9a-f]*) fail "$digest_label is not a lowercase SHA-256 digest" ;;
    esac
}

require_commit_id()
{
    commit_label=$1
    commit_value=$2
    case "${#commit_value}" in
        40|64) ;;
        *) fail "$commit_label is not a full Git object ID" ;;
    esac
    case "$commit_value" in
        *[!0-9a-f]*) fail "$commit_label is not a lowercase Git object ID" ;;
    esac
}

attestation_value()
{
    attestation_key=$1
    sed -n "s/^$attestation_key=//p" "$attestation"
}

config_value()
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
    ' "$build_config"
}

command -v git >/dev/null 2>&1 || fail "git is required"
command -v shasum >/dev/null 2>&1 || fail "shasum is required"
command -v unzip >/dev/null 2>&1 || fail "unzip is required"
[ -x /usr/libexec/PlistBuddy ] || fail "/usr/libexec/PlistBuddy is required"

project_root=$(CDPATH= cd "$project_root" 2>/dev/null && pwd -P) || \
    fail "project root is not accessible"
[ ! -L "$candidate_dir" ] || fail "candidate directory must not be a symlink"
candidate_dir=$(CDPATH= cd "$candidate_dir" 2>/dev/null && pwd -P) || \
    fail "candidate directory is not accessible"

git_root=$(git -C "$project_root" rev-parse --show-toplevel 2>/dev/null) || \
    fail "project root is not a Git worktree"
git_root=$(CDPATH= cd "$git_root" 2>/dev/null && pwd -P) || \
    fail "Git worktree root is not accessible"
[ "$git_root" = "$project_root" ] || \
    fail "PROJECT_ROOT must be the Git worktree root"

attestation="$candidate_dir/attestation.txt"
[ -f "$attestation" ] && [ ! -L "$attestation" ] || \
    fail "attestation.txt is missing or is not a regular file"

expected_keys=$(printf '%s\n' \
    schema \
    source_commit \
    source_head_at_start \
    source_head_at_finish \
    source_tag \
    source_tag_commit \
    source_tree \
    app_version \
    dist_channel \
    dist_arch \
    dist_macos_min \
    artifact_filename \
    artifact_sha256 \
    checksum_filename \
    build_config_filename \
    build_config_sha256 \
    gate_log_filename \
    gate_log_sha256 \
    gate_clean \
    gate_test_alpha_candidate \
    gate_debug \
    gate_test_architecture \
    gate_test \
    gate_test_sanitize \
    gate_coverage \
    gate_test_dist)
actual_keys=$(sed 's/=.*//' "$attestation")
[ "$actual_keys" = "$expected_keys" ] || \
    fail "attestation keys are missing, duplicated, reordered, or unexpected"

schema=$(attestation_value schema)
source_commit=$(attestation_value source_commit)
source_head_at_start=$(attestation_value source_head_at_start)
source_head_at_finish=$(attestation_value source_head_at_finish)
source_tag=$(attestation_value source_tag)
source_tag_commit=$(attestation_value source_tag_commit)
source_tree=$(attestation_value source_tree)
app_version=$(attestation_value app_version)
dist_channel=$(attestation_value dist_channel)
dist_arch=$(attestation_value dist_arch)
dist_macos_min=$(attestation_value dist_macos_min)
artifact_filename=$(attestation_value artifact_filename)
artifact_sha256=$(attestation_value artifact_sha256)
checksum_filename=$(attestation_value checksum_filename)
build_config_filename=$(attestation_value build_config_filename)
build_config_sha256=$(attestation_value build_config_sha256)
gate_log_filename=$(attestation_value gate_log_filename)
gate_log_sha256=$(attestation_value gate_log_sha256)

[ "$schema" = tanks3d-alpha-candidate-v3 ] || \
    fail "unsupported attestation schema"
require_commit_id "source commit" "$source_commit"
require_commit_id "starting HEAD" "$source_head_at_start"
require_commit_id "finishing HEAD" "$source_head_at_finish"
require_commit_id "tag commit" "$source_tag_commit"
[ "$source_head_at_start" = "$source_commit" ] && \
    [ "$source_head_at_finish" = "$source_commit" ] && \
    [ "$source_tag_commit" = "$source_commit" ] || \
    fail "attestation does not bind HEAD and tag to one commit"
[ "$source_tree" = clean ] || fail "attestation does not record a clean tree"

require_safe_token "bundle version" "$app_version"
require_safe_token "distribution channel" "$dist_channel"
require_safe_token "release tag" "$source_tag"
expected_tag="v$app_version-$dist_channel"
[ "$source_tag" = "$expected_tag" ] || \
    fail "release tag does not match version and channel"
[ "$dist_arch" = arm64 ] || fail "candidate architecture must be arm64"
printf '%s\n' "$dist_macos_min" | \
    grep -Eq '^[0-9]+(\.[0-9]+)*$' || fail "deployment target is invalid"

artifact_basename="Tanks3D-$app_version-$dist_channel-macos-$dist_arch-macos$dist_macos_min"
expected_artifact_filename="$artifact_basename.zip"
[ "$artifact_filename" = "$expected_artifact_filename" ] || \
    fail "artifact filename does not match version, channel, or platform"
[ "$checksum_filename" = "$artifact_filename.sha256" ] || \
    fail "checksum filename does not belong to the artifact"
[ "$build_config_filename" = build-config.txt ] || \
    fail "unexpected build configuration filename"
[ "$gate_log_filename" = alpha-candidate-gates.log ] || \
    fail "unexpected gate log filename"
require_sha256 "artifact digest" "$artifact_sha256"
require_sha256 "build configuration digest" "$build_config_sha256"
require_sha256 "gate log digest" "$gate_log_sha256"

if [ "$staging_mode" = yes ]; then
    case "$candidate_dir" in
        "$project_root/build/release/.$source_tag.candidate."*) ;;
        *) fail "staging directory is outside the bound release location" ;;
    esac
else
    [ "$candidate_dir" = "$project_root/build/release/$source_tag" ] || \
        fail "candidate directory name does not match the release tag"
fi

for gate_key in gate_clean gate_test_alpha_candidate gate_debug \
        gate_test_architecture gate_test gate_test_sanitize gate_coverage \
        gate_test_dist; do
    [ "$(attestation_value "$gate_key")" = PASS ] || \
        fail "attestation gate '$gate_key' is not PASS"
done

artifact="$candidate_dir/$artifact_filename"
checksum="$candidate_dir/$checksum_filename"
build_config="$candidate_dir/$build_config_filename"
gate_log="$candidate_dir/$gate_log_filename"

entry_count=0
seen_attestation=no
seen_artifact=no
seen_checksum=no
seen_build_config=no
seen_gate_log=no
for candidate_entry in "$candidate_dir"/* "$candidate_dir"/.[!.]* \
        "$candidate_dir"/..?*; do
    if [ ! -e "$candidate_entry" ] && [ ! -L "$candidate_entry" ]; then
        continue
    fi
    [ -f "$candidate_entry" ] && [ ! -L "$candidate_entry" ] || \
        fail "candidate contains a non-regular entry"
    entry_name=${candidate_entry##*/}
    case "$entry_name" in
        attestation.txt) seen_attestation=yes ;;
        "$artifact_filename") seen_artifact=yes ;;
        "$checksum_filename") seen_checksum=yes ;;
        "$build_config_filename") seen_build_config=yes ;;
        "$gate_log_filename") seen_gate_log=yes ;;
        *) fail "candidate contains unexpected file '$entry_name'" ;;
    esac
    entry_count=$((entry_count + 1))
done
[ "$entry_count" -eq 5 ] && [ "$seen_attestation" = yes ] && \
    [ "$seen_artifact" = yes ] && [ "$seen_checksum" = yes ] && \
    [ "$seen_build_config" = yes ] && [ "$seen_gate_log" = yes ] || \
    fail "candidate file set is incomplete"

actual_artifact_sha256=$(shasum -a 256 "$artifact" | awk '{print $1}')
[ "$actual_artifact_sha256" = "$artifact_sha256" ] || \
    fail "artifact digest does not match the attestation"
actual_build_config_sha256=$(shasum -a 256 "$build_config" | awk '{print $1}')
[ "$actual_build_config_sha256" = "$build_config_sha256" ] || \
    fail "build configuration digest does not match the attestation"
actual_gate_log_sha256=$(shasum -a 256 "$gate_log" | awk '{print $1}')
[ "$actual_gate_log_sha256" = "$gate_log_sha256" ] || \
    fail "gate log digest does not match the attestation"

checksum_records=$(awk 'NF { count++ } END { print count + 0 }' "$checksum")
[ "$checksum_records" -eq 1 ] || \
    fail "checksum must contain exactly one non-empty record"
checksum_fields=$(awk 'NF { print NF; exit }' "$checksum")
[ "$checksum_fields" -eq 2 ] || \
    fail "checksum record must contain a digest and filename"
recorded_hash=$(awk 'NF { print $1; exit }' "$checksum")
recorded_target=$(awk 'NF { print $2; exit }' "$checksum")
recorded_target=${recorded_target#\*}
[ "$recorded_hash" = "$artifact_sha256" ] || \
    fail "checksum digest does not match the attestation"
[ "$recorded_target" = "$artifact_filename" ] || \
    fail "checksum record names a different artifact"
(
    cd "$candidate_dir"
    shasum -a 256 -c "$checksum_filename"
) >/dev/null 2>&1 || fail "artifact checksum verification failed"
unzip -tq "$artifact" >/dev/null 2>&1 || fail "artifact ZIP integrity check failed"

config_arch=$(config_value arch) || \
    fail "build configuration architecture is missing or ambiguous"
config_macos_min=$(config_value macos-min) || \
    fail "build configuration deployment target is missing or ambiguous"
config_source_commit=$(config_value source-commit) || \
    fail "build configuration source commit is missing or ambiguous"
config_source_tag=$(config_value source-tag) || \
    fail "build configuration source tag is missing or ambiguous"
config_raylib_version=$(config_value raylib-version) || \
    fail "build configuration raylib version is missing or ambiguous"
config_performance_capability_schema=$(config_value \
    performance-capability-schema) || \
    fail "build configuration performance capability schema is missing or ambiguous"
config_performance_telemetry_schema=$(config_value \
    performance-telemetry-schema) || \
    fail "build configuration performance telemetry schema is missing or ambiguous"
config_performance_contract_sha256=$(config_value \
    performance-capability-contract-sha256) || \
    fail "build configuration performance capability contract is missing or ambiguous"
[ "$config_arch" = "$dist_arch" ] || \
    fail "build configuration architecture does not match the attestation"
[ "$config_macos_min" = "$dist_macos_min" ] || \
    fail "build configuration deployment target does not match the attestation"
[ "$config_source_commit" = "$source_commit" ] || \
    fail "build configuration source commit does not match the attestation"
[ "$config_source_tag" = "$source_tag" ] || \
    fail "build configuration source tag does not match the attestation"
[ "$config_raylib_version" = "$required_raylib_version" ] || \
    fail "build configuration raylib version is unsupported"
[ "$config_performance_capability_schema" = \
    "$required_performance_capability_schema" ] || \
    fail "build configuration performance capability schema is unsupported"
[ "$config_performance_telemetry_schema" = \
    "$required_performance_telemetry_schema" ] || \
    fail "build configuration performance telemetry schema is unsupported"
[ "$config_performance_contract_sha256" = \
    "$required_performance_contract_sha256" ] || \
    fail "build configuration performance capability contract is unsupported"

dist_verifier="$project_root/scripts/verify_macos_dist.sh"
[ -f "$dist_verifier" ] && [ ! -L "$dist_verifier" ] || \
    fail "macOS distribution verifier is missing or is not a regular file"
if ! sh "$dist_verifier" "$project_root" "$artifact" "$checksum" \
        "$dist_arch" "$dist_macos_min" "$app_version" \
        "$artifact_basename" "$source_commit" "$source_tag" \
        >/dev/null 2>&1; then
    fail "macOS distribution verifier rejected the candidate artifact"
fi

gate_pass_count=$(grep -Ec '^GATE [A-Za-z0-9-]+ PASS$' "$gate_log" || :)
[ "$gate_pass_count" -eq 8 ] || \
    fail "gate log does not contain eight PASS records"
if grep -Eq '^GATE .* FAIL([[:space:]]|$)' "$gate_log"; then
    fail "gate log contains a FAIL record"
fi
for gate_name in clean test-alpha-candidate debug test-architecture test \
        test-sanitize coverage test-dist; do
    gate_record_count=$(grep -Fxc "GATE $gate_name PASS" "$gate_log" || :)
    [ "$gate_record_count" -eq 1 ] || \
        fail "gate log does not contain exactly one PASS for '$gate_name'"
done
grep -Fqx "SOURCE COMMIT $source_commit" "$gate_log" || \
    fail "gate log is not bound to the source commit"
grep -Fqx "SOURCE TAG $source_tag" "$gate_log" || \
    fail "gate log is not bound to the release tag"
grep -Fqx "APP VERSION $app_version" "$gate_log" || \
    fail "gate log is not bound to the bundle version"
grep -Fqx "DIST CHANNEL $dist_channel" "$gate_log" || \
    fail "gate log is not bound to the distribution channel"
grep -Fqx "SOURCE HEAD VERIFIED $source_commit" "$gate_log" || \
    fail "gate log does not record the final HEAD check"
grep -Fqx "SOURCE TAG VERIFIED $source_tag" "$gate_log" || \
    fail "gate log does not record the final tag check"
grep -Fqx 'SOURCE TREE VERIFIED clean' "$gate_log" || \
    fail "gate log does not record the final clean-tree check"
grep -Fqx "ARTIFACT SHA256 VERIFIED $artifact_sha256" "$gate_log" || \
    fail "gate log is not bound to the artifact digest"

current_commit=$(git -C "$project_root" rev-parse --verify HEAD^{commit} \
    2>/dev/null) || fail "current HEAD does not resolve to a commit"
[ "$current_commit" = "$source_commit" ] || \
    fail "current HEAD does not match the attested commit"
current_tag_commit=$(git -C "$project_root" rev-parse --verify \
    "refs/tags/$source_tag^{commit}" 2>/dev/null) || \
    fail "attested release tag does not exist"
[ "$current_tag_commit" = "$source_commit" ] || \
    fail "attested release tag does not identify the attested commit"
current_version=$(/usr/libexec/PlistBuddy -c \
    'Print :CFBundleShortVersionString' "$project_root/macos/Info.plist" \
    2>/dev/null) || fail "current bundle version cannot be read"
[ "$current_version" = "$app_version" ] || \
    fail "current bundle version does not match the attestation"
current_status=$(git -C "$project_root" status --porcelain=v1 \
    --untracked-files=all) || fail "current Git worktree status cannot be read"
[ -z "$current_status" ] || fail "current Git worktree is not clean"

printf 'Verified Alpha candidate: %s\n' "$candidate_dir"
printf 'Bound source: %s (%s)\n' "$source_commit" "$source_tag"
printf 'Artifact SHA-256: %s\n' "$artifact_sha256"
