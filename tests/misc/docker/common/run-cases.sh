#!/bin/sh
# Walk the cases.tsv files and press the keys.  Runs inside the mc container.
#
# usage: run-cases.sh [-c subject] [-w transports] [-l locale] [-o [sec.]key=val]...
#                     [-k keymap] [-r report] [-v] [case-dir...]
#
# Each row of a cases.tsv is "file, keys, expect, why, transports".  mc is
# started under tmux with the panel in that directory, the file is found by
# quick search, the keys are pressed in order and the screen is read.  A key
# is Enter, F3, F5, C-o, ".." (up one level), "on <name>" (the cursor goes
# there), "cd <path>" (through the Quick cd box), "type <text>".  Rows whose
# keys or expectation this script does not know are reported as skipped; a
# row with transports named runs only over those.
#
#   -c   the subject under cases/ (default archives)
#   -w   comma separated: local, sftp, ftp, smb, sh (default local)
#   -l   locale mc runs in; messages stay English (default ru_RU.UTF-8)
#   -o   an ini value written before mc starts, section Midnight-Commander
#        unless given: -o old_esc_mode=true -o Layout.message_visible=false
#   -k   a keymap from common/keymaps/ put in place as mc.keymap
#   -r   report directory (default /reports/<stamp>-<env>)
#
# The environment's envs/<env>/expect.tsv overrides expectations: columns
# "dir/file, key, expect, transports" (the last one optional, comma separated);
# "*" in the first two stands for any case or any key.
# An expectation "known: <why>" marks a failure that is understood: the case
# is still run, a failure is reported as "known", and a pass as "FIXED".
#
# What must come of it: "archive panel", "listing", "error dialog",
# "nothing, no error", "the panel it came from", "extfs panel", "copy to the
# other panel" (the file is then in /tmp, the same size), "the name as
# written" (the file's name is on the screen, in the shell's output).  mc's
# stderr is read too: an assertion or a critical warning fails the case
# whatever the screen says.
set -u

MC=/work/opt/mc/bin/mc
SRC=/src/tests/misc/docker
env_name=${SANDBOX_ENV:-unknown}
subject=archives
transports=local
locale=ru_RU.UTF-8
keymap=
report=
verbose=0
options=

while getopts "c:w:l:o:k:r:v" opt; do
    case "$opt" in
    c) subject=$OPTARG ;;
    w) transports=$OPTARG ;;
    l) locale=$OPTARG ;;
    o) options="$options
$OPTARG" ;;
    k) keymap=$OPTARG ;;
    r) report=$OPTARG ;;
    v) verbose=1 ;;
    *) exit 2 ;;
    esac
done
shift $((OPTIND - 1))

[ -x "$MC" ] || { echo "run-cases.sh: no mc at $MC, run build first" >&2; exit 2; }
[ -d "/work/local/$subject" ] || { echo "run-cases.sh: no local fixtures for $subject" >&2; exit 2; }

stamp=$(date +%Y-%m-%dT%H-%M-%S)
[ -n "$report" ] || report=/reports/$stamp-$env_name
mkdir -p "$report"
index=$report/index.md
expect_file=$SRC/envs/$env_name/expect.tsv

# ------------------------------------------------------------- mc's config ---

config=${XDG_CONFIG_HOME:-$HOME/.config}/mc
mkdir -p "$config"

# -o values, grouped by section
if [ -n "$options" ]; then
    echo "$options" | grep . | awk -F= '
    {
        key = $1; sub(/^[^=]*=/, "", $0); val = $0
        sec = "Midnight-Commander"
        if (index(key, ".") > 0) { sec = substr(key, 1, index(key, ".") - 1); key = substr(key, index(key, ".") + 1) }
        if (!(sec in seen)) { order[++n] = sec; seen[sec] = 1 }
        body[sec] = body[sec] key "=" val "\n"
    }
    END { for (i = 1; i <= n; i++) printf "[%s]\n%s\n", order[i], body[order[i]] }' > "$config/ini"
fi

if [ -n "$keymap" ]; then
    cp "$SRC/common/keymaps/$keymap.keymap" "$config/mc.keymap" \
        || { echo "run-cases.sh: no keymap $keymap in common/keymaps" >&2; exit 2; }
fi

# the connection each plugin reads on start; plain passwords are accepted
printf '[sandbox]\nhost=remote\nuser=mc\npassword=mc\npath=/home/mc/cases/%s\nuse_agent=false\n' "$subject" \
    > "$config/sftp-connections.ini"
printf '[sandbox]\nhost=remote\nuser=mc\npassword=mc\npath=/home/mc/cases/%s\n' "$subject" \
    > "$config/shell-connections.ini"
printf '[sandbox]\nhost=remote\nuser=mc\npassword=mc\npath=/cases/%s\n' "$subject" \
    > "$config/ftp-connections.ini"
printf '[sandbox]\nserver=remote\nshare=cases\nusername=mc\npassword=mc\n' \
    > "$config/smb-connections.ini"
# shell-link's libssh2 transport insists on a known_hosts file, and a host it
# does not know is a question; the answer is put there beforehand
mkdir -p "$HOME/.ssh" && chmod 700 "$HOME/.ssh"
case ",$transports," in
*,sh,*) ssh-keyscan remote > "$HOME/.ssh/known_hosts" 2>/dev/null ;;
esac

# ------------------------------------------------------------------ tmux ---

T="tmux -L cases"
screen ()
{
    $T capture-pane -t mc -p
}

# wait until the screen shows $1, up to $2 seconds
wait_for ()
{
    n=$(( ${2:-10} * 5 ))
    while [ "$n" -gt 0 ]; do
        screen | grep -qF -- "$1" && return 0
        sleep 0.2
        n=$((n - 1))
    done
    return 1
}

# quick search puts the cursor on a name; the key that follows ends the search
select_entry ()
{
    $T send-keys -t mc C-s
    sleep 0.2
    $T send-keys -t mc -l "$1"
    sleep 0.3
}

# start mc with the panel in case directory $1 over transport $2; stderr to $3
start_mc ()
{
    $T kill-server 2>/dev/null
    case "$2" in
    local) open_path=/work/local/$subject/$1 ;;
    *) open_path="$2:" ;;
    esac
    # messages in English so that the screen can be read, the charset as asked
    $T new-session -d -s mc -x 120 -y 40 \
        "env -u LC_ALL LANG=$locale LC_CTYPE=$locale LC_MESSAGES=en_US.UTF-8 TERM=xterm-256color \
         $MC -S default '$open_path' /tmp 2>'$3'"
    case "$2" in
    local)
        wait_for cases.tsv 10 || return 1
        ;;
    *)
        # the plugin lists its connections; Enter on ours opens the subject
        # a listing shows up before the panel takes keys again, hence the sleeps
        wait_for sandbox 10 || return 1
        select_entry sandbox
        $T send-keys -t mc Enter
        wait_for "$subject" 30 || return 1
        sleep 1
        # the samba share is the whole cases tree; the others land in the subject
        if [ "$2" = smb ]; then
            select_entry "$subject"
            $T send-keys -t mc Enter
            wait_for "cases/$subject" 30 || return 1
            sleep 1
        fi
        select_entry "$1"
        $T send-keys -t mc Enter
        wait_for cases.tsv 30 || return 1
        sleep 1
        ;;
    esac
    return 0
}

stop_mc ()
{
    $T kill-server 2>/dev/null
}

# what the screen must show for an expectation
check ()
{
    case "$1" in
    "archive panel")
        screen | grep -q "Arcmc:" && ! screen | grep -q " Error "
        ;;
    "listing")
        screen | grep -q "4Hex" && ! screen | grep -q " Error "
        ;;
    "nothing, no error")
        ! screen | grep -q " Error " && ! screen | grep -q "Arcmc:" && screen | grep -qF "cases.tsv"
        ;;
    "error dialog")
        screen | grep -q " Error " && ! screen | grep -q "Arcmc:"
        ;;
    "the panel it came from")
        ! screen | grep -q " Error " && ! screen | grep -q "Arcmc:\|uzip://" && screen | grep -qF "cases.tsv"
        ;;
    "extfs panel")
        screen | grep -q "uzip://" && ! screen | grep -q " Error "
        ;;
    "copy to the other panel")
        # the copy lands in /tmp, the other panel, and must be as big as the
        # size mc shows for the source; give it a moment to finish
        want=$(screen | grep -F " $2 " | head -1 | awk -F'│' '{gsub(/ /, "", $3); print $3}')
        n=150
        while [ "$n" -gt 0 ]; do
            have=$(stat -c %s "/tmp/$2" 2>/dev/null)
            case "$want" in
            *[0-9]) [ "$have" = "$want" ] && break ;;
            *) [ -n "$have" ] && [ "$have" != 0 ] && [ "$have" = "$last" ] && break ;;
            esac
            last=$have
            sleep 0.2
            n=$((n - 1))
        done
        [ "$n" -gt 0 ] && ! screen | grep -q " Error "
        ;;
    "the name as written")
        screen | grep -qF -- "$2"
        ;;
    *)
        return 2
        ;;
    esac
}

# can every step of $1 be pressed?
steps_known ()
{
    echo "$1" | tr ',' '\n' | while read -r step; do
        case "$step" in
        Enter | F3 | F5 | C-o | .. | "on "* | "cd "* | "type "*) ;;
        *) exit 1 ;;
        esac
    done
}

press ()
{
    echo "$1" | tr ',' '\n' | while read -r step; do
        case "$step" in
        F5)
            $T send-keys -t mc F5
            # the copy box takes its time over a remote panel
            wait_for " Copy " 15 >/dev/null
            ;;
        Enter | F3 | C-o)
            $T send-keys -t mc "$step"
            ;;
        ..)
            # the first entry of a listing is ".."
            $T send-keys -t mc Home
            sleep 0.3
            $T send-keys -t mc Enter
            ;;
        "on "*)
            select_entry "${step#on }"
            ;;
        "cd "*)
            $T send-keys -t mc M-c
            wait_for "cd:" 5 >/dev/null
            $T send-keys -t mc -l "${step#cd }"
            sleep 0.3
            $T send-keys -t mc Enter
            ;;
        "type "*)
            $T send-keys -t mc -l "${step#type }"
            ;;
        esac
        sleep 1.5
    done
}

# the environment's own expectation for dir/file + key over transport $3
override ()
{
    [ -f "$expect_file" ] || return 1
    awk -F'\t' -v f="$1" -v k="$2" -v w="$3" '
        ($1 == f || $1 == "*") && ($2 == k || $2 == "*") && ($4 == "" || index("," $4 ",", "," w ",") > 0) { print $3; found = 1 }
        END { exit found ? 0 : 1 }' "$expect_file"
}

now_ms ()
{
    date +%s%3N
}

# ------------------------------------------------------------------- run ---

if [ $# -eq 0 ]; then
    set -- $(cd "/work/local/$subject" && ls -d */ | sed 's,/$,,')
fi

{
    echo "# $env_name: $subject"
    echo
    echo "- started: $stamp"
    echo "- locale: $locale"
    [ -n "$options" ] && echo "- ini: $(echo "$options" | grep . | tr '\n' ' ')"
    [ -n "$keymap" ] && echo "- keymap: $keymap"
    echo "- mc: $(readlink /work/opt/mc)"
    echo
    echo "| transport | passed | failed | known | fixed | skipped |"
    echo "|-----------|--------|--------|-------|-------|---------|"
} > "$index"

total_fail=0
for where in $(echo "$transports" | tr ',' ' '); do
    out=$report/$where
    mkdir -p "$out"
    results=$out/results.tsv
    printf 'case\tkey\texpect\tverdict\tms\twhy\n' > "$results"

    echo "cases: $subject over $where"
    for d in "$@"; do
        d=${d%/}
        tsv=/work/local/$subject/$d/cases.tsv
        [ -f "$tsv" ] || { echo "  $d: no cases.tsv"; continue; }

        tail -n +2 "$tsv" | while IFS="$(printf '\t')" read -r file key expect why only; do
            [ -n "$file" ] || continue
            rm -f "/tmp/$file"
            # a row for some transports only
            if [ -n "$only" ] && ! echo ",$only," | grep -qF ",$where,"; then
                continue
            fi
            name="$d/$file"
            # a row that names a situation rather than a file is for a person
            if [ ! -e "/work/local/$subject/$d/$file" ]; then
                printf '  skip  %-30s %-8s %s\n' "$name" "$key" "$why"
                printf '%s\t%s\t%s\tskip\t\t%s\n' "$name" "$key" "$expect" "$why" >> "$results"
                continue
            fi
            if ! steps_known "$key"; then
                printf '  skip  %-30s %-8s %s\n' "$name" "$key" "$why"
                printf '%s\t%s\t%s\tskip\t\t%s\n' "$name" "$key" "$expect" "$why" >> "$results"
                continue
            fi
            known=
            if o=$(override "$name" "$key" "$where"); then
                case "$o" in
                known:*)
                    known=$(echo "$o" | sed 's/^known: *//')
                    ;;
                *)
                    expect=$o
                    why="$why (expected by $env_name)"
                    ;;
                esac
            fi
            case "$expect" in
            "archive panel" | listing | "nothing, no error" | "error dialog" | "the panel it came from" \
                | "extfs panel" | "copy to the other panel" | "the name as written") ;;
            *)
                printf '  skip  %-30s %-8s %s\n' "$name" "$key" "$expect"
                printf '%s\t%s\t%s\tskip\t\t%s\n' "$name" "$key" "$expect" "$why" >> "$results"
                continue
                ;;
            esac

            slug=$(echo "$name.$key" | tr '/ ' '..')
            stderr=$out/$slug.stderr
            t0=$(now_ms)
            if ! start_mc "$d" "$where" "$stderr"; then
                screen > "$out/$slug.screen"
                if [ -n "$known" ]; then
                    printf '  known %-30s %-8s %s\n' "$name" "$key" "$known"
                    printf '%s\t%s\t%s\tknown\t%s\t%s\n' "$name" "$key" "$expect" "$(( $(now_ms) - t0 ))" "$known" >> "$results"
                else
                    printf '  FAIL  %-30s %-8s could not open %s over %s\n' "$name" "$key" "$d" "$where"
                    printf '%s\t%s\t%s\tFAIL\t%s\tcould not open the directory\n' "$name" "$key" "$expect" "$(( $(now_ms) - t0 ))" >> "$results"
                fi
                stop_mc
                continue
            fi
            select_entry "$file"
            press "$key"
            sleep 1
            # a remote file takes a moment to arrive: wait for what is expected
            if [ "$where" != local ]; then
                case "$expect" in
                "archive panel") wait_for "Arcmc:" 20 >/dev/null ;;
                listing) wait_for "4Hex" 20 >/dev/null ;;
                "error dialog") wait_for " Error " 20 >/dev/null ;;
                "the panel it came from") wait_for "cases.tsv" 20 >/dev/null ;;
                esac
            fi
            ms=$(( $(now_ms) - t0 ))
            verdict=ok
            check "$expect" "$file" "$d" || verdict=FAIL
            if grep -qE "assert|CRITICAL|Segmentation|AddressSanitizer|runtime error" "$stderr" 2>/dev/null; then
                verdict=FAIL
                why="$why; stderr: $(grep -m1 -E 'assert|CRITICAL|Segmentation|AddressSanitizer|runtime error' "$stderr")"
            fi
            if [ -n "$known" ]; then
                if [ $verdict = ok ]; then
                    verdict=FIXED
                    why="$why; was known to fail: $known"
                else
                    verdict=known
                    why="$known"
                fi
            fi
            case $verdict in
            ok)
                printf '  ok    %-30s %-8s %s  %sms\n' "$name" "$key" "$expect" "$ms"
                [ $verbose = 1 ] && screen > "$out/$slug.screen"
                ;;
            known)
                printf '  known %-30s %-8s %s\n' "$name" "$key" "$known"
                screen > "$out/$slug.screen"
                ;;
            FIXED)
                printf '  FIXED %-30s %-8s passes now, drop it from expect.tsv: %s\n' "$name" "$key" "$known"
                ;;
            *)
                printf '  FAIL  %-30s %-8s expected: %s\n' "$name" "$key" "$expect"
                screen > "$out/$slug.screen"
                sed 's/^/        | /' "$out/$slug.screen"
                ;;
            esac
            printf '%s\t%s\t%s\t%s\t%s\t%s\n' "$name" "$key" "$expect" "$verdict" "$ms" "$why" >> "$results"
            stop_mc
        done
    done

    pass=$(grep -c "$(printf "\tok\t")" "$results")
    fail=$(grep -c "$(printf "\tFAIL\t")" "$results")
    skip=$(grep -c "$(printf "\tskip\t")" "$results")
    knownc=$(grep -c "$(printf "\tknown\t")" "$results")
    fixed=$(grep -c "$(printf "\tFIXED\t")" "$results")
    total_fail=$((total_fail + fail))
    echo "  $where: passed $pass, failed $fail, known $knownc, fixed $fixed, skipped $skip (for a person)"
    echo "| $where | $pass | $fail | $knownc | $fixed | $skip |" >> "$index"
done

{
    echo
    if [ "$total_fail" = 0 ]; then
        echo "No failures."
    else
        echo "## Failures"
        echo
        for where in $(echo "$transports" | tr ',' ' '); do
            grep "$(printf "\tFAIL\t")" "$report/$where/results.tsv" | while IFS="$(printf '\t')" read -r name key expect verdict ms why; do
                slug=$(echo "$name.$key" | tr '/ ' '..')
                echo "- $where: $name $key, expected $expect ($why) - [screen]($where/$slug.screen)"
            done
        done
    fi
    if grep -q "$(printf "\tknown\t\|\tFIXED\t")" "$report"/*/results.tsv; then
        echo
        echo "## Known"
        echo
        for where in $(echo "$transports" | tr ',' ' '); do
            grep "$(printf "\tknown\t\|\tFIXED\t")" "$report/$where/results.tsv" | while IFS="$(printf '\t')" read -r name key expect verdict ms why; do
                echo "- $where: $name $key: $verdict, $why"
            done
        done
    fi
    echo
    echo "- finished: $(date +%Y-%m-%dT%H-%M-%S)"
} >> "$index"

echo "report: $report"
[ "$total_fail" = 0 ]
