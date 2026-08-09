#!/bin/sh

set -eu
LC_ALL=C
export LC_ALL
umask 077

if [ "$#" -gt 1 ]; then
    echo "usage: $0 [PROJECT_ROOT]" >&2
    exit 2
fi

project_root=${1:-$(CDPATH= cd "$(dirname "$0")/.." && pwd -P)}
collector="$project_root/scripts/collect_alpha_v2_clean_mac_qa.sh"
[ -f "$collector" ] || {
    echo "collector test failed: collector is missing" >&2
    exit 1
}

temporary_root=$(/usr/bin/mktemp -d "${TMPDIR:-/tmp}/tanks3d-clean-mac-collector-test.XXXXXX")
temporary_root=$(CDPATH= cd -P "$temporary_root" && pwd -P)
cleanup()
{
    status=$?
    trap - 0 1 2 15
    /bin/rm -rf "$temporary_root"
    exit "$status"
}
trap cleanup 0
trap 'exit 1' 1 2 15

fail()
{
    echo "collector test failed: $1" >&2
    exit 1
}

digest()
{
    /usr/bin/shasum -a 256 "$1" | /usr/bin/awk '{print $1}'
}

plist_string()
{
    file=$1
    key=$2
    value=$3
    /usr/bin/plutil -insert "$key" -string "$value" "$file" >/dev/null
}

make_plan()
{
    kit=$1
    candidate_sha=$2
    candidate_name=$3
    url=$4
    /usr/bin/plutil -create xml1 "$kit/clean-mac-plan.plist"
    plist_string "$kit/clean-mac-plan.plist" schema tanks3d-clean-mac-plan-v1
    plist_string "$kit/clean-mac-plan.plist" requirements_profile macos-alpha-v2
    plist_string "$kit/clean-mac-plan.plist" candidate_tag v0.1.0-alpha.test
    plist_string "$kit/clean-mac-plan.plist" candidate_filename "$candidate_name"
    plist_string "$kit/clean-mac-plan.plist" candidate_sha256 "$candidate_sha"
    plist_string "$kit/clean-mac-plan.plist" download_url "$url"
    plist_string "$kit/clean-mac-plan.plist" minimum_macos_version 1.0
    plist_string "$kit/clean-mac-plan.plist" collector_sha256 "$(digest "$kit/START_HERE.command")"
    plist_string "$kit/clean-mac-plan.plist" prepared_at_utc "$(/bin/date -u '+%Y-%m-%dT%H:%M:%SZ')"
    plist_string "$kit/clean-mac-plan.plist" session_nonce 0123456789abcdef0123456789abcdef
    /bin/chmod 600 "$kit/clean-mac-plan.plist"
}

make_where_froms()
{
    fixture_dir=$1
    url=$2
    where_plist="$fixture_dir/where-froms.plist"
    /usr/bin/plutil -create xml1 "$where_plist"
    /usr/libexec/PlistBuddy -c 'Clear array' "$where_plist" >/dev/null
    /usr/libexec/PlistBuddy -c "Add :0 string $url" "$where_plist" >/dev/null
    /usr/bin/xxd -p "$where_plist" "$fixture_dir/where-froms.hex"
    /bin/chmod 600 "$fixture_dir/where-froms.plist" "$fixture_dir/where-froms.hex"
}

make_fake_tools()
{
    tool_dir=$1
    /bin/mkdir -m 700 "$tool_dir"

    /usr/bin/printf '%s\n' \
        '#!/bin/sh' \
        'set -eu' \
        'fixture=${TANKS3D_COLLECTOR_TEST_FIXTURE_DIR:?}' \
        'download=${TANKS3D_COLLECTOR_TEST_DOWNLOAD_DIR:?}' \
        'candidate=${TANKS3D_COLLECTOR_TEST_CANDIDATE_FILENAME:?}' \
        '/bin/cp "$fixture/$candidate" "$download/$candidate"' \
        '/bin/chmod 600 "$download/$candidate"' \
        'exit 0' > "$tool_dir/open"

    /usr/bin/printf '%s\n' \
        '#!/bin/sh' \
        'exec /usr/bin/shasum "$@"' > "$tool_dir/shasum"

    /usr/bin/printf '%s\n' \
        '#!/bin/sh' \
        'set -eu' \
        'fixture=${TANKS3D_COLLECTOR_TEST_FIXTURE_DIR:?}' \
        'if [ "$1" = -px ]; then' \
        '    exec /bin/cat "$fixture/where-froms.hex"' \
        'fi' \
        'target=$3' \
        'if [ "$target" != Tanks3D.app ]; then' \
        '    record="$fixture/zip-quarantine.txt"' \
        '    if [ ! -f "$record" ]; then' \
        '        timestamp=$(/usr/bin/printf "%x" "$(/bin/date +%s)")' \
        '        agent=${TANKS3D_COLLECTOR_TEST_QUARANTINE_AGENT:-Safari}' \
        '        /usr/bin/printf "0083;%s;%s;TEST-UUID\\n" "$timestamp" "$agent" > "$record"' \
        '        /bin/chmod 600 "$record"' \
        '    fi' \
        '    app="$PWD/Tanks3D.app/Contents/MacOS"' \
        '    if [ ! -d "$app" ]; then' \
        '        /bin/mkdir -p "$app"' \
        '        /usr/bin/printf "fixture executable\\n" > "$app/Tanks3D"' \
        '        /bin/chmod 700 "$app/Tanks3D"' \
        '    fi' \
        '    exec /bin/cat "$record"' \
        'fi' \
        'timestamp=$(/usr/bin/printf "%x" "$(/bin/date +%s)")' \
        '/usr/bin/printf "0083;%s;Archive Utility;TEST-UUID\\n" "$timestamp"' \
        > "$tool_dir/xattr"

    /usr/bin/printf '%s\n' \
        '#!/bin/sh' \
        'echo "Tanks3D.app: valid on disk" >&2' \
        'exit 0' > "$tool_dir/codesign"

    /usr/bin/printf '%s\n' \
        '#!/bin/sh' \
        'echo "Tanks3D.app: rejected" >&2' \
        'echo "source=Unnotarized Developer ID" >&2' \
        'exit 1' > "$tool_dir/spctl"

    /bin/chmod 700 "$tool_dir/open" "$tool_dir/shasum" "$tool_dir/xattr" \
        "$tool_dir/codesign" "$tool_dir/spctl"
}

make_case()
{
    case_root=$1
    planned_sha=${2:-actual}
    /bin/mkdir -m 700 "$case_root"
    kit="$case_root/kit"
    download_dir="$case_root/download"
    fixture_dir="$case_root/fixture"
    tool_dir="$case_root/tools"
    /bin/mkdir -m 700 "$kit" "$download_dir" "$fixture_dir"
    /bin/cp "$collector" "$kit/START_HERE.command"
    /bin/chmod 700 "$kit/START_HERE.command"
    candidate_name=Tanks3D-0.1.0-alpha.test-macos-arm64-macos1.0.zip
    candidate="$download_dir/$candidate_name"
    # The fake Safari tool does not download; the harness materializes the
    # exact bytes only after the collector has completed its preflight below.
    staged_candidate="$fixture_dir/$candidate_name"
    /usr/bin/printf 'fixture candidate archive bytes\n' > "$staged_candidate"
    /bin/chmod 600 "$staged_candidate"
    candidate_sha=$(digest "$staged_candidate")
    [ "$planned_sha" = actual ] || candidate_sha=$planned_sha
    encoded_name="%54${candidate_name#T}"
    url="https://github.com/example/tanks3d/releases/download/v0.1.0-alpha.test/$encoded_name"
    make_plan "$kit" "$candidate_sha" "$candidate_name" "$url"
    make_where_froms "$fixture_dir" "$url"
    make_fake_tools "$tool_dir"
}

run_case()
{
    case_root=$1
    output=$2
    quarantine_agent=${3:-Safari}
    kit="$case_root/kit"
    download_dir="$case_root/download"
    fixture_dir="$case_root/fixture"
    tool_dir="$case_root/tools"
    candidate_name=Tanks3D-0.1.0-alpha.test-macos-arm64-macos1.0.zip
    candidate="$download_dir/$candidate_name"
    app="$download_dir/Tanks3D.app"
    set +e
    /usr/bin/printf '%s\n' \
        'Fixture Tester' \
        'Fixture Tester' \
        'Fresh local account with no prior install' \
        YES YES YES YES YES \
        "$download_dir" \
        "$candidate" \
        "$app" \
        'First Finder launch displayed the recorded Gatekeeper dialog.' \
        'macOS cannot verify the developer of Tanks3D.app.' \
        'Finder, then Privacy & Security, Open Anyway, authenticate, Open.' \
        YES YES PASS \
        'Source-free Safari and Gatekeeper workflow completed.' | \
        TANKS3D_COLLECTOR_TEST_MODE=1 \
        TANKS3D_COLLECTOR_TEST_TOOL_DIR="$tool_dir" \
        TANKS3D_COLLECTOR_TEST_FIXTURE_DIR="$fixture_dir" \
        TANKS3D_COLLECTOR_TEST_DOWNLOAD_DIR="$download_dir" \
        TANKS3D_COLLECTOR_TEST_CANDIDATE_FILENAME="$candidate_name" \
        TANKS3D_COLLECTOR_TEST_QUARANTINE_AGENT="$quarantine_agent" \
        /bin/zsh -f "$kit/START_HERE.command" "$output" \
        > "$case_root/collector.stdout" 2> "$case_root/collector.stderr"
    result=$?
    set -e
    return "$result"
}

/bin/zsh -n "$collector" || fail 'zsh syntax validation failed'

success_root="$temporary_root/success"
make_case "$success_root"
success_output="$success_root/evidence"
run_case "$success_root" "$success_output" || {
    /bin/cat "$success_root/collector.stderr" >&2
    fail 'canonical test-mode collection failed'
}

[ -d "$success_output" ] && [ ! -L "$success_output" ] || fail 'output is not a real directory'
[ "$(/usr/bin/stat -f '%Lp' "$success_output")" = 700 ] || fail 'output mode is not 0700'
expected_names='COMPLETE
app-quarantine.stderr
app-quarantine.stdout
checksum.stderr
checksum.stdout
clean-mac-intake.plist
clean-mac-plan.plist
codesign.stderr
codesign.stdout
spctl.stderr
spctl.stdout
where-froms.hex
zip-quarantine.stderr
zip-quarantine.stdout'
actual_names=$(/usr/bin/find "$success_output" -mindepth 1 -maxdepth 1 -print | \
    /usr/bin/awk -F/ '{print $NF}' | LC_ALL=C /usr/bin/sort)
[ "$actual_names" = "$expected_names" ] || fail 'success output file set is not exact'
for output_file in "$success_output"/*; do
    [ -f "$output_file" ] && [ ! -L "$output_file" ] || fail 'output contains a non-regular file'
    [ "$(/usr/bin/stat -f '%Lp' "$output_file")" = 600 ] || fail "output is not 0600: $output_file"
done
[ ! -s "$success_output/COMPLETE" ] || fail 'COMPLETE marker is not empty'
/usr/bin/cmp -s "$success_root/kit/clean-mac-plan.plist" \
    "$success_output/clean-mac-plan.plist" || fail 'copied plan differs'
intake="$success_output/clean-mac-intake.plist"
/usr/bin/plutil -lint "$intake" >/dev/null || fail 'intake plist is invalid'
[ "$(/usr/bin/plutil -extract schema raw -o - "$intake")" = tanks3d-clean-mac-intake-v1 ] || fail 'intake schema drifted'
[ "$(/usr/bin/plutil -extract complete raw -o - "$intake")" = true ] || fail 'intake is not complete'
[ "$(/usr/bin/plutil -extract test_mode raw -o - "$intake")" = true ] || fail 'test intake is not marked test_mode'
[ "$(/usr/bin/plutil -extract commands raw -o - "$intake")" = 5 ] || fail 'intake does not contain five commands'
[ "$(/usr/bin/plutil -extract commands.0.argv.0 raw -o - "$intake")" = /usr/bin/shasum ] || fail 'checksum argv is not absolute'
[ "$(/usr/bin/plutil -extract commands.1.id raw -o - "$intake")" = zip_quarantine ] || fail 'ZIP quarantine command ID is not canonical'
[ "$(/usr/bin/plutil -extract commands.2.id raw -o - "$intake")" = app_quarantine ] || fail 'app quarantine command ID is not canonical'
[ "$(/usr/bin/plutil -extract commands.4.argv.0 raw -o - "$intake")" = /usr/sbin/spctl ] || fail 'spctl argv is not absolute'
[ "$(/usr/bin/plutil -extract acquisition.client raw -o - "$intake")" = Safari ] || fail 'Safari identity is missing'
[ "$(/usr/bin/plutil -extract machine_details.source_checkout_absent raw -o - "$intake")" = yes ] || fail 'clean-machine fact is missing'

intake_before=$(digest "$intake")
if run_case "$success_root" "$success_output"; then
    fail 'collector overwrote an existing output directory'
fi
[ "$(digest "$intake")" = "$intake_before" ] || fail 'no-overwrite failure changed existing intake'

wrong_hash_root="$temporary_root/wrong-hash"
make_case "$wrong_hash_root" "$(/usr/bin/printf '0%.0s' $(/usr/bin/seq 1 64))"
wrong_hash_output="$wrong_hash_root/evidence"
if run_case "$wrong_hash_root" "$wrong_hash_output"; then
    fail 'collector accepted a downloaded ZIP with the wrong SHA-256'
fi
[ ! -e "$wrong_hash_output/COMPLETE" ] || fail 'wrong-hash run published COMPLETE'

wrong_quarantine_root="$temporary_root/wrong-quarantine"
make_case "$wrong_quarantine_root"
wrong_quarantine_output="$wrong_quarantine_root/evidence"
if run_case "$wrong_quarantine_root" "$wrong_quarantine_output" Curl; then
    fail 'collector accepted a non-Safari ZIP quarantine agent'
fi
[ ! -e "$wrong_quarantine_output/COMPLETE" ] || fail 'wrong-quarantine run published COMPLETE'

echo 'Alpha-v2 clean-Mac collector tests passed: 4/4'
