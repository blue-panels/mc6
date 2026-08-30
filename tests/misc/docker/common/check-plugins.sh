#!/bin/sh
# Ask whether every plugin the build installed can be loaded at all.
#
# A plugin resolves its symbols against the mc binary and the libraries the
# process has open.  mc links lib/ as a static archive unless --enable-mclib
# is given, and a linker takes out of an archive only what the program itself
# refers to: a widget or a helper that only a plugin uses is left behind, and
# the plugin then fails at g_module_open with an undefined symbol.  That is a
# link-time mistake nothing else catches until someone presses a key.
#
# Runs inside the mc container, after build.
set -u

MC=${MC:-/work/opt/mc/bin/mc}
[ -x "$MC" ] || { echo "check-plugins.sh: no mc at $MC, run build first" >&2; exit 2; }

names () { awk '{print $NF}' | sed 's/@.*//' | sort -u; }

# what the program and the libraries it carries can answer for
{
    nm -D --defined-only "$MC" | names
    for lib in $(ldd "$MC" | awk '/=>/ {print $3}' | grep '^/'); do
        nm -D --defined-only "$lib" 2>/dev/null | names
    done
} | sort -u > /tmp/have.syms

rc=0
n=0
for so in "$(dirname "$MC")"/../lib/mc/*/*/*.so "$(dirname "$MC")"/../lib/mc/*/*.so; do
    [ -f "$so" ] || continue
    n=$((n + 1))
    {
        cat /tmp/have.syms
        for lib in $(ldd "$so" 2>/dev/null | awk '/=>/ {print $3}' | grep '^/'); do
            nm -D --defined-only "$lib" 2>/dev/null | names
        done
    } | sort -u > /tmp/answer.syms

    missing=$(nm -D -u "$so" | names \
        | comm -23 - /tmp/answer.syms \
        | grep -v '^_ITM_\|^__gmon_start__$\|^__cxa_finalize$')

    if [ -n "$missing" ]; then
        printf '  %-28s NOT LOADABLE\n' "$(basename "$so")"
        echo "$missing" | sed 's/^/      /'
        rc=1
    else
        printf '  %-28s ok\n' "$(basename "$so")"
    fi
done

rm -f /tmp/have.syms /tmp/answer.syms
echo "$n plugins checked"
exit $rc
