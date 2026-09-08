#!/bin/sh
# The get helper announces the size of the bytes it sends, for a symbolic
# link the size of the file behind it, in each of its branches. The copy
# built into the plugin is held to the same.

helper="${srcdir:-.}/../../../../src/panel-plugins/shell-link/helpers/get"
if [ ! -f "$helper" ]; then
    echo "helper not found: $helper" >&2
    exit 99
fi

tmp=$(mktemp -d) || exit 99
trap 'rm -rf "$tmp"' EXIT

awk 'BEGIN { for (i = 0; i < 300; i++) printf "line %d of the file\n", i }' > "$tmp/file"
ln -s file "$tmp/link"
size=$(wc -c < "$tmp/file" | tr -d ' ')

status=0

# $1 = script, $2 = label, $3 = path, $4 = expected size, the rest = environment
check ()
{
    script=$1; mode=$2; path=$3; want=$4; shift 4

    env -i PATH="$PATH" SHELL_FILENAME="${path#/}" SHELL_START_OFFSET=0 "$@" sh "$script" > "$tmp/out"
    got=$(sed -n 1p "$tmp/out")
    if [ "$got" != "$want" ]; then
        echo "$mode: $path: announced $got, expected $want" >&2
        status=1
    fi
    if [ "$(sed -n 2p "$tmp/out")" != "### 100" ]; then
        echo "$mode: $path: no ### 100" >&2
        status=1
    fi
    # the bytes between the ### 100 line and the ### 200 tail
    total=$(wc -c < "$tmp/out" | tr -d ' ')
    head=$(sed -n 1,2p "$tmp/out" | wc -c | tr -d ' ')
    sent=$((total - head - 8))
    if [ "$sent" != "$want" ]; then
        echo "$mode: $path: sent $sent bytes, announced $want" >&2
        status=1
    fi
}

check_mode ()
{
    script=$1; mode=$2; shift 2

    check "$script" "$mode" "$tmp/file" "$size" "$@"
    check "$script" "$mode" "$tmp/link" "$size" "$@"
}

if perl -e 1 2>/dev/null; then
    check_mode "$helper" perl SHELL_HAVE_PERL=1 SHELL_MAX_BYTES=0
    check "$helper" perl-capped "$tmp/link" 100 SHELL_HAVE_PERL=1 SHELL_MAX_BYTES=100
fi
check_mode "$helper" tail SHELL_HAVE_TAIL=1 SHELL_MAX_BYTES=0
check "$helper" tail-capped "$tmp/link" 100 SHELL_HAVE_TAIL=1 SHELL_MAX_BYTES=100
check_mode "$helper" dd SHELL_MAX_BYTES=0
check "$helper" dd-capped "$tmp/link" 100 SHELL_MAX_BYTES=100

if [ -x ./builtin_helper ]; then
    ./builtin_helper get > "$tmp/builtin_get"
    check_mode "$tmp/builtin_get" builtin
fi

exit $status
