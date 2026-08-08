#!/bin/sh

set -eu
LC_ALL=C
export LC_ALL
umask 077

if [ "$#" -ne 2 ]; then
    echo "usage: $0 PROJECT_ROOT CANDIDATE_DIR" >&2
    exit 2
fi

project_root_argument=$1
candidate_dir_argument=$2

fail()
{
    echo "tagged Alpha candidate verification failed: $1" >&2
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
    awk -v wanted="$attestation_key" '
        index($0, wanted "=") == 1 {
            count++
            value = substr($0, length(wanted) + 2)
        }
        END {
            if (count != 1)
                exit 1
            print value
        }
    ' "$attestation"
}

command -v git >/dev/null 2>&1 || fail "git is required"
command -v mktemp >/dev/null 2>&1 || fail "mktemp is required"
command -v cp >/dev/null 2>&1 || fail "cp is required"
command -v cmp >/dev/null 2>&1 || fail "cmp is required"

[ ! -L "$project_root_argument" ] || fail "project root must not be a symlink"
project_root=$(CDPATH= cd "$project_root_argument" 2>/dev/null && pwd -P) || \
    fail "project root is not accessible"
[ ! -L "$candidate_dir_argument" ] || \
    fail "candidate directory must not be a symlink"
candidate_dir=$(CDPATH= cd "$candidate_dir_argument" 2>/dev/null && pwd -P) || \
    fail "candidate directory is not accessible"

git_root=$(git -C "$project_root" rev-parse --show-toplevel 2>/dev/null) || \
    fail "project root is not a Git worktree"
git_root=$(CDPATH= cd "$git_root" 2>/dev/null && pwd -P) || \
    fail "Git worktree root is not accessible"
[ "$git_root" = "$project_root" ] || \
    fail "PROJECT_ROOT must be the Git worktree root"

initial_status=$(git -C "$project_root" status --porcelain=v1 \
    --untracked-files=all) || fail "current Git worktree status cannot be read"
[ -z "$initial_status" ] || fail "current Git worktree is not clean"
initial_head=$(git -C "$project_root" rev-parse --verify HEAD^{commit} \
    2>/dev/null) || fail "current HEAD does not resolve to a commit"

attestation="$candidate_dir/attestation.txt"
[ -f "$attestation" ] && [ ! -L "$attestation" ] || \
    fail "attestation.txt is missing or is not a regular file"

source_commit=$(attestation_value source_commit) || \
    fail "source_commit is missing or duplicated in the attestation"
source_tag=$(attestation_value source_tag) || \
    fail "source_tag is missing or duplicated in the attestation"
artifact_filename=$(attestation_value artifact_filename) || \
    fail "artifact_filename is missing or duplicated in the attestation"
checksum_filename=$(attestation_value checksum_filename) || \
    fail "checksum_filename is missing or duplicated in the attestation"
build_config_filename=$(attestation_value build_config_filename) || \
    fail "build_config_filename is missing or duplicated in the attestation"
gate_log_filename=$(attestation_value gate_log_filename) || \
    fail "gate_log_filename is missing or duplicated in the attestation"

require_commit_id "attested source commit" "$source_commit"
require_safe_token "attested release tag" "$source_tag"
require_safe_token "artifact filename" "$artifact_filename"
require_safe_token "checksum filename" "$checksum_filename"
require_safe_token "build configuration filename" "$build_config_filename"
require_safe_token "gate log filename" "$gate_log_filename"

[ ! -L "$project_root/build" ] || \
    fail "project build directory must not be a symlink"
[ ! -L "$project_root/build/release" ] || \
    fail "project release directory must not be a symlink"
expected_candidate_dir="$project_root/build/release/$source_tag"
[ "$candidate_dir" = "$expected_candidate_dir" ] || \
    fail "candidate directory name does not match the attested release tag"

current_tag_commit=$(git -C "$project_root" rev-parse --verify \
    "refs/tags/$source_tag^{commit}" 2>/dev/null) || \
    fail "attested release tag does not exist"
[ "$current_tag_commit" = "$source_commit" ] || \
    fail "attested release tag does not identify the attested commit"

for first_name in attestation.txt "$artifact_filename" "$checksum_filename" \
        "$build_config_filename" "$gate_log_filename"; do
    duplicate_count=0
    for second_name in attestation.txt "$artifact_filename" \
            "$checksum_filename" "$build_config_filename" \
            "$gate_log_filename"; do
        if [ "$first_name" = "$second_name" ]; then
            duplicate_count=$((duplicate_count + 1))
        fi
    done
    [ "$duplicate_count" -eq 1 ] || \
        fail "attestation assigns duplicate candidate filenames"
done

validate_candidate_file_set()
{
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
            fail "candidate contains a non-regular or symbolic-link entry"
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
}

validate_candidate_file_set

temporary_root=${TMPDIR:-/tmp}
[ ! -L "$temporary_root" ] || fail "temporary root must not be a symlink"
temporary_root=$(CDPATH= cd "$temporary_root" 2>/dev/null && pwd -P) || \
    fail "temporary root is not accessible"
snapshot_workspace=
snapshot_owned=no

cleanup()
{
    cleanup_status=$?
    trap - 0 1 2 15
    if [ "${snapshot_owned:-no}" = yes ]; then
        rm -rf "$snapshot_workspace"
    fi
    exit "$cleanup_status"
}
trap cleanup 0
trap 'exit 1' 1 2 15

snapshot_workspace=$(mktemp -d \
    "$temporary_root/tanks3d-tagged-alpha.XXXXXX") || \
    fail "private snapshot directory cannot be created"
snapshot_owned=yes

snapshot_parent=$(CDPATH= cd "$snapshot_workspace/.." 2>/dev/null && pwd -P) || \
    fail "private snapshot parent cannot be normalized"
[ "$snapshot_parent" = "$temporary_root" ] || \
    fail "private snapshot was created outside the temporary root"
case "${snapshot_workspace##*/}" in
    tanks3d-tagged-alpha.*) ;;
    *) fail "private snapshot has an unexpected path" ;;
esac
[ -d "$snapshot_workspace" ] && [ ! -L "$snapshot_workspace" ] || \
    fail "private snapshot is not a real directory"

snapshot_root="$snapshot_workspace/source"
git clone --quiet --no-hardlinks --no-checkout "$project_root" \
    "$snapshot_root" || fail "cannot create a private source clone"
git -C "$snapshot_root" checkout --quiet --detach "$source_commit" || \
    fail "cannot check out the attested source commit"
snapshot_commit=$(git -C "$snapshot_root" rev-parse --verify HEAD^{commit} \
    2>/dev/null) || fail "snapshot HEAD does not resolve to a commit"
[ "$snapshot_commit" = "$source_commit" ] || \
    fail "snapshot HEAD does not match the attested commit"

snapshot_verifier="$snapshot_root/scripts/verify_alpha_candidate.sh"
[ -f "$snapshot_verifier" ] && [ ! -L "$snapshot_verifier" ] || \
    fail "attested source has no regular strict candidate verifier"

snapshot_candidate="$snapshot_root/build/release/$source_tag"
mkdir -p "$snapshot_candidate" || \
    fail "cannot create the candidate directory in the source snapshot"

copy_candidate_file()
{
    copied_name=$1
    source_file="$candidate_dir/$copied_name"
    snapshot_file="$snapshot_candidate/$copied_name"
    cp -P "$source_file" "$snapshot_file" || \
        fail "cannot copy candidate file '$copied_name' into the snapshot"
    [ -f "$snapshot_file" ] && [ ! -L "$snapshot_file" ] || \
        fail "candidate file '$copied_name' did not copy as a regular file"
    cmp -s "$source_file" "$snapshot_file" || \
        fail "candidate file '$copied_name' changed while it was copied"
}

copy_candidate_file attestation.txt
copy_candidate_file "$artifact_filename"
copy_candidate_file "$checksum_filename"
copy_candidate_file "$build_config_filename"
copy_candidate_file "$gate_log_filename"

if ! sh "$snapshot_verifier" "$snapshot_root" "$snapshot_candidate"; then
    fail "the attested tag's strict verifier rejected the candidate snapshot"
fi

validate_candidate_file_set
for verified_name in attestation.txt "$artifact_filename" \
        "$checksum_filename" "$build_config_filename" "$gate_log_filename"; do
    cmp -s "$candidate_dir/$verified_name" \
        "$snapshot_candidate/$verified_name" || \
        fail "candidate changed during tagged verification"
done

finish_head=$(git -C "$project_root" rev-parse --verify HEAD^{commit} \
    2>/dev/null) || fail "current HEAD cannot be re-read"
[ "$finish_head" = "$initial_head" ] || \
    fail "current HEAD changed during tagged verification"
finish_tag_commit=$(git -C "$project_root" rev-parse --verify \
    "refs/tags/$source_tag^{commit}" 2>/dev/null) || \
    fail "attested release tag disappeared during tagged verification"
[ "$finish_tag_commit" = "$source_commit" ] || \
    fail "attested release tag changed during tagged verification"
finish_status=$(git -C "$project_root" status --porcelain=v1 \
    --untracked-files=all) || fail "current Git worktree status cannot be re-read"
[ -z "$finish_status" ] || \
    fail "current Git worktree became dirty during tagged verification"

printf 'Verified tagged Alpha candidate: %s\n' "$candidate_dir"
printf 'Attested source snapshot: %s (%s)\n' "$source_commit" "$source_tag"
