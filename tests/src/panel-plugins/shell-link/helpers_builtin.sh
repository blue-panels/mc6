#!/bin/sh
# The copy of every helper compiled into the plugin is the file in helpers/,
# not a second version of it. shelldef.h is made from those files, so a broken
# escaping rule shows up here as a script that differs from its source.
#
# Each script also has to declare its revision, because the plugin reads the
# revision it expects out of that same text.

dir="${srcdir:-.}/../../../../src/panel-plugins/shell-link/helpers"
if [ ! -d "$dir" ]; then
    echo "helpers not found: $dir" >&2
    exit 99
fi
if [ ! -x ./builtin_helper ]; then
    echo "builtin_helper was not built" >&2
    exit 77
fi

tmp=$(mktemp -d) || exit 99
trap 'rm -rf "$tmp"' EXIT

status=0
count=0

for path in "$dir"/*; do
    name=$(basename "$path")
    case "$name" in
        README.* | Makefile* | *.mk) continue ;;
    esac

    if ! ./builtin_helper "$name" > "$tmp/out" 2>"$tmp/err"; then
        echo "$name: no built-in copy" >&2
        cat "$tmp/err" >&2
        status=1
        continue
    fi

    if ! cmp -s "$tmp/out" "$path"; then
        echo "$name: the built-in copy differs from $path" >&2
        diff -u "$path" "$tmp/out" | head -20 >&2
        status=1
        continue
    fi

    rev=$(sed -n "1s/^# shfs-helper: $name \\([0-9][0-9]*\\)\$/\\1/p" "$path")
    if [ -z "$rev" ] || [ "$rev" -lt 1 ]; then
        echo "$name: the first line does not declare a revision" >&2
        head -1 "$path" >&2
        status=1
        continue
    fi

    count=$((count + 1))
done

if [ $count -eq 0 ]; then
    echo "no helper was checked" >&2
    status=1
fi

exit $status
