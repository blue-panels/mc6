#!/bin/sh
# Menus that compose one sandbox.sh test command, show it, run it, and open
# the screens of what failed.  Nothing lives here that the command line cannot
# do; the choices are kept in reports/last.ui so that the next time starts
# where this one ended.
#
# usage: sandbox.sh [env] ui
set -u

root=$(cd "$(dirname "$0")/.." && pwd)
env=${1:-debian-12}
state=$root/reports/last.ui

D=$(command -v dialog || command -v whiptail) || {
    echo "ui.sh: needs dialog or whiptail" >&2
    exit 2
}

# saved choices
subject=archives; transports=local; locale=ru_RU.UTF-8; profile=; toggles=; extra=; keymap=; dirs=
[ -f "$state" ] && . "$state"

# whiptail and dialog both answer on stderr; dialog takes the mouse, whiptail
# cannot
mouse=
case "$D" in
*/dialog) mouse=--mouse ;;
esac
ask ()
{
    "$D" $mouse --title "mc sandbox" "$@" 3>&1 1>&2 2>&3
}

on_off ()
{
    case ",$2," in
    *,"$1",*) echo on ;;
    *) echo off ;;
    esac
}

# ------------------------------------------------------------ the menus ---

# 1. environment
set --
for d in "$root"/envs/*/; do
    n=$(basename "$d")
    [ -f "$d/docker-compose.yml" ] || continue
    set -- "$@" "$n" "$(head -1 "$d/README.md" 2>/dev/null | sed 's/^# *//')" "$(on_off "$n" "$env")"
done
env=$(ask --radiolist "Environment" 15 70 6 "$@") || exit 0

# 2. subject
set --
for d in "$root"/cases/*/; do
    n=$(basename "$d")
    set -- "$@" "$n" "cases/$n" "$(on_off "$n" "$subject")"
done
subject=$(ask --radiolist "Subject" 12 60 4 "$@") || exit 0

# 3. transports
transports=$(ask --checklist "Transports (space marks)" 14 60 5 \
    local "a local panel, no server" "$(on_off local "$transports")" \
    sftp "the sftp plugin, a stream" "$(on_off sftp "$transports")" \
    sh "shell-link, a stream" "$(on_off sh "$transports")" \
    ftp "the ftp plugin, a local copy" "$(on_off ftp "$transports")" \
    smb "the samba plugin, a local copy" "$(on_off smb "$transports")") || exit 0
transports=$(echo "$transports" | tr -d '"' | tr ' ' ',')
[ -n "$transports" ] || transports=local

# 4. locale
locale=$(ask --radiolist "Locale mc runs in (messages stay English)" 13 60 5 \
    ru_RU.UTF-8 "UTF-8, Cyrillic" "$(on_off ru_RU.UTF-8 "$locale")" \
    en_US.UTF-8 "UTF-8" "$(on_off en_US.UTF-8 "$locale")" \
    ru_RU.KOI8-R "8-bit, Cyrillic" "$(on_off ru_RU.KOI8-R "$locale")" \
    ru_RU.CP866 "8-bit, the DOS codepage" "$(on_off ru_RU.CP866 "$locale")" \
    C "no locale at all" "$(on_off C "$locale")") || exit 0

# 5. build profile
set -- keep "the build there is now" "$(on_off keep "${profile:-keep}")"
for p in $(sed -n 's/^\[\(.*\)\]$/\1/p' "$root/common/features.ini"); do
    # a description starting with "-" would be read as an option
    set -- "$@" "$p" "$(sed -n "/^\[$p\]/,/^\[/{s/^configure *= *//p;s/^cflags *= *//p}" "$root/common/features.ini" | head -1 | cut -c1-40 | sed 's/^/: /')" "$(on_off "$p" "${profile:-keep}")"
done
profile=$(ask --radiolist "Build profile (a rebuild before the run)" 20 78 12 "$@") || exit 0
if [ "$profile" != keep ]; then
    more=$(ask --inputbox "Profiles, comma separated; -name takes one out" 8 60 "$profile") || exit 0
    profile=$more
fi

# 6. runtime toggles
set --
while IFS='|' read -r left desc; do
    tag=$(echo "$left" | sed 's/ *=.*//')
    [ -n "$tag" ] || continue
    case "$tag" in \#*) continue ;; esac
    set -- "$@" "$tag" "$(echo "$desc" | sed 's/^ *//')" "$(on_off "$tag" "$toggles")"
done < "$root/common/toggles.ini"
toggles=$(ask --checklist "ini values written before mc starts" 16 70 8 "$@") || exit 0
toggles=$(echo "$toggles" | tr -d '"' | tr ' ' ',')
extra=$(ask --inputbox "More ini values, space separated: [section.]key=value" 8 70 "$extra") || exit 0

# 7. keymap
set -- none "mc's own bindings" "$(on_off none "${keymap:-none}")"
for k in "$root"/common/keymaps/*.keymap; do
    [ -f "$k" ] || continue
    n=$(basename "$k" .keymap)
    set -- "$@" "$n" "$(grep -v '^\[' "$k" | grep . | head -2 | tr '\n' ' ')" "$(on_off "$n" "${keymap:-none}")"
done
keymap=$(ask --radiolist "Keymap" 12 70 5 "$@") || exit 0

# 8. case directories, read from the subject's fixtures
set --
for n in $(sed -n 's/^mkdir -p \([0-9][^ ]*\)$/\1/p' "$root/cases/$subject/fixtures.sh"); do
    set -- "$@" "$n" "" "$(if [ -z "$dirs" ]; then echo on; else on_off "$n" "$dirs"; fi)"
done
dirs=$(ask --checklist "Case directories (none marked = all)" 14 60 6 "$@") || exit 0
dirs=$(echo "$dirs" | tr -d '"' | tr ' ' ',')

# -------------------------------------------------------- the command ---

cat > "$state" <<EOF
env=$env
subject=$subject
transports=$transports
locale=$locale
profile=$profile
toggles=$toggles
extra="$extra"
keymap=$keymap
dirs=$dirs
EOF

# what was typed in goes to sandbox.sh as arguments, never through a shell;
# an ini value or a profile is letters, digits and a few marks
for v in $extra $profile; do
    case "$v" in
    *[!A-Za-z0-9_.,=:/-]*)
        ask --msgbox "Not taken: $v\n\nan ini value is [section.]key=value, a profile a name" 8 60
        exit 1
        ;;
    esac
done
opts=""
for t in $(echo "$toggles" | tr ',' ' '); do
    v=$(sed -n "s/^$t *= *\([^|]*\)|.*/\1/p" "$root/common/toggles.ini" | sed 's/ *$//')
    [ -n "$v" ] && opts="$opts -o $v"
done
for v in $extra; do
    opts="$opts -o $v"
done
[ "$keymap" != none ] && opts="$opts -k $keymap"
dirs_sp=$(echo "$dirs" | tr ',' ' ')

build=""
[ "$profile" != keep ] && build="$root/sandbox.sh $env build -f $profile && "
cmd="$root/sandbox.sh $env test -c $subject -w $transports -l $locale$opts $dirs_sp"

ask --yesno "Run this?\n\n$build$cmd" 12 78 || exit 0
echo "$build$cmd" > "$root/reports/last.cmd"

clear
echo "$build$cmd"
echo
run ()
{
    if [ "$profile" != keep ]; then
        "$root/sandbox.sh" "$env" build -f "$profile" || return $?
    fi
    # shellcheck disable=SC2086
    "$root/sandbox.sh" "$env" test -c "$subject" -w "$transports" -l "$locale" $opts $dirs_sp
}
report=$(run | tee /dev/stderr | sed -n 's/^report: \/reports\///p' | tail -1)

# ---------------------------------------------------------- the failures ---

[ -n "$report" ] || exit 1
index=$root/reports/$report/index.md
[ -f "$index" ] || exit 1

while :; do
    set --
    i=0
    for f in "$root/reports/$report"/*/*.screen; do
        [ -f "$f" ] || continue
        i=$((i + 1))
        set -- "$@" "$i" "$(echo "$f" | sed "s,.*/reports/$report/,,")"
    done
    if [ $# -eq 0 ]; then
        ask --textbox "$index" $(( $(tput lines) - 2 )) $(( $(tput cols) - 2 ))
        exit 0
    fi
    pick=$(ask --menu "Failures in $report (the screen mc showed)" 20 78 10 "$@") || exit 1
    [ -n "$pick" ] || exit 1
    n=0
    for f in "$root/reports/$report"/*/*.screen; do
        [ -f "$f" ] || continue
        n=$((n + 1))
        [ "$n" = "$pick" ] && ask --textbox "$f" $(( $(tput lines) - 2 )) $(( $(tput cols) - 2 ))
    done
done
