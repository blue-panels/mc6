#!/bin/sh
# The ls helper says what every symbolic link points to, in each of its
# branches, and lists a link to a directory as that directory. The copy
# built into the plugin (shelldef.h) is held to the same, when the check
# program that prints it is around.

helper="${srcdir:-.}/../../../../src/panel-plugins/shell-link/helpers/ls"
if [ ! -f "$helper" ]; then
    echo "helper not found: $helper" >&2
    exit 99
fi

tmp=$(mktemp -d) || exit 99
trap 'rm -rf "$tmp"' EXIT

mkdir "$tmp/real" || exit 99
: > "$tmp/real/inside"
: > "$tmp/plain"
ln -s real "$tmp/todir"
ln -s plain "$tmp/tofile"
ln -s nowhere "$tmp/dangling"

status=0

# The T line of the record named $2 in the listing $1, or "none".
tline ()
{
    t=none
    name=""
    while IFS= read -r line; do
        case "$line" in
        T*) t=$line ;;
        :*) name=${line#:}
            name=${name%% -> *}
            name=${name#\"}
            name=${name%\"} ;;
        "") if [ "$name" = "$2" ]; then
                echo "$t"
            fi
            t=none
            name="" ;;
        esac
    done < "$1"
}

expect ()
{
    got=$(tline "$1" "$2")
    if [ "$got" != "$3" ]; then
        echo "$mode: $2: expected $3, got $got" >&2
        status=1
    fi
}

# $1 = script, $2 = label, $3 = "T-" or "none" for a link to a file,
# the rest = environment
check_mode ()
{
    script=$1; mode=$2; tofile=$3; shift 3

    env -i PATH="$PATH" SHELL_FILENAME="${tmp#/}" "$@" sh "$script" > "$tmp.lst"
    expect "$tmp.lst" todir Td
    expect "$tmp.lst" dangling 'T!'
    expect "$tmp.lst" tofile "$tofile"
    expect "$tmp.lst" plain none
    expect "$tmp.lst" real none

    # the path of a link to a directory lists the directory
    env -i PATH="$PATH" SHELL_FILENAME="${tmp#/}/todir" "$@" sh "$script" > "$tmp.lst"
    if ! grep -q '^:"\{0,1\}inside"\{0,1\}$' "$tmp.lst"; then
        echo "$mode: the listing of todir does not show inside" >&2
        status=1
    fi
    rm -f "$tmp.lst"
}

if perl -e 1 2>/dev/null; then
    check_mode "$helper" perl T- SHELL_HAVE_PERL=1
fi
if ls -Q / >/dev/null 2>&1; then
    check_mode "$helper" lsq none SHELL_HAVE_LSQ=1
    if [ -x ./builtin_helper ]; then
        ./builtin_helper ls > "$tmp/builtin_ls"
        check_mode "$tmp/builtin_ls" builtin none
    fi
fi
check_mode "$helper" sed none SHELL_HAVE_SED=1 SHELL_HAVE_DATE_MDYT=1
check_mode "$helper" poor_ls none

exit $status
