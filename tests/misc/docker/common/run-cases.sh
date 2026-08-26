#!/bin/sh
# Walk the cases.tsv files and press the keys.  Runs inside the mc container.
#
# usage: run-cases.sh [-w local|sftp|ftp|smb|sh] [-v] [case-dir...]
#
# Each row of a cases.tsv is "file, key, expect, why".  mc is started under
# tmux with the panel in that directory, the file is found by quick search,
# the key is pressed and the screen is read.  Only rows whose key and
# expectation this script knows how to press and read are run; the rest are
# reported as skipped, they are for a person.
#
# For a remote location the connection is written to the plugin's ini file and
# the panel is opened through the plugin, so the same rows run over each
# protocol.
set -u

MC=/work/opt/mc/bin/mc
where=local
verbose=0
log=$(mktemp)

while getopts "w:v" opt; do
    case "$opt" in
    w) where=$OPTARG ;;
    v) verbose=1 ;;
    *) exit 2 ;;
    esac
done
shift $((OPTIND - 1))

# where the tree is
case "$where" in
local) root=/work/local ;;
sftp | sh) root=/home/mc/archives ;;
ftp) root=/archives ;;
smb) root=/ ;;
*)
    echo "run-cases.sh: unknown location: $where" >&2
    exit 2
    ;;
esac

# the connection each plugin reads on start; plain passwords are accepted
config=${XDG_CONFIG_HOME:-$HOME/.config}/mc
mkdir -p "$config"
case "$where" in
sftp)
    printf '[sandbox]\nhost=remote\nuser=mc\npassword=mc\npath=%s\nuse_agent=false\n' "$root" \
        > "$config/sftp-connections.ini"
    ;;
sh)
    printf '[sandbox]\nhost=remote\nuser=mc\npassword=mc\npath=%s\n' "$root" \
        > "$config/shell-connections.ini"
    # shell-link's libssh2 transport insists on a known_hosts file, and a host
    # it does not know is a question; the answer is put there beforehand
    mkdir -p "$HOME/.ssh" && chmod 700 "$HOME/.ssh"
    ssh-keyscan remote > "$HOME/.ssh/known_hosts" 2>/dev/null
    ;;
ftp)
    printf '[sandbox]\nhost=remote\nuser=mc\npassword=mc\npath=%s\n' "$root" \
        > "$config/ftp-connections.ini"
    ;;
smb)
    printf '[sandbox]\nserver=remote\nshare=archives\nusername=mc\npassword=mc\n' \
        > "$config/smb-connections.ini"
    ;;
esac

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

start_mc ()
{
    $T kill-server 2>/dev/null
    # the panel opens in the case directory, or on the plugin's connection list;
    # messages in English so that the screen can be read, the charset as it is
    case "$where" in
    local) open_path=$1 ;;
    *) open_path="$where:" ;;
    esac
    $T new-session -d -s mc -x 120 -y 40 \
        "TERM=xterm-256color LC_ALL=en_US.UTF-8 $MC -S default '$open_path' /tmp"
    case "$where" in
    local)
        wait_for cases.tsv 10 || return 1
        ;;
    *)
        # the plugin lists its connections; Enter on ours opens $root
        wait_for sandbox 10 || return 1
        select_entry sandbox
        $T send-keys -t mc Enter
        wait_for "$(basename "$(dirname "$1")")" 30 || return 1
        select_entry "$(basename "$1")"
        $T send-keys -t mc Enter
        wait_for cases.tsv 30 || return 1
        ;;
    esac
    return 0
}

stop_mc ()
{
    $T kill-server 2>/dev/null
}

# what the screen must show for an expectation; empty means "not automated"
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
    *)
        return 2
        ;;
    esac
}

press ()
{
    case "$1" in
    Enter) $T send-keys -t mc Enter ;;
    F3) $T send-keys -t mc F3 ;;
    *) return 2 ;;
    esac
}

pass=0
fail=0
skip=0
: > "$log"

if [ $# -eq 0 ]; then
    set -- $(cd /work/local && ls -d */ | sed 's,/$,,')
fi

echo "cases over $where"
for d in "$@"; do
    d=${d%/}
    tsv=/work/local/$d/cases.tsv
    [ -f "$tsv" ] || { echo "  $d: no cases.tsv"; continue; }

    tail -n +2 "$tsv" | while IFS="$(printf '\t')" read -r file key expect why; do
        [ -n "$file" ] || continue
        # a row that names a situation rather than a file is for a person
        if [ ! -e "/work/local/$d/$file" ]; then
            printf '  skip  %-24s %-14s %s\n' "$d/$file" "$key" "$why"
            echo skip >> "$log"
            continue
        fi
        case "$key" in
        Enter | F3) ;;
        *)
            printf '  skip  %-24s %-14s %s\n' "$d/$file" "$key" "$why"
            echo skip >> "$log"
            continue
            ;;
        esac
        case "$expect" in
        "archive panel" | listing | "nothing, no error" | "error dialog") ;;
        *)
            printf '  skip  %-24s %-14s %s\n' "$d/$file" "$key" "$expect"
            echo skip >> "$log"
            continue
            ;;
        esac

        if ! start_mc "$root/$d"; then
            printf '  FAIL  %-24s %-14s could not open %s\n' "$d/$file" "$key" "$root/$d"
            [ $verbose = 1 ] && screen
            echo fail >> "$log"
            stop_mc
            continue
        fi
        select_entry "$file"
        press "$key"
        sleep 2
        # a remote archive takes a moment to arrive
        [ "$where" = local ] || wait_for "Arcmc:" 20 >/dev/null
        if check "$expect"; then
            printf '  ok    %-24s %-14s %s\n' "$d/$file" "$key" "$expect"
            echo pass >> "$log"
        else
            printf '  FAIL  %-24s %-14s expected: %s\n' "$d/$file" "$key" "$expect"
            screen | sed 's/^/        | /'
            echo fail >> "$log"
        fi
        stop_mc
    done
done

pass=$(grep -c pass "$log")
fail=$(grep -c fail "$log")
skip=$(grep -c skip "$log")
echo "passed $pass, failed $fail, skipped $skip (for a person)"
[ "$fail" = 0 ]
