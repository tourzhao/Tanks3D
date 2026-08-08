#!/bin/sh

set -eu
LC_ALL=C
export LC_ALL

if [ "$#" -ne 7 ]; then
    echo "usage: $0 PROJECT_ROOT ARCHIVE CHECKSUM ARCH MACOS_MIN VERSION BASENAME" >&2
    exit 2
fi

project_root=$1
archive=$2
checksum=$3
expected_arch=$4
expected_macos_min=$5
expected_version=$6
expected_basename=$7

fail()
{
    echo "distribution verification failed: $1" >&2
    exit 1
}

contains_newline()
{
    case "$1" in
        *'
'*) return 0 ;;
    esac
    return 1
}

resolve_directory()
{
    CDPATH= cd -P "$1" 2>/dev/null || return 1
    resolved_directory=$PWD
}

if contains_newline "$project_root" || contains_newline "$archive" || \
    contains_newline "$checksum"; then
    fail "input paths must not contain newline characters"
fi

CDPATH= cd -P . 2>/dev/null || fail "working directory is not accessible"
invocation_dir=$PWD
contains_newline "$invocation_dir" && \
    fail "resolved paths must not contain newline characters"
case "$project_root" in /*) ;; *) project_root="$invocation_dir/$project_root" ;; esac
case "$archive" in /*) ;; *) archive="$invocation_dir/$archive" ;; esac
case "$checksum" in /*) ;; *) checksum="$invocation_dir/$checksum" ;; esac

resolve_directory "$project_root" || fail "project root is not accessible"
project_root=$resolved_directory
contains_newline "$project_root" && \
    fail "resolved paths must not contain newline characters"

[ -f "$archive" ] || fail "archive is missing"
[ ! -L "$archive" ] || fail "archive must not be a symbolic link"
archive_parent=${archive%/*}
[ -n "$archive_parent" ] || archive_parent=/
archive_name=${archive##*/}
resolve_directory "$archive_parent" || fail "archive directory is not accessible"
archive_dir=$resolved_directory
contains_newline "$archive_dir" && \
    fail "resolved paths must not contain newline characters"
archive="$archive_dir/$archive_name"
[ -f "$archive" ] || fail "archive changed during path resolution"
[ ! -L "$archive" ] || fail "archive must not be a symbolic link"

[ -f "$checksum" ] || fail "checksum is missing"
[ ! -L "$checksum" ] || fail "checksum must not be a symbolic link"
checksum_parent=${checksum%/*}
[ -n "$checksum_parent" ] || checksum_parent=/
checksum_name=${checksum##*/}
resolve_directory "$checksum_parent" || \
    fail "checksum directory is not accessible"
checksum_dir=$resolved_directory
contains_newline "$checksum_dir" && \
    fail "resolved paths must not contain newline characters"
checksum="$checksum_dir/$checksum_name"
[ -f "$checksum" ] || fail "checksum changed during path resolution"
[ ! -L "$checksum" ] || fail "checksum must not be a symbolic link"

resource_manifest="$project_root/tests/expected_dist_bundle_resources.txt"
case "$archive" in
    "$project_root"/build/dist/*.zip | \
        "$project_root"/build/release/*/*.zip) ;;
    *) fail "archive is outside the permitted build output" ;;
esac

[ -f "$resource_manifest" ] || fail "resource manifest is missing"

[ "$archive_name" = "$expected_basename.zip" ] || \
    fail "archive name does not match the requested release identity"
[ "$checksum" = "$archive.sha256" ] || \
    fail "checksum path does not belong to the archive"

verify_root=$(mktemp -d /private/tmp/tanks3d-dist-verify.XXXXXX) || \
    fail "could not create a private verification directory"
trap 'rm -rf "$verify_root"' EXIT HUP INT TERM
snapshot_dir="$verify_root/input"
mkdir -p "$snapshot_dir"
snapshot_archive="$snapshot_dir/$archive_name"
snapshot_checksum="$snapshot_archive.sha256"
resolve_directory "$checksum_dir" || \
    fail "checksum directory changed before snapshot"
[ "$resolved_directory" = "$checksum_dir" ] || \
    fail "checksum directory changed before snapshot"
if ! /bin/cp -RP "./$checksum_name" "$snapshot_checksum"; then
    fail "could not snapshot the checksum"
fi
[ -f "$snapshot_checksum" ] && [ ! -L "$snapshot_checksum" ] || \
    fail "checksum changed while being snapshotted"
resolve_directory "$archive_dir" || \
    fail "archive directory changed before snapshot"
[ "$resolved_directory" = "$archive_dir" ] || \
    fail "archive directory changed before snapshot"
if ! /bin/cp -RP "./$archive_name" "$snapshot_archive"; then
    fail "could not snapshot the archive"
fi
[ -f "$snapshot_archive" ] && [ ! -L "$snapshot_archive" ] || \
    fail "archive changed while being snapshotted"

resolve_directory "$archive_parent" || \
    fail "archive directory changed while being snapshotted"
[ "$resolved_directory" = "$archive_dir" ] || \
    fail "archive directory changed while being snapshotted"
contains_newline "$resolved_directory" && \
    fail "resolved paths must not contain newline characters"
[ -f "$archive" ] && [ ! -L "$archive" ] || \
    fail "archive changed while being snapshotted"
resolve_directory "$checksum_parent" || \
    fail "checksum directory changed while being snapshotted"
[ "$resolved_directory" = "$checksum_dir" ] || \
    fail "checksum directory changed while being snapshotted"
contains_newline "$resolved_directory" && \
    fail "resolved paths must not contain newline characters"
[ -f "$checksum" ] && [ ! -L "$checksum" ] || \
    fail "checksum changed while being snapshotted"

archive="$snapshot_archive"
checksum="$snapshot_checksum"
archive_dir="$snapshot_dir"
checksum_dir="$snapshot_dir"
checksum_name="$archive_name.sha256"

checksum_line_count=$(awk 'NF{count++} END{print count+0}' "$checksum")
[ "$checksum_line_count" -eq 1 ] || \
    fail "checksum must contain exactly one non-empty record"
checksum_hash=$(awk 'NF{print $1; exit}' "$checksum")
checksum_target=$(awk 'NF{print $2; exit}' "$checksum")
checksum_target=${checksum_target#\*}
printf '%s\n' "$checksum_hash" | grep -Eq '^[[:xdigit:]]{64}$' || \
    fail "checksum does not contain a SHA-256 digest"
[ "$checksum_target" = "$archive_name" ] || \
    fail "checksum record names a different archive"

archive_entries="$verify_root/archive-entries.txt"
zipinfo -1 "$archive" > "$archive_entries"
[ -s "$archive_entries" ] || fail "archive contains no entries"

invalid_entry=$(awk '
    /^\// || /\\/ || $0 !~ /^Tanks3D\.app\// {print; exit}
    {
        count = split($0, component, "/")
        for (i = 1; i <= count; i++) {
            if (component[i] == "..") {
                print
                exit
            }
        }
    }
' "$archive_entries")
[ -z "$invalid_entry" ] || {
    echo "$invalid_entry" >&2
    fail "archive contains an unsafe or unexpected path"
}

duplicate_entry=$(LC_ALL=C sort "$archive_entries" | uniq -d | sed -n '1p')
[ -z "$duplicate_entry" ] || fail "archive contains a duplicate entry"

symlink_entry=$(zipinfo -l "$archive" | awk '$1 ~ /^l/{print; exit}')
[ -z "$symlink_entry" ] || fail "archive contains a symbolic link"

archive_files="$verify_root/archive-files.txt"
expected_archive_files="$verify_root/expected-archive-files.txt"
awk 'substr($0, length($0), 1) != "/"' "$archive_entries" | \
    LC_ALL=C sort > "$archive_files"
{
    printf '%s\n' \
        Tanks3D.app/Contents/Info.plist \
        Tanks3D.app/Contents/MacOS/Tanks3D \
        Tanks3D.app/Contents/_CodeSignature/CodeResources
    sed 's#^\./#Tanks3D.app/Contents/Resources/#' "$resource_manifest"
} | LC_ALL=C sort > "$expected_archive_files"
diff -u "$expected_archive_files" "$archive_files" || \
    fail "archive file manifest does not match the release contract"

original_dir=$PWD
if ! (
    cd "$archive_dir"
    shasum -a 256 -c "$checksum_name"
); then
    fail "archive checksum verification failed"
fi
unzip -tq "$archive"

zip_metadata=$(zipinfo -1 "$archive" | awk '/(^|\/)__MACOSX(\/|$)|(^|\/)\.DS_Store$/ {print; exit}')
[ -z "$zip_metadata" ] || fail "archive contains Finder or resource-fork metadata"

metadata_root="$verify_root/metadata"
clean_root="$verify_root/clean"
mkdir -p "$metadata_root" "$clean_root"
ditto -x -k "$archive" "$metadata_root"
archived_forbidden_metadata=$(xattr -lr "$metadata_root/Tanks3D.app" \
    2>/dev/null | awk '/com\.apple\.quarantine|com\.nextdlp\./ {print; exit}')
[ -z "$archived_forbidden_metadata" ] || \
    fail "archive carries quarantine or local DLP metadata"

ditto -x -k --norsrc --noextattr --noqtn --noacl "$archive" "$clean_root"

app="$clean_root/Tanks3D.app"
executable="$app/Contents/MacOS/Tanks3D"
plist="$app/Contents/Info.plist"
resources="$app/Contents/Resources"
[ -d "$app" ] || fail "Tanks3D.app is missing after extraction"
[ -x "$executable" ] || fail "app executable is missing or not executable"
plutil -lint "$plist"

bundle_id=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$plist")
bundle_executable=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$plist")
bundle_version=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$plist")
bundle_macos_min=$(/usr/libexec/PlistBuddy -c 'Print :LSMinimumSystemVersion' "$plist")
[ "$bundle_id" = "io.github.tourzhao.tanks3d" ] || fail "unexpected bundle identifier"
[ "$bundle_executable" = "Tanks3D" ] || fail "unexpected bundle executable"
[ "$bundle_version" = "$expected_version" ] || fail "unexpected bundle version"
[ "$bundle_macos_min" = "$expected_macos_min" ] || fail "plist deployment target mismatch"

actual_arch=$(lipo -archs "$executable")
[ "$actual_arch" = "$expected_arch" ] || fail "Mach-O architecture mismatch"
actual_macos_min=$(vtool -show-build "$executable" | awk '/minos/{print $2; exit}')
[ "$actual_macos_min" = "$expected_macos_min" ] || fail "Mach-O deployment target mismatch"

macho_files="$verify_root/macho-files.txt"
find "$app" -type f -print | while IFS= read -r candidate; do
    if file -b "$candidate" | grep -q 'Mach-O'; then
        printf '%s\n' "$candidate"
    fi
done > "$macho_files"
macho_count=$(awk 'END{print NR+0}' "$macho_files")
[ "$macho_count" -eq 1 ] || \
    fail "app must contain exactly one Mach-O executable"
[ "$(sed -n '1p' "$macho_files")" = "$executable" ] || \
    fail "app contains an unexpected Mach-O file"

unexpected_dependencies=$(otool -L "$executable" | awk '
    NR > 1 {
        path = $1
        if (path !~ "^/System/Library/" && path !~ "^/usr/lib/")
            print path
    }')
[ -z "$unexpected_dependencies" ] || {
    echo "$unexpected_dependencies" >&2
    fail "executable contains a non-system dynamic dependency"
}

unexpected_rpaths=$(otool -l "$executable" | awk '
    $1 == "cmd" && $2 == "LC_RPATH" {found = 1}
    found && $1 == "path" {print $2; found = 0}
')
[ -z "$unexpected_rpaths" ] || {
    echo "$unexpected_rpaths" >&2
    fail "executable contains an LC_RPATH entry"
}

if ! codesign --verify --deep --strict --verbose=4 "$app"; then
    fail "app signature integrity check failed"
fi

resource_list="$verify_root/resources.txt"
cd "$resources"
find . -type f -print | LC_ALL=C sort > "$resource_list"
cd "$original_dir"
diff -u "$resource_manifest" "$resource_list"

cmp "$project_root/LICENSE" "$resources/licenses/LICENSE"
cmp "$project_root/NOTICE" "$resources/licenses/NOTICE"
cmp "$project_root/README.md" "$resources/licenses/README.md"
cmp "$project_root/THIRD_PARTY_NOTICES.md" \
    "$resources/licenses/THIRD_PARTY_NOTICES.md"
cmp "$project_root/ASSET_LICENSES.md" \
    "$resources/licenses/ASSET_LICENSES.md"
cmp "$project_root/LICENSES/CC0-1.0.txt" \
    "$resources/licenses/LICENSES/CC0-1.0.txt"
cmp "$project_root/LICENSES/MIT-upstream.txt" \
    "$resources/licenses/LICENSES/MIT-upstream.txt"
cmp "$project_root/LICENSES/Zlib-raylib.txt" \
    "$resources/licenses/LICENSES/Zlib-raylib.txt"

forbidden_metadata=$(xattr -lr "$app" 2>/dev/null | \
    awk '/com\.apple\.quarantine|com\.nextdlp\./ {print; exit}')
[ -z "$forbidden_metadata" ] || fail "archive retained quarantine or local DLP metadata"

unexpected_files=$(find "$app" \( -name .DS_Store -o -name __MACOSX \) \
    -print -quit)
[ -z "$unexpected_files" ] || fail "extracted app contains Finder metadata"

local_paths=$(grep -R -a -l -E '/Users/|/opt/homebrew|/usr/local/' "$app" \
    2>/dev/null | sed -n '1p')
[ -z "$local_paths" ] || fail "app embeds a build-machine path"

"$executable" --self-test
echo "Verified self-contained distribution: $archive_name"
