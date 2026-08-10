#!/bin/sh
# Build the tree mounted at /src and install it to /opt/mc.  The sources are
# copied first, so the working tree stays as it was and root-owned build
# output stays in the container volume.
set -e

copy_sources ()
{
    rsync -a --delete \
        --exclude '.git' \
        --exclude 'docker/stream-sandbox/' \
        /src/ /work/src/
}

echo "== copying sources =="
mkdir -p /work/src /work/build
copy_sources

cd /work/src
if [ ! -x configure ] || [ configure.ac -nt configure ]; then
    echo "== autogen =="
    ./autogen.sh
fi

# Everything lives in the volume: the container that builds mc is not the one
# that runs it.
PREFIX=/work/opt/mc

cd /work/build
if [ ! -f config.status ] || [ /work/src/configure -nt config.status ] \
    || ! ./config.status --config 2>/dev/null | grep -q -- "--prefix=$PREFIX"; then
    echo "== configure =="
    /work/src/configure \
        --prefix="$PREFIX" \
        --with-screen=ncurses \
        --enable-panel-plugin-sftp \
        --enable-panel-plugin-arcmc \
        --enable-panel-plugin-ftp \
        --enable-panel-plugin-samba \
        --enable-panel-plugin-shell-link \
        --disable-panel-plugin-mongo
fi

echo "== make =="
make -j"$(nproc)"
make install

echo
echo "mc installed:      $PREFIX/bin/mc"
echo "panel plugins in:  $(ls -d "$PREFIX"/lib/mc/panel-plugins/* 2>/dev/null | tr '\n' ' ')"
