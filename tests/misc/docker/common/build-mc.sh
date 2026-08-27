#!/bin/sh
# Build the tree mounted at /src and install it under /work/opt.  The sources
# are copied first, so the working tree stays as it was and root-owned build
# output stays in the container volume.
#
# usage: build-mc.sh [-f profiles]
#
# Profiles come from features.ini; each set of them gets its own build and
# install directory, and /work/opt/mc points at the one built last, which is
# the one run-cases.sh runs.
set -e

features=${FEATURES:-all}
while getopts "f:" opt; do
    case "$opt" in
    f) features=$OPTARG ;;
    *) exit 2 ;;
    esac
done

ini=/usr/local/bin/features.ini

# "a,b,-c": the flags of a and b, then those of c taken out.  An --enable-x
# flag taken out becomes --enable-x=no, since configure's own default is auto.
profile_flags ()
{
    key=$1
    shift
    for name in $(echo "$features" | tr ',' ' '); do
        case "$name" in
        -*) continue ;;
        esac
        sed -n "/^\[$name\]/,/^\[/p" "$ini" | sed -n "s/^$key *= *//p"
    done | tr ' ' '\n' | grep . | sort -u > /tmp/flags.$$
    for name in $(echo "$features" | tr ',' ' '); do
        case "$name" in
        -*)
            for flag in $(sed -n "/^\[${name#-}\]/,/^\[/p" "$ini" | sed -n "s/^$key *= *//p"); do
                base=${flag%%=*}
                grep -vE "^$base(=.*)?\$" /tmp/flags.$$ > /tmp/flags2.$$ || true
                mv /tmp/flags2.$$ /tmp/flags.$$
                case "$base" in
                --enable-*) echo "$base=no" >> /tmp/flags.$$ ;;
                esac
            done
            ;;
        esac
    done
    tr '\n' ' ' < /tmp/flags.$$
    rm -f /tmp/flags.$$
}

configure_flags=$(profile_flags configure)
cflags=$(profile_flags cflags)
tag=$(echo "$features" | tr ',' '+')

copy_sources ()
{
    rsync -a --delete \
        --exclude '.git' \
        --exclude 'tests/misc/docker/' \
        /src/ /work/src/
}

echo "== copying sources =="
mkdir -p /work/src
copy_sources

cd /work/src
[ -x autogen.sh ] || { echo "no autogen.sh in /work/src: is the tree mounted at /src?" >&2; exit 1; }
if [ ! -x configure ] || [ configure.ac -nt configure ]; then
    echo "== autogen =="
    ./autogen.sh
fi

# Everything lives in the volume: the container that builds mc is not the one
# that runs it.  One tree per set of features.
PREFIX=/work/opt/mc-$tag
BUILD=/work/build-$tag
mkdir -p "$BUILD"

cd "$BUILD"
if [ ! -f config.status ] || [ /work/src/configure -nt config.status ] \
    || [ "$(cat flags 2>/dev/null)" != "$configure_flags|$cflags" ]; then
    echo "== configure [$features] =="
    echo "   $configure_flags"
    [ -n "$cflags" ] && echo "   CFLAGS: $cflags"
    # shellcheck disable=SC2086
    CFLAGS="$cflags" /work/src/configure --prefix="$PREFIX" $configure_flags
    echo "$configure_flags|$cflags" > flags
fi

echo "== make =="
make -j"$(nproc)"
make install

# a volume from before the profiles has a real directory here
if [ -d /work/opt/mc ] && [ ! -L /work/opt/mc ]; then
    mv /work/opt/mc /work/opt/mc-before-profiles
fi
rm -f /work/opt/mc
ln -s "$PREFIX" /work/opt/mc

# The same cases on a local panel, for what does not need a server.  An
# environment that exists to check the build may not have an archiver at all.
# Made again on every build: cases.tsv comes from here, and it is cheap.
for f in /src/tests/misc/docker/cases/*/fixtures.sh; do
    subject=$(basename "$(dirname "$f")")
    if command -v bsdtar >/dev/null; then
        echo "== local fixtures: $subject =="
        sh "$f" "/work/local/$subject" >/dev/null
    else
        echo "== local fixtures skipped: no bsdtar in this image =="
    fi
done

echo
echo "mc installed:      $PREFIX/bin/mc  (features: $features)"
echo "panel plugins in:  $(ls -d "$PREFIX"/lib/mc/panel-plugins/* 2>/dev/null | xargs -n1 basename 2>/dev/null | tr '\n' ' ')"
