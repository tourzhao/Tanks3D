#!/bin/zsh -f

# Source-free macOS Alpha-v2 Safari/Gatekeeper evidence collector.
# The release preparer copies this file to START_HERE.command beside the
# candidate-bound clean-mac-plan.plist.  This script records observations only;
# it never edits release status or declares a release approved.

emulate -LR zsh
setopt ERR_EXIT NO_UNSET PIPE_FAIL NO_CLOBBER
umask 077

export LC_ALL=C
export LANG=C
export PATH=/usr/bin:/bin:/usr/sbin:/sbin
unset CDPATH ENV BASH_ENV ZDOTDIR
unset -m 'DYLD_*' 2>/dev/null || true

typeset -gr kPlanSchema='tanks3d-clean-mac-plan-v1'
typeset -gr kIntakeSchema='tanks3d-clean-mac-intake-v1'
typeset -gr kRequirementsProfile='macos-alpha-v2'
typeset -gr kMaximumTextBytes=1048576
typeset -gr kExpectedPlanKeys=$'candidate_filename\ncandidate_sha256\ncandidate_tag\ncollector_sha256\ndownload_url\nminimum_macos_version\nprepared_at_utc\nrequirements_profile\nschema\nsession_nonce'
typeset -gr kExpectedOutputNames=$'COMPLETE\napp-quarantine.stderr\napp-quarantine.stdout\nchecksum.stderr\nchecksum.stdout\nclean-mac-intake.plist\nclean-mac-plan.plist\ncodesign.stderr\ncodesign.stdout\nspctl.stderr\nspctl.stdout\nwhere-froms.hex\nzip-quarantine.stderr\nzip-quarantine.stdout'

fail()
{
    print -u2 -r -- "clean-Mac QA collection failed: $1"
    exit 1
}

usage()
{
    print -u2 -r -- 'usage: START_HERE.command <new-output-dir>'
    exit 2
}

utc_now()
{
    /bin/date -u '+%Y-%m-%dT%H:%M:%SZ'
}

require_safe_text()
{
    local value=$1
    local label=$2
    local maximum=${3:-4096}
    [[ -n $value ]] || fail "$label must not be empty"
    (( ${#value} <= maximum )) || fail "$label is too long"
    [[ $value != *$'\n'* && $value != *$'\r'* ]] || \
        fail "$label contains a line break"
    [[ $value != *[[:cntrl:]]* ]] || fail "$label contains a control character"
}

require_safe_token()
{
    local value=$1
    local label=$2
    [[ $value =~ '^[A-Za-z0-9][A-Za-z0-9._-]*$' ]] || \
        fail "$label contains unsafe characters"
}

require_sha256()
{
    [[ $1 =~ '^[0-9a-f]{64}$' ]] || fail "$2 is not a lowercase SHA-256 digest"
}

require_utc_timestamp()
{
    [[ $1 =~ '^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z$' ]] || \
        fail "$2 must use YYYY-MM-DDTHH:MM:SSZ"
    /bin/date -j -u -f '%Y-%m-%dT%H:%M:%SZ' "$1" '+%Y-%m-%dT%H:%M:%SZ' \
        >/dev/null 2>&1 || fail "$2 is not a real UTC timestamp"
}

sha256_file()
{
    local path=$1
    local line digest
    line=$(/usr/bin/shasum -a 256 "$path") || fail "cannot hash $path"
    digest=${line[1,64]}
    require_sha256 "$digest" "SHA-256 for $path"
    print -r -- "$digest"
}

require_no_symlink_components()
{
    local path=$1
    local label=$2
    [[ $path == /* ]] || fail "$label is not absolute"
    local current=''
    local component
    for component in ${(s:/:)path}; do
        [[ -n $component ]] || continue
        current="$current/$component"
        [[ ! -h $current ]] || fail "$label traverses a symbolic link: $current"
    done
}

directory_snapshot()
{
    local path=$1
    [[ -d $path && ! -h $path ]] || fail "directory is missing or symbolic: $path"
    /usr/bin/stat -f '%d:%i:%u:%HT' "$path"
}

regular_file_snapshot()
{
    local path=$1
    local label=$2
    [[ -f $path && ! -h $path ]] || fail "$label must be a regular non-symlink file"
    local metadata
    metadata=$(/usr/bin/stat -f '%d:%i:%z:%m:%c:%u:%l:%HT' "$path") || \
        fail "cannot inspect $label"
    local owner links kind
    owner=$(/usr/bin/stat -f '%u' "$path") || fail "cannot read $label owner"
    links=$(/usr/bin/stat -f '%l' "$path") || fail "cannot read $label link count"
    kind=$(/usr/bin/stat -f '%HT' "$path") || fail "cannot read $label type"
    [[ $owner == $EUID ]] || fail "$label is not owned by the current user"
    [[ $links == 1 ]] || fail "$label must have exactly one hard link"
    [[ $kind == 'Regular File' ]] || fail "$label is not a regular file"
    print -r -- "$metadata"
}

require_private_directory()
{
    local path=$1
    local label=$2
    [[ -d $path && ! -h $path ]] || fail "$label must be a real directory"
    local owner mode
    owner=$(/usr/bin/stat -f '%u' "$path") || fail "cannot inspect $label owner"
    mode=$(/usr/bin/stat -f '%Lp' "$path") || fail "cannot inspect $label mode"
    [[ $owner == $EUID ]] || fail "$label must be owned by the current user"
    [[ $mode == 700 ]] || fail "$label must have mode 0700"
}

version_at_least()
{
    local installed=$1
    local minimum=$2
    [[ $installed =~ '^[0-9]+(\.[0-9]+)*$' ]] || return 1
    [[ $minimum =~ '^[0-9]+(\.[0-9]+)*$' ]] || return 1
    local -a installed_parts minimum_parts
    installed_parts=(${(s:.:)installed})
    minimum_parts=(${(s:.:)minimum})
    local count=${#installed_parts}
    (( ${#minimum_parts} > count )) && count=${#minimum_parts}
    local index installed_value minimum_value
    for (( index = 1; index <= count; ++index )); do
        installed_value=${installed_parts[$index]:-0}
        minimum_value=${minimum_parts[$index]:-0}
        (( installed_value = 10#$installed_value ))
        (( minimum_value = 10#$minimum_value ))
        (( installed_value > minimum_value )) && return 0
        (( installed_value < minimum_value )) && return 1
    done
    return 0
}

typeset ANSWER=''
ask_text()
{
    local prompt=$1
    local label=$2
    local maximum=${3:-4096}
    print -u2 -r -- "$prompt"
    IFS= read -r ANSWER || fail "input ended while reading $label"
    require_safe_text "$ANSWER" "$label" "$maximum"
}

ask_yes()
{
    local prompt=$1
    local label=$2
    print -u2 -r -- "$prompt (type YES)"
    IFS= read -r ANSWER || fail "input ended while reading $label"
    [[ $ANSWER == 'YES' ]] || fail "$label was not explicitly confirmed"
    ANSWER='yes'
}

plan_value()
{
    local key=$1
    /usr/bin/plutil -extract "$key" raw -expect string -o - "$plan_path" 2>/dev/null || \
        fail "plan key $key is missing or is not a string"
}

validate_download_url()
{
    local value=$1
    local filename=$2
    require_safe_text "$value" 'download URL' 4096
    [[ $value == https://* ]] || fail 'download URL must use HTTPS'
    [[ $value != *'?'* && $value != *'#'* ]] || \
        fail 'download URL must not contain a query or fragment'
    local remainder=${value#https://}
    local host=${remainder%%/*}
    local path=${remainder#*/}
    [[ -n $host && $remainder != $host ]] || fail 'download URL has no path'
    [[ $host != *'@'* && $host == *.* ]] || fail 'download URL host is unsafe'
    [[ $host != .* && $host != *. && $host != *..* ]] || fail 'download URL host is malformed'
    [[ $host != *:* || $host == *:443 ]] || fail 'download URL uses a nonstandard port'
    local raw_basename=${path##*/}
    local decoded_basename=''
    local decoded_byte character hex byte_value
    local index=1
    while (( index <= ${#raw_basename} )); do
        character=${raw_basename[$index]}
        if [[ $character == '%' ]]; then
            (( index + 2 <= ${#raw_basename} )) || fail 'download URL has a malformed percent escape'
            hex="${raw_basename[$(( index + 1 ))]}${raw_basename[$(( index + 2 ))]}"
            [[ $hex =~ '^[0-9A-Fa-f]{2}$' ]] || fail 'download URL has a malformed percent escape'
            byte_value=$(/usr/bin/printf '%d' "0x$hex") || fail 'download URL has an invalid percent escape'
            (( byte_value != 0 && byte_value != 47 && byte_value != 92 )) || \
                fail 'download URL encodes NUL or a path separator'
            (( byte_value >= 32 && byte_value != 127 )) || \
                fail 'download URL encodes a control character'
            builtin printf -v decoded_byte '%b' "\\x$hex"
            decoded_basename+=$decoded_byte
            (( index += 3 ))
        else
            decoded_basename+=$character
            (( ++index ))
        fi
    done
    [[ $decoded_basename == $filename ]] || fail 'download URL filename does not match the candidate'
}

read_text_file_exact()
{
    local path=$1
    local label=$2
    local size
    size=$(/usr/bin/stat -f '%z' "$path") || fail "cannot inspect $label"
    (( size <= kMaximumTextBytes )) || fail "$label exceeds the output safety limit"
    local sentinel=$'\x1e'
    local value
    value=$(/bin/cat "$path"; /usr/bin/printf '%s' "$sentinel") || \
        fail "cannot read $label"
    [[ $value == *$sentinel ]] || fail "cannot preserve $label contents"
    REPLY=${value%$sentinel}
}

typeset -A command_exit command_stdout command_stderr command_started command_completed

test_program_for()
{
    local canonical=$1
    local name=${canonical:t}
    local replacement="$test_tool_dir/$name"
    [[ -f $replacement && ! -h $replacement && -x $replacement ]] || \
        fail "test tool is missing or unsafe: $replacement"
    print -r -- "$replacement"
}

run_recorded_command()
{
    local command_id=$1
    local cwd=$2
    shift 2
    local -a recorded_argv actual_argv
    recorded_argv=("$@")
    (( ${#recorded_argv} > 0 )) || fail "internal empty argv for $command_id"
    local actual_program=${recorded_argv[1]}
    if [[ $test_mode == true ]]; then
        actual_program=$(test_program_for "$actual_program")
    fi
    actual_argv=("$actual_program")
    local index
    for (( index = 2; index <= ${#recorded_argv}; ++index )); do
        actual_argv+=("${recorded_argv[$index]}")
    done

    local cwd_before cwd_after
    cwd_before=$(directory_snapshot "$cwd")
    local stdout_path="$output_dir/$command_id.stdout"
    local stderr_path="$output_dir/$command_id.stderr"
    [[ ! -e $stdout_path && ! -h $stdout_path && ! -e $stderr_path && ! -h $stderr_path ]] || \
        fail "command output already exists for $command_id"
    local started completed result
    started=$(utc_now)
    if (
        builtin cd -P -- "$cwd"
        "${actual_argv[@]}"
    ) > "$stdout_path" 2> "$stderr_path"; then
        result=0
    else
        result=$?
    fi
    completed=$(utc_now)
    /bin/chmod 600 "$stdout_path" "$stderr_path"
    cwd_after=$(directory_snapshot "$cwd")
    [[ $cwd_before == $cwd_after ]] || fail "$command_id working directory changed"
    read_text_file_exact "$stdout_path" "$command_id stdout"
    command_stdout[$command_id]=$REPLY
    read_text_file_exact "$stderr_path" "$command_id stderr"
    command_stderr[$command_id]=$REPLY
    command_exit[$command_id]=$result
    command_started[$command_id]=$started
    command_completed[$command_id]=$completed
}

plist_insert_string()
{
    /usr/bin/plutil -insert "$1" -string "$2" "$intake_path" >/dev/null
}

plist_insert_bool()
{
    /usr/bin/plutil -insert "$1" -bool "$2" "$intake_path" >/dev/null
}

plist_insert_integer()
{
    /usr/bin/plutil -insert "$1" -integer "$2" "$intake_path" >/dev/null
}

insert_command_entry()
{
    local array_index=$1
    local command_id=$2
    shift 2
    local base="commands.$array_index"
    local canonical_id=${command_id//-/_}
    /usr/bin/plutil -insert "$base" -dictionary "$intake_path" >/dev/null
    plist_insert_string "$base.id" "$canonical_id"
    /usr/bin/plutil -insert "$base.argv" -array "$intake_path" >/dev/null
    local argv_index=0
    local argument
    for argument in "$@"; do
        plist_insert_string "$base.argv.$argv_index" "$argument"
        (( ++argv_index ))
    done
    plist_insert_integer "$base.exit_code" "${command_exit[$command_id]}"
    plist_insert_string "$base.stdout" "${command_stdout[$command_id]}"
    plist_insert_string "$base.stderr" "${command_stderr[$command_id]}"
    plist_insert_string "$base.started_at_utc" "${command_started[$command_id]}"
    plist_insert_string "$base.completed_at_utc" "${command_completed[$command_id]}"
}

[[ $# == 1 ]] || usage

typeset test_mode=false
typeset test_tool_dir=''
case ${TANKS3D_COLLECTOR_TEST_MODE:-} in
    '') ;;
    1)
        test_mode=true
        test_tool_dir=${TANKS3D_COLLECTOR_TEST_TOOL_DIR:-}
        [[ $test_tool_dir == /* ]] || fail 'test tool directory must be absolute'
        require_no_symlink_components "$test_tool_dir" 'test tool directory'
        require_private_directory "$test_tool_dir" 'test tool directory'
        ;;
    *) fail 'TANKS3D_COLLECTOR_TEST_MODE must be unset or 1' ;;
esac

typeset script_argument=${0:a}
require_no_symlink_components "$script_argument" 'collector path'
[[ -f $script_argument && ! -h $script_argument ]] || fail 'collector must be a regular non-symlink file'
typeset script_path=${script_argument:A}
typeset script_dir=${script_path:h}
typeset plan_path="$script_dir/clean-mac-plan.plist"
require_no_symlink_components "$plan_path" 'plan path'
[[ -f $plan_path && ! -h $plan_path ]] || fail 'clean-mac-plan.plist is missing beside the collector'
/usr/bin/plutil -lint "$plan_path" >/dev/null || fail 'clean-mac plan is not a valid plist'

typeset plan_xml actual_plan_keys
plan_xml=$(/usr/bin/plutil -convert xml1 -o - "$plan_path") || fail 'cannot normalize clean-mac plan'
actual_plan_keys=$(print -r -- "$plan_xml" | \
    /usr/bin/sed -n 's/^[[:space:]]*<key>\([^<]*\)<\/key>[[:space:]]*$/\1/p' | \
    LC_ALL=C /usr/bin/sort)
[[ $actual_plan_keys == $kExpectedPlanKeys ]] || fail 'clean-mac plan keys are missing, duplicated, nested, or unexpected'

typeset plan_schema requirements_profile candidate_tag candidate_filename
typeset candidate_sha256 download_url minimum_macos_version
typeset planned_collector_sha256 prepared_at_utc session_nonce
plan_schema=$(plan_value schema)
requirements_profile=$(plan_value requirements_profile)
candidate_tag=$(plan_value candidate_tag)
candidate_filename=$(plan_value candidate_filename)
candidate_sha256=$(plan_value candidate_sha256)
download_url=$(plan_value download_url)
minimum_macos_version=$(plan_value minimum_macos_version)
planned_collector_sha256=$(plan_value collector_sha256)
prepared_at_utc=$(plan_value prepared_at_utc)
session_nonce=$(plan_value session_nonce)

[[ $plan_schema == $kPlanSchema ]] || fail 'clean-mac plan has the wrong schema'
[[ $requirements_profile == $kRequirementsProfile ]] || fail 'clean-mac plan has the wrong requirements profile'
require_safe_token "$candidate_tag" 'candidate tag'
require_safe_token "$candidate_filename" 'candidate filename'
[[ $candidate_filename == *.zip ]] || fail 'candidate filename must end in .zip'
require_sha256 "$candidate_sha256" 'candidate SHA-256'
require_sha256 "$planned_collector_sha256" 'planned collector SHA-256'
[[ $minimum_macos_version =~ '^[0-9]+(\.[0-9]+)*$' ]] || fail 'minimum macOS version is invalid'
require_utc_timestamp "$prepared_at_utc" 'prepared_at_utc'
[[ $session_nonce =~ '^([0-9a-f]{32}|[0-9a-f]{64})$' ]] || fail 'session nonce is invalid'
validate_download_url "$download_url" "$candidate_filename"

typeset collector_sha256 plan_sha256
collector_sha256=$(sha256_file "$script_path")
[[ $collector_sha256 == $planned_collector_sha256 ]] || fail 'collector SHA-256 does not match the plan'
plan_sha256=$(sha256_file "$plan_path")

typeset raw_output=$1
require_safe_text "$raw_output" 'output directory path' 4096
[[ $raw_output == /* ]] || raw_output="$PWD/$raw_output"
typeset output_name=${raw_output:t}
typeset output_parent_argument=${raw_output:h}
[[ -n $output_name && $output_name != '.' && $output_name != '..' ]] || fail 'output directory name is invalid'
require_no_symlink_components "$output_parent_argument" 'output parent path'
[[ -d $output_parent_argument && ! -h $output_parent_argument ]] || fail 'output parent must already be a real directory'
typeset output_parent
output_parent=$(builtin cd -P -- "$output_parent_argument" && pwd -P) || fail 'cannot normalize output parent'
typeset output_dir="$output_parent/$output_name"
[[ ! -e $output_dir && ! -h $output_dir ]] || fail 'output directory already exists; refusing to overwrite it'
/bin/mkdir -m 700 "$output_dir" || fail 'cannot create output directory'
require_private_directory "$output_dir" 'output directory'

typeset output_complete=false
collector_exit_handler()
{
    local collector_exit_code=$1
    if [[ $output_complete != true && -n ${output_dir:-} && -d ${output_dir:-} ]]; then
        print -u2 -r -- "Incomplete evidence remains at $output_dir (no COMPLETE marker)."
    fi
    return $collector_exit_code
}
trap 'collector_exit_handler $?' EXIT

/bin/cp "$plan_path" "$output_dir/clean-mac-plan.plist"
/bin/chmod 600 "$output_dir/clean-mac-plan.plist"
[[ $(sha256_file "$output_dir/clean-mac-plan.plist") == $plan_sha256 ]] || fail 'copied plan changed'

ask_text 'Enter the tester name:' tester 200
typeset tester=$ANSWER
ask_text 'Type the same tester name as the tester signature:' tester_signature 200
typeset tester_signature=$ANSWER
[[ $tester_signature == $tester ]] || fail 'tester signature must exactly match the tester name'

typeset mac_model chip uname_machine ram macos_version macos_build
if [[ $test_mode == true ]]; then
    mac_model=${TANKS3D_COLLECTOR_TEST_MAC_MODEL:-'Test Mac'}
    chip=${TANKS3D_COLLECTOR_TEST_CHIP:-'Apple Test Chip'}
    uname_machine=${TANKS3D_COLLECTOR_TEST_UNAME_MACHINE:-arm64}
    ram=${TANKS3D_COLLECTOR_TEST_RAM:-'16 GB'}
    macos_version=${TANKS3D_COLLECTOR_TEST_MACOS_VERSION:-'26.0'}
    macos_build=${TANKS3D_COLLECTOR_TEST_MACOS_BUILD:-'25A000'}
else
    typeset hardware_text ram_bytes
    hardware_text=$(/usr/sbin/system_profiler SPHardwareDataType -detailLevel mini) || fail 'system_profiler failed'
    mac_model=$(print -r -- "$hardware_text" | /usr/bin/awk -F': ' '/Model Name:/ {print $2; exit}')
    chip=$(print -r -- "$hardware_text" | /usr/bin/awk -F': ' '/Chip:/ {print $2; exit}')
    uname_machine=$(/usr/bin/uname -m) || fail 'uname failed'
    ram_bytes=$(/usr/sbin/sysctl -n hw.memsize) || fail 'sysctl hw.memsize failed'
    [[ $ram_bytes =~ '^[0-9]+$' ]] || fail 'RAM query returned a nonnumeric value'
    ram="$ram_bytes bytes"
    macos_version=$(/usr/bin/sw_vers -productVersion) || fail 'sw_vers product version failed'
    macos_build=$(/usr/bin/sw_vers -buildVersion) || fail 'sw_vers build version failed'
fi
for value_label in \
    "$mac_model|mac model" "$chip|chip" "$uname_machine|uname machine" \
    "$ram|RAM" "$macos_version|macOS version" "$macos_build|macOS build"; do
    require_safe_text "${value_label%%|*}" "${value_label#*|}" 512
done
[[ $uname_machine == arm64 ]] || fail 'clean-Mac QA requires an arm64 machine'
[[ ${chip:l} == *apple* ]] || fail 'clean-Mac QA requires Apple Silicon'
version_at_least "$macos_version" "$minimum_macos_version" || fail 'installed macOS is below the candidate minimum'
typeset machine="$mac_model; $chip; $uname_machine; macOS $macos_version ($macos_build)"

ask_text 'Describe the fresh-account or clean-machine method:' clean_machine_method 1000
typeset clean_machine_method=$ANSWER
ask_yes 'Confirm Tanks3D.app was absent before this session.' prior_app_absent
typeset prior_app_absent=$ANSWER
ask_yes 'Confirm no prior Gatekeeper approval existed for this candidate.' prior_approval_absent
typeset prior_approval_absent=$ANSWER
ask_yes "Confirm macOS $macos_version meets the planned minimum $minimum_macos_version." minimum_macos_met
typeset minimum_macos_met=$ANSWER
ask_yes 'Confirm no Tanks3D source checkout is present on this test account.' source_checkout_absent
typeset source_checkout_absent=$ANSWER
ask_yes 'Confirm Homebrew raylib is not used by this downloaded app.' homebrew_raylib_unused
typeset homebrew_raylib_unused=$ANSWER

ask_text 'Enter the real 0700 Safari download directory to use for this session:' download_directory 4096
typeset download_directory=$ANSWER
[[ $download_directory == /* ]] || fail 'Safari download directory must be absolute'
require_no_symlink_components "$download_directory" 'Safari download directory'
require_private_directory "$download_directory" 'Safari download directory'
typeset expected_zip="$download_directory/$candidate_filename"
typeset expected_partial="$expected_zip.download"
typeset expected_app="$download_directory/Tanks3D.app"
[[ ! -e $expected_zip && ! -h $expected_zip ]] || fail 'candidate ZIP existed before Safari acquisition'
[[ ! -e $expected_partial && ! -h $expected_partial ]] || fail 'partial Safari download existed before acquisition'
[[ ! -e $expected_app && ! -h $expected_app ]] || fail 'Tanks3D.app existed before extraction'

typeset session_started_at_utc acquisition_started_at_utc
session_started_at_utc=$(utc_now)
acquisition_started_at_utc=$(utc_now)
print -u2 -r -- "Opening the exact candidate URL in Safari: $download_url"
typeset open_program=/usr/bin/open
[[ $test_mode == true ]] && open_program=$(test_program_for /usr/bin/open)
"$open_program" -b com.apple.Safari "$download_url" >/dev/null 2>&1 || fail 'Safari could not be opened for the candidate URL'
print -u2 -r -- 'Complete the Safari download without copying another ZIP or changing quarantine metadata.'
ask_text 'After Safari finishes, enter the exact downloaded ZIP path:' downloaded_zip 4096
typeset downloaded_zip=$ANSWER
[[ $downloaded_zip == "$expected_zip" ]] || fail 'entered ZIP path is not the planned fresh download path'
[[ ! -e $expected_partial && ! -h $expected_partial ]] || fail 'Safari download is still partial'
require_no_symlink_components "$downloaded_zip" 'downloaded ZIP path'
typeset zip_snapshot zip_sha256
zip_snapshot=$(regular_file_snapshot "$downloaded_zip" 'downloaded candidate ZIP')
zip_sha256=$(sha256_file "$downloaded_zip")
[[ $zip_sha256 == $candidate_sha256 ]] || fail 'downloaded candidate SHA-256 does not match the plan'
[[ $(regular_file_snapshot "$downloaded_zip" 'downloaded candidate ZIP') == $zip_snapshot ]] || fail 'downloaded ZIP changed while it was hashed'

typeset where_froms_path="$output_dir/where-froms.hex"
typeset where_froms_program=/usr/bin/xattr
[[ $test_mode == true ]] && where_froms_program=$(test_program_for /usr/bin/xattr)
if ! (
    builtin cd -P -- "$download_directory"
    "$where_froms_program" -px com.apple.metadata:kMDItemWhereFroms "$candidate_filename"
) > "$where_froms_path" 2>/dev/null; then
    fail 'Safari origin metadata is missing from the downloaded ZIP'
fi
/bin/chmod 600 "$where_froms_path"
[[ -s $where_froms_path ]] || fail 'Safari origin metadata is empty'
typeset where_froms_sha256
where_froms_sha256=$(sha256_file "$where_froms_path")
typeset decoded_where_froms="$output_dir/.where-froms.decoded.plist"
/usr/bin/xxd -r -p "$where_froms_path" "$decoded_where_froms" || fail 'where-froms hex is malformed'
/bin/chmod 600 "$decoded_where_froms"
/usr/bin/plutil -lint "$decoded_where_froms" >/dev/null || fail 'where-froms data is not a plist'
typeset where_root
where_root=$(/usr/libexec/PlistBuddy -c Print "$decoded_where_froms" 2>/dev/null) || fail 'cannot read where-froms plist'
[[ $where_root == 'Array {'* ]] || fail 'where-froms metadata must be an array'
typeset where_index=0 where_value where_match=false
while where_value=$(/usr/bin/plutil -extract "$where_index" raw -expect string -o - "$decoded_where_froms" 2>/dev/null); do
    [[ $where_value == $download_url ]] && where_match=true
    (( ++where_index ))
done
(( where_index > 0 )) || fail 'where-froms metadata contains no URLs'
[[ $where_match == true ]] || fail 'where-froms metadata does not contain the exact candidate URL'
/bin/rm -f "$decoded_where_froms"

typeset quarantine_probe="$output_dir/.zip-quarantine.probe"
typeset quarantine_program=/usr/bin/xattr
[[ $test_mode == true ]] && quarantine_program=$(test_program_for /usr/bin/xattr)
if ! (
    builtin cd -P -- "$download_directory"
    "$quarantine_program" -p com.apple.quarantine "$candidate_filename"
) > "$quarantine_probe" 2>/dev/null; then
    fail 'downloaded ZIP has no quarantine metadata'
fi
/bin/chmod 600 "$quarantine_probe"
read_text_file_exact "$quarantine_probe" 'ZIP quarantine probe'
typeset zip_quarantine=${REPLY%$'\n'}
[[ $zip_quarantine != *$'\n'* && $zip_quarantine != *$'\r'* ]] || fail 'ZIP quarantine record is not one line'
typeset -a quarantine_fields
quarantine_fields=(${(s:;:)zip_quarantine})
(( ${#quarantine_fields} >= 3 && ${#quarantine_fields} <= 4 )) || fail 'ZIP quarantine record has invalid fields'
[[ ${quarantine_fields[1]} =~ '^[0-9A-Fa-f]{4}$' ]] || fail 'ZIP quarantine flags are invalid'
[[ ${quarantine_fields[2]} =~ '^[0-9A-Fa-f]{8,16}$' ]] || fail 'ZIP quarantine timestamp is invalid'
[[ ${quarantine_fields[3]} == Safari ]] || fail 'ZIP quarantine agent must be Safari'
typeset quarantine_epoch quarantine_timestamp_utc acquisition_completed_at_utc
quarantine_epoch=$(/usr/bin/printf '%d' "0x${quarantine_fields[2]}") || fail 'cannot decode ZIP quarantine timestamp'
quarantine_timestamp_utc=$(/bin/date -u -r "$quarantine_epoch" '+%Y-%m-%dT%H:%M:%SZ') || fail 'cannot format ZIP quarantine timestamp'
acquisition_completed_at_utc=$(utc_now)
[[ $quarantine_timestamp_utc < $acquisition_started_at_utc ]] && fail 'ZIP quarantine timestamp predates Safari acquisition'
[[ $quarantine_timestamp_utc > $acquisition_completed_at_utc ]] && fail 'ZIP quarantine timestamp follows Safari acquisition'
/bin/rm -f "$quarantine_probe"
[[ $(regular_file_snapshot "$downloaded_zip" 'downloaded candidate ZIP') == $zip_snapshot ]] || fail 'downloaded ZIP changed during Safari metadata validation'

run_recorded_command checksum "$download_directory" \
    /usr/bin/shasum -a 256 "$candidate_filename"
[[ ${command_exit[checksum]} == 0 ]] || fail 'candidate checksum command failed'
typeset checksum_text=${command_stdout[checksum]%$'\n'}
[[ $checksum_text == "$candidate_sha256  $candidate_filename" ]] || fail 'checksum output does not exactly identify the candidate'
[[ $(regular_file_snapshot "$downloaded_zip" 'downloaded candidate ZIP') == $zip_snapshot ]] || fail 'downloaded ZIP changed during checksum verification'

run_recorded_command zip-quarantine "$download_directory" \
    /usr/bin/xattr -p com.apple.quarantine "$candidate_filename"
[[ ${command_exit[zip-quarantine]} == 0 ]] || fail 'ZIP quarantine command failed'
typeset recorded_zip_quarantine=${command_stdout[zip-quarantine]%$'\n'}
[[ $recorded_zip_quarantine == $zip_quarantine ]] || fail 'ZIP quarantine changed between probe and canonical command'
[[ $(regular_file_snapshot "$downloaded_zip" 'downloaded candidate ZIP') == $zip_snapshot ]] || fail 'downloaded ZIP changed during quarantine verification'

print -u2 -r -- 'Now extract this exact ZIP with Finder/Archive Utility. Do not use Terminal, clear quarantine, or replace the app.'
ask_text 'After Finder/Archive Utility finishes, enter the exact Tanks3D.app path:' extracted_app 4096
typeset extracted_app=$ANSWER
[[ $extracted_app == "$expected_app" ]] || fail 'entered app path is not the expected Finder extraction path'
require_no_symlink_components "$extracted_app" 'extracted app path'
[[ -d $extracted_app && ! -h $extracted_app ]] || fail 'Tanks3D.app is not a real directory'
[[ $(/usr/bin/stat -f '%u' "$extracted_app") == $EUID ]] || fail 'Tanks3D.app is not owned by the current user'
typeset unexpected_bundle_entry
unexpected_bundle_entry=$(/usr/bin/find "$extracted_app" \( -type l -o \( ! -type d ! -type f \) \) -print -quit) || fail 'cannot inspect extracted app bundle'
[[ -z $unexpected_bundle_entry ]] || fail 'Tanks3D.app contains a symlink or special file'
typeset app_executable="$extracted_app/Contents/MacOS/Tanks3D"
[[ -f $app_executable && ! -h $app_executable && -x $app_executable ]] || fail 'Tanks3D.app executable is missing or unsafe'
typeset app_parent=${extracted_app:h}
typeset app_directory_snapshot
app_directory_snapshot=$(directory_snapshot "$extracted_app")

run_recorded_command app-quarantine "$app_parent" \
    /usr/bin/xattr -p com.apple.quarantine Tanks3D.app
[[ ${command_exit[app-quarantine]} == 0 ]] || fail 'app quarantine command failed'
typeset app_quarantine=${command_stdout[app-quarantine]%$'\n'}
[[ $app_quarantine != *$'\n'* && $app_quarantine != *$'\r'* ]] || fail 'app quarantine output is not one line'
typeset -a app_quarantine_fields
app_quarantine_fields=(${(s:;:)app_quarantine})
(( ${#app_quarantine_fields} >= 3 && ${#app_quarantine_fields} <= 4 )) || fail 'app quarantine output has invalid fields'
[[ ${app_quarantine_fields[1]} =~ '^[0-9A-Fa-f]{4}$' ]] || fail 'app quarantine flags are invalid'
[[ -n ${app_quarantine_fields[2]} && -n ${app_quarantine_fields[3]} ]] || fail 'app quarantine fields are empty'
[[ $(directory_snapshot "$extracted_app") == $app_directory_snapshot ]] || fail 'Tanks3D.app was replaced during quarantine verification'

run_recorded_command codesign "$app_parent" \
    /usr/bin/codesign --verify --deep --strict --verbose=4 Tanks3D.app
[[ ${command_exit[codesign]} == 0 ]] || fail 'codesign verification failed'
[[ $(directory_snapshot "$extracted_app") == $app_directory_snapshot ]] || fail 'Tanks3D.app was replaced during signature verification'

run_recorded_command spctl "$app_parent" \
    /usr/sbin/spctl --assess --type execute --verbose=4 Tanks3D.app
typeset spctl_text="${command_stdout[spctl]}\n${command_stderr[spctl]}"
typeset spctl_lower=${spctl_text:l}
typeset spctl_has_accepted=false spctl_has_rejected=false
[[ $spctl_lower == *accepted* ]] && spctl_has_accepted=true
[[ $spctl_lower == *rejected* ]] && spctl_has_rejected=true
[[ $spctl_has_accepted != $spctl_has_rejected ]] || fail 'spctl output must state exactly one of accepted or rejected'
if [[ ${command_exit[spctl]} == 0 ]]; then
    [[ $spctl_has_accepted == true ]] || fail 'spctl exit 0 contradicts its output'
else
    [[ $spctl_has_rejected == true ]] || fail 'spctl rejection exit contradicts its output'
fi
[[ $(directory_snapshot "$extracted_app") == $app_directory_snapshot ]] || fail 'Tanks3D.app was replaced during Gatekeeper assessment'

print -u2 -r -- 'Use Finder for the first app launch. If blocked, use only System Settings > Privacy & Security > Open Anyway, then confirm Open. Never disable Gatekeeper or remove quarantine.'
ask_text 'Describe the observed first Finder launch:' first_finder_launch 2000
typeset first_finder_launch=$ANSWER
ask_text 'Enter the exact Gatekeeper dialog text observed:' dialog_text 4000
typeset dialog_text=$ANSWER
ask_text 'Describe the exact documented Finder/Open Anyway launch path used:' documented_launch_path 3000
typeset documented_launch_path=$ANSWER
ask_yes 'Confirm the downloaded app reached its main menu.' main_menu_reached
typeset main_menu_reached=$ANSWER
ask_yes 'Confirm the app signature remained intact and no bundle files were replaced.' signature_preserved
typeset signature_preserved=$ANSWER
ask_text 'Enter the observed Gatekeeper conclusion (must be PASS):' conclusion 100
typeset conclusion=$ANSWER
[[ $conclusion == PASS ]] || fail 'Gatekeeper conclusion must explicitly be PASS'
ask_text 'Enter concise session notes:' notes 4000
typeset notes=$ANSWER
typeset session_completed_at_utc
session_completed_at_utc=$(utc_now)

typeset intake_path="$output_dir/clean-mac-intake.plist"
[[ ! -e $intake_path && ! -h $intake_path ]] || fail 'intake output already exists'
/usr/bin/plutil -create xml1 "$intake_path"
/bin/chmod 600 "$intake_path"
plist_insert_string schema "$kIntakeSchema"
plist_insert_string plan_sha256 "$plan_sha256"
plist_insert_string session_nonce "$session_nonce"
plist_insert_string collector_sha256 "$collector_sha256"
plist_insert_string candidate_filename "$candidate_filename"
plist_insert_string candidate_sha256 "$candidate_sha256"
plist_insert_string download_url "$download_url"
plist_insert_string tester "$tester"
plist_insert_string tester_signature "$tester_signature"
plist_insert_string machine "$machine"

/usr/bin/plutil -insert machine_details -dictionary "$intake_path" >/dev/null
plist_insert_string machine_details.mac_model "$mac_model"
plist_insert_string machine_details.chip "$chip"
plist_insert_string machine_details.uname_machine "$uname_machine"
plist_insert_string machine_details.ram "$ram"
plist_insert_string machine_details.macos_version "$macos_version"
plist_insert_string machine_details.macos_build "$macos_build"
plist_insert_string machine_details.clean_machine_method "$clean_machine_method"
plist_insert_string machine_details.prior_app_absent "$prior_app_absent"
plist_insert_string machine_details.prior_approval_absent "$prior_approval_absent"
plist_insert_string machine_details.minimum_macos_met "$minimum_macos_met"
plist_insert_string machine_details.source_checkout_absent "$source_checkout_absent"
plist_insert_string machine_details.homebrew_raylib_unused "$homebrew_raylib_unused"

plist_insert_string session_started_at_utc "$session_started_at_utc"
plist_insert_string session_completed_at_utc "$session_completed_at_utc"
/usr/bin/plutil -insert acquisition -dictionary "$intake_path" >/dev/null
plist_insert_string acquisition.client Safari
plist_insert_string acquisition.started_at_utc "$acquisition_started_at_utc"
plist_insert_string acquisition.completed_at_utc "$acquisition_completed_at_utc"
plist_insert_string acquisition.zip_quarantine_agent Safari
plist_insert_string acquisition.quarantine_timestamp_utc "$quarantine_timestamp_utc"
plist_insert_string acquisition.where_froms_url "$download_url"
plist_insert_string acquisition.where_froms_sha256 "$where_froms_sha256"

/usr/bin/plutil -insert commands -array "$intake_path" >/dev/null
insert_command_entry 0 checksum \
    /usr/bin/shasum -a 256 "$candidate_filename"
insert_command_entry 1 zip-quarantine \
    /usr/bin/xattr -p com.apple.quarantine "$candidate_filename"
insert_command_entry 2 app-quarantine \
    /usr/bin/xattr -p com.apple.quarantine Tanks3D.app
insert_command_entry 3 codesign \
    /usr/bin/codesign --verify --deep --strict --verbose=4 Tanks3D.app
insert_command_entry 4 spctl \
    /usr/sbin/spctl --assess --type execute --verbose=4 Tanks3D.app

/usr/bin/plutil -insert observations -dictionary "$intake_path" >/dev/null
plist_insert_string observations.first_finder_launch "$first_finder_launch"
plist_insert_string observations.dialog_text "$dialog_text"
plist_insert_string observations.documented_launch_path "$documented_launch_path"
plist_insert_string observations.main_menu_reached "$main_menu_reached"
plist_insert_string observations.signature_preserved "$signature_preserved"
plist_insert_string observations.conclusion "$conclusion"
plist_insert_string notes "$notes"
plist_insert_bool complete YES
if [[ $test_mode == true ]]; then
    plist_insert_bool test_mode YES
else
    plist_insert_bool test_mode NO
fi
/usr/bin/plutil -convert xml1 "$intake_path" >/dev/null
/usr/bin/plutil -lint "$intake_path" >/dev/null || fail 'generated intake plist is invalid'
/bin/chmod 600 "$intake_path"

typeset actual_output_names
actual_output_names=$(/usr/bin/find "$output_dir" -mindepth 1 -maxdepth 1 -print | \
    /usr/bin/awk -F/ '{print $NF}' | LC_ALL=C /usr/bin/sort)
typeset expected_before_complete=${kExpectedOutputNames#*$'\n'}
expected_before_complete=${expected_before_complete//$'\nCOMPLETE'/}
# Compare explicitly without COMPLETE, which is published last.
typeset expected_without_complete
expected_without_complete=$(print -r -- "$kExpectedOutputNames" | /usr/bin/sed '/^COMPLETE$/d')
[[ $actual_output_names == $expected_without_complete ]] || fail 'collector output file set is not canonical before completion'

typeset output_entry
for output_entry in "$output_dir"/*; do
    [[ -f $output_entry && ! -h $output_entry ]] || fail 'collector output contains a non-regular entry'
    /bin/chmod 600 "$output_entry"
done

: > "$output_dir/COMPLETE"
/bin/chmod 600 "$output_dir/COMPLETE"
[[ ! -s "$output_dir/COMPLETE" ]] || fail 'COMPLETE marker must be empty'
output_complete=true
print -r -- "Clean-Mac QA intake collected: $output_dir/clean-mac-intake.plist"
print -r -- 'This is raw evidence only; repository compilation and independent review are still required.'
