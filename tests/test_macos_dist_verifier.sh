#!/bin/sh

set -eu
LC_ALL=C
export LC_ALL

if [ "$#" -ne 7 ]; then
    echo "usage: $0 PROJECT_ROOT VALID_ARCHIVE ARCH MACOS_MIN VERSION SOURCE_COMMIT SOURCE_TAG" >&2
    exit 2
fi

project_root=$1
valid_archive=$2
expected_arch=$3
expected_macos_min=$4
expected_version=$5
expected_source_commit=$6
expected_source_tag=$7
verifier="$project_root/scripts/verify_macos_dist.sh"

fail()
{
    echo "distribution verifier negative test failed: $1" >&2
    exit 1
}

[ -f "$valid_archive" ] || fail "valid archive fixture is missing"
[ -f "$verifier" ] || fail "distribution verifier is missing"
command -v zip >/dev/null 2>&1 || fail "zip is required"
command -v unzip >/dev/null 2>&1 || fail "unzip is required"
command -v codesign >/dev/null 2>&1 || fail "codesign is required"
command -v python3 >/dev/null 2>&1 || fail "python3 is required"
command -v clang >/dev/null 2>&1 || fail "clang is required"
[ -x /usr/bin/ditto ] || fail "ditto is required"

fixture_root=
escape_root=
cleanup()
{
    [ -z "$fixture_root" ] || rm -rf "$fixture_root"
    [ -z "$escape_root" ] || rm -rf "$escape_root"
}
trap cleanup EXIT HUP INT TERM
fixture_root=$(mktemp -d "$project_root/build/dist/verifier-negative.XXXXXX")
escape_root=$(mktemp -d "$project_root/build/verifier-path-escape.XXXXXX")

write_checksum()
{
    checksum_archive=$1
    checksum_dir=$(dirname "$checksum_archive")
    checksum_archive_name=$(basename "$checksum_archive")
    (
        cd "$checksum_dir"
        shasum -a 256 "$checksum_archive_name" > \
            "$checksum_archive_name.sha256"
    )
}

expect_rejection()
{
    rejection_label=$1
    expected_message=$2
    rejection_archive=$3
    rejection_basename=$4
    rejection_log="$fixture_root/$rejection_label.log"

    if sh "$verifier" "$project_root" "$rejection_archive" \
        "$rejection_archive.sha256" "$expected_arch" \
        "$expected_macos_min" "$expected_version" \
        "$rejection_basename" "$expected_source_commit" \
        "$expected_source_tag" > "$rejection_log" 2>&1; then
        fail "$rejection_label fixture was accepted"
    fi
    grep -F "$expected_message" "$rejection_log" >/dev/null || {
        sed -n '1,80p' "$rejection_log" >&2
        fail "$rejection_label fixture failed for the wrong reason"
    }
}

checksum_archive="$fixture_root/Tanks3D-checksum-binding.zip"
cp "$valid_archive" "$checksum_archive"
checksum_hash=$(shasum -a 256 "$checksum_archive" | awk '{print $1}')
printf '%s  %s\n' "$checksum_hash" unrelated-release.zip > \
    "$checksum_archive.sha256"
expect_rejection checksum-binding \
    "checksum record names a different archive" "$checksum_archive" \
    Tanks3D-checksum-binding

checksum_digest_archive="$fixture_root/Tanks3D-checksum-digest.zip"
cp "$valid_archive" "$checksum_digest_archive"
printf '%064d  %s\n' 0 "$(basename "$checksum_digest_archive")" > \
    "$checksum_digest_archive.sha256"
expect_rejection checksum-digest "archive checksum verification failed" \
    "$checksum_digest_archive" Tanks3D-checksum-digest

checksum_symlink_archive="$fixture_root/Tanks3D-checksum-symlink.zip"
cp "$valid_archive" "$checksum_symlink_archive"
write_checksum "$checksum_symlink_archive"
checksum_symlink_target="$fixture_root/checksum-symlink-target.sha256"
mv "$checksum_symlink_archive.sha256" "$checksum_symlink_target"
ln -s "$(basename "$checksum_symlink_target")" \
    "$checksum_symlink_archive.sha256"
expect_rejection checksum-symlink \
    "checksum must not be a symbolic link" \
    "$checksum_symlink_archive" Tanks3D-checksum-symlink

newline_dir="$fixture_root/newline-parent
"
mkdir -p "$newline_dir"
newline_archive_name=Tanks3D-newline-path.zip
newline_archive="$newline_dir/$newline_archive_name"
cp "$valid_archive" "$newline_archive"
(
    cd "$newline_dir"
    shasum -a 256 "$newline_archive_name" > \
        "$newline_archive_name.sha256"
)
expect_rejection newline-path \
    "input paths must not contain newline characters" \
    "$newline_archive" Tanks3D-newline-path

resolved_newline_dir="$fixture_root/resolved-newline-target
"
mkdir -p "$resolved_newline_dir"
resolved_newline_archive_name=Tanks3D-resolved-newline.zip
resolved_newline_archive="$resolved_newline_dir/$resolved_newline_archive_name"
cp "$valid_archive" "$resolved_newline_archive"
(
    cd "$resolved_newline_dir"
    shasum -a 256 "$resolved_newline_archive_name" > \
        "$resolved_newline_archive_name.sha256"
)
resolved_newline_link="$fixture_root/resolved-newline-parent"
ln -s "$resolved_newline_dir" "$resolved_newline_link"
expect_rejection resolved-newline-path \
    "resolved paths must not contain newline characters" \
    "$resolved_newline_link/$resolved_newline_archive_name" \
    Tanks3D-resolved-newline

path_escape_archive="$escape_root/Tanks3D-path-escape.zip"
cp "$valid_archive" "$path_escape_archive"
write_checksum "$path_escape_archive"
path_escape_lexical="$fixture_root/../../$(basename "$escape_root")/\
$(basename "$path_escape_archive")"
expect_rejection path-escape "archive is outside the permitted build output" \
    "$path_escape_lexical" Tanks3D-path-escape

parent_symlink_archive="$escape_root/Tanks3D-parent-symlink.zip"
cp "$valid_archive" "$parent_symlink_archive"
write_checksum "$parent_symlink_archive"
parent_symlink_dir="$fixture_root/escaped-parent"
ln -s "$escape_root" "$parent_symlink_dir"
expect_rejection parent-symlink \
    "archive is outside the permitted build output" \
    "$parent_symlink_dir/$(basename "$parent_symlink_archive")" \
    Tanks3D-parent-symlink

archive_symlink="$fixture_root/Tanks3D-archive-symlink.zip"
ln -s "$valid_archive" "$archive_symlink"
write_checksum "$archive_symlink"
expect_rejection archive-symlink "archive must not be a symbolic link" \
    "$archive_symlink" Tanks3D-archive-symlink

extra_archive="$fixture_root/Tanks3D-extra-entry.zip"
cp "$valid_archive" "$extra_archive"
printf '%s\n' "unexpected release payload" > \
    "$fixture_root/unexpected-release-file.txt"
(
    cd "$fixture_root"
    zip -q "$(basename "$extra_archive")" unexpected-release-file.txt
)
write_checksum "$extra_archive"
expect_rejection extra-entry "archive contains an unsafe or unexpected path" \
    "$extra_archive" Tanks3D-extra-entry

symlink_archive="$fixture_root/Tanks3D-symlink-entry.zip"
cp "$valid_archive" "$symlink_archive"
symlink_payload="$fixture_root/symlink-payload"
symlink_path=Tanks3D.app/Contents/Resources/licenses/unexpected-link
mkdir -p "$symlink_payload/Tanks3D.app/Contents/Resources/licenses"
ln -s ../../../../../outside "$symlink_payload/$symlink_path"
(
    cd "$symlink_payload"
    zip -q -y "$symlink_archive" "$symlink_path"
)
write_checksum "$symlink_archive"
expect_rejection symlink-entry "archive contains a symbolic link" \
    "$symlink_archive" Tanks3D-symlink-entry

signature_archive="$fixture_root/Tanks3D-signature-tamper.zip"
cp "$valid_archive" "$signature_archive"
signature_payload="$fixture_root/signature-payload"
signature_member=Tanks3D.app/Contents/Resources/sounds/player_idle.ogg
mkdir -p "$signature_payload/$(dirname "$signature_member")"
unzip -p "$valid_archive" "$signature_member" > \
    "$signature_payload/$signature_member"
printf '%s\n' "unsigned resource mutation" >> \
    "$signature_payload/$signature_member"
(
    cd "$signature_payload"
    zip -q "$signature_archive" "$signature_member"
)
write_checksum "$signature_archive"
expect_rejection signature-tamper "app signature integrity check failed" \
    "$signature_archive" Tanks3D-signature-tamper

dependency_notice_archive="$fixture_root/Tanks3D-dependency-notice-tamper.zip"
dependency_notice_payload="$fixture_root/dependency-notice-payload"
mkdir -p "$dependency_notice_payload"
unzip -q "$valid_archive" -d "$dependency_notice_payload"
dependency_notice_app="$dependency_notice_payload/Tanks3D.app"
dependency_notice_file="$dependency_notice_app/Contents/Resources/licenses/\
LICENSES/Raylib-6.0-dependencies.txt"
printf '%s\n' 'dependency notice mutation' >> "$dependency_notice_file"
codesign --force --sign - --timestamp=none "$dependency_notice_app" \
    >/dev/null 2>&1 || fail "dependency notice fixture could not be signed"
(
    cd "$dependency_notice_payload"
    /usr/bin/ditto -c -k --keepParent --norsrc --noextattr --noqtn \
        --noacl Tanks3D.app "$dependency_notice_archive"
)
write_checksum "$dependency_notice_archive"
expect_rejection dependency-notice-tamper \
    "bundled raylib dependency notices do not match the source notice" \
    "$dependency_notice_archive" Tanks3D-dependency-notice-tamper

raylib_license_archive="$fixture_root/Tanks3D-raylib-license-tamper.zip"
raylib_license_payload="$fixture_root/raylib-license-payload"
mkdir -p "$raylib_license_payload"
unzip -q "$valid_archive" -d "$raylib_license_payload"
raylib_license_app="$raylib_license_payload/Tanks3D.app"
raylib_license_file="$raylib_license_app/Contents/Resources/licenses/\
LICENSES/Zlib-raylib.txt"
printf '%s\n' 'raylib license mutation' >> "$raylib_license_file"
codesign --force --sign - --timestamp=none "$raylib_license_app" \
    >/dev/null 2>&1 || fail "raylib license fixture could not be signed"
(
    cd "$raylib_license_payload"
    /usr/bin/ditto -c -k --keepParent --norsrc --noextattr --noqtn \
        --noacl Tanks3D.app "$raylib_license_archive"
)
write_checksum "$raylib_license_archive"
expect_rejection raylib-license-tamper \
    "bundled raylib license does not match the official source notice" \
    "$raylib_license_archive" Tanks3D-raylib-license-tamper

capability_archive="$fixture_root/Tanks3D-performance-capability-tamper.zip"
capability_payload="$fixture_root/performance-capability-payload"
mkdir -p "$capability_payload"
unzip -q "$valid_archive" -d "$capability_payload"
capability_app="$capability_payload/Tanks3D.app"
capability_executable="$capability_app/Contents/MacOS/Tanks3D"
python3 - "$capability_executable" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
data = path.read_bytes()
before = b"tanks3d-release-performance-capabilities-v1"
after = b"tanks3d-release-performance-capabilities-x1"
if len(before) != len(after) or data.count(before) == 0:
    raise SystemExit("performance capability schema was not found in the executable")
path.write_bytes(data.replace(before, after))
PY
codesign --force --sign - --timestamp=none "$capability_app" \
    >/dev/null 2>&1 || fail "performance capability fixture could not be signed"
(
    cd "$capability_payload"
    /usr/bin/ditto -c -k --keepParent --norsrc --noextattr --noqtn \
        --noacl Tanks3D.app "$capability_archive"
)
write_checksum "$capability_archive"
expect_rejection performance-capability-tamper \
    "release performance capability manifest does not match the contract" \
    "$capability_archive" Tanks3D-performance-capability-tamper

capability_timeout_archive="$fixture_root/Tanks3D-performance-capability-timeout.zip"
capability_timeout_payload="$fixture_root/performance-capability-timeout-payload"
mkdir -p "$capability_timeout_payload"
unzip -q "$valid_archive" -d "$capability_timeout_payload"
capability_timeout_app="$capability_timeout_payload/Tanks3D.app"
capability_timeout_executable="$capability_timeout_app/Contents/MacOS/Tanks3D"
clang -arch "$expected_arch" \
    "-mmacosx-version-min=$expected_macos_min" \
    -O0 -x c -o "$capability_timeout_executable" - <<'C'
#include <unistd.h>

int main(void)
{
    for (;;)
        sleep(60);
}
C
codesign --force --sign - --timestamp=none "$capability_timeout_app" \
    >/dev/null 2>&1 || fail "performance capability timeout fixture could not be signed"
(
    cd "$capability_timeout_payload"
    /usr/bin/ditto -c -k --keepParent --norsrc --noextattr --noqtn \
        --noacl Tanks3D.app "$capability_timeout_archive"
)
write_checksum "$capability_timeout_archive"
expect_rejection performance-capability-timeout \
    "release performance capability probe timed out" \
    "$capability_timeout_archive" Tanks3D-performance-capability-timeout

self_test_timeout_archive="$fixture_root/Tanks3D-integrated-self-test-timeout.zip"
self_test_timeout_payload="$fixture_root/integrated-self-test-timeout-payload"
mkdir -p "$self_test_timeout_payload"
unzip -q "$valid_archive" -d "$self_test_timeout_payload"
self_test_timeout_app="$self_test_timeout_payload/Tanks3D.app"
self_test_timeout_executable="$self_test_timeout_app/Contents/MacOS/Tanks3D"
python3 - "$project_root/tests/expected_release_performance_capabilities.json" \
        "$expected_source_commit" "$expected_source_tag" \
        "$self_test_timeout_payload/hanging-self-test.c" <<'PY'
from pathlib import Path
import json
import sys

contract = Path(sys.argv[1]).read_text(encoding="utf-8")
contract = contract.replace("0" * 40, sys.argv[2], 1)
contract = contract.replace('"source_tag": "development"',
                            '"source_tag": ' + json.dumps(sys.argv[3]), 1)
source = """#include <stdio.h>
#include <string.h>
#include <unistd.h>

static const char manifest[] = {manifest};

int main(int argc, char **argv)
{{
    if (argc == 2 && strcmp(argv[1],
            "--self-test=release-performance-capabilities") == 0)
    {{
        fputs(manifest, stdout);
        return 0;
    }}
    for (;;)
        sleep(60);
}}
""".format(manifest=json.dumps(contract))
Path(sys.argv[4]).write_text(source, encoding="utf-8")
PY
clang -arch "$expected_arch" \
    "-mmacosx-version-min=$expected_macos_min" -O0 \
    "$self_test_timeout_payload/hanging-self-test.c" \
    -o "$self_test_timeout_executable"
codesign --force --sign - --timestamp=none "$self_test_timeout_app" \
    >/dev/null 2>&1 || fail "integrated self-test timeout fixture could not be signed"
(
    cd "$self_test_timeout_payload"
    /usr/bin/ditto -c -k --keepParent --norsrc --noextattr --noqtn \
        --noacl Tanks3D.app "$self_test_timeout_archive"
)
write_checksum "$self_test_timeout_archive"
expect_rejection integrated-self-test-timeout \
    "integrated self-test timed out" \
    "$self_test_timeout_archive" Tanks3D-integrated-self-test-timeout

echo "Distribution verifier negative tests passed: 16 rejection cases."
