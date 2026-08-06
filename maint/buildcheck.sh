#!/bin/sh
#
# Run what .github/workflows/ci-ubuntu.yml runs, in a container, on this machine.
#
# Usage:
#   maint/buildcheck.sh [ref]
#
# Environment:
#   BUILDCHECK_IMAGE  container image (default: ubuntu:24.04, what ubuntu-latest is)
#   BUILDCHECK_JOBS   parallel make jobs (default: all cores)
#
# The build runs as an unprivileged user: the extfs helper tests compare uid and
# gid against the ones of the process, and root makes them print differently.

set -eu

REF=${1:-HEAD}
IMAGE=${BUILDCHECK_IMAGE:-ubuntu:24.04}
NAME=mc-buildcheck-$$

srcdir=$(cd "$(dirname "$0")/.." && pwd)
test -d "$srcdir/.git" || { echo "buildcheck: $srcdir is not a git checkout" >&2; exit 1; }
command -v docker >/dev/null || { echo "buildcheck: docker not found" >&2; exit 1; }

sha=$(git -C "$srcdir" rev-parse "$REF")
echo "buildcheck: $IMAGE, $(git -C "$srcdir" log --oneline -1 "$sha")"

cleanup() { docker rm -f "$NAME" >/dev/null 2>&1 || true; }
trap cleanup EXIT INT TERM

docker run -d --name "$NAME" -v "$srcdir":/repo:ro "$IMAGE" sleep infinity >/dev/null

docker exec -i -e SHA="$sha" -e JOBS="${BUILDCHECK_JOBS:-}" "$NAME" sh -s <<'INNER'
set -eu

nproc_or() { j=${JOBS:-}; test -n "$j" || j=$(nproc); echo "$j"; }
JOBS=$(nproc_or)
fail=0
step() { printf '\n=== %s\n' "$1"; }
ok() { printf 'PASS %s\n' "$1"; }
bad() { printf 'FAIL %s\n' "$1"; fail=1; }

export DEBIAN_FRONTEND=noninteractive
step "install dependencies"
apt-get update -qq >/dev/null
apt-get install -y -qq --no-install-recommends \
    git autoconf automake autopoint build-essential gettext libtool pkg-config check unzip bzip2 \
    e2fslibs-dev libaspell-dev libglib2.0-dev libgpm-dev libncurses-dev libslang2-dev \
    libssh2-1-dev libx11-dev libarchive-dev libcurl4-openssl-dev libsmbclient-dev \
    libmongoc-dev libbson-dev >/dev/null 2>&1 || { bad "dependencies"; exit 1; }

id -u build >/dev/null 2>&1 || useradd -m build
git config --global --add safe.directory '*'
rm -rf /work
git clone -q /repo /work
cd /work
git checkout -q "$SHA"
chown -R build /work

as_build() { su build -c "cd $1 && $2"; }

step "bootstrap"
if as_build /work "./autogen.sh >/tmp/autogen.log 2>&1"; then
    ok "bootstrap"
else
    bad "bootstrap"; tail -8 /tmp/autogen.log; exit 1
fi

step "mongo plugin"
mkdir -p /work/build-mongo && chown build /work/build-mongo
if as_build /work/build-mongo "../configure --enable-panel-plugin-mongo=yes >/tmp/cf-mongo.log 2>&1 && make -j$JOBS >/tmp/mk-mongo.log 2>&1" \
   && test -f /work/build-mongo/src/panel-plugins/mongo/.libs/mc-panel-mongo.so; then
    ok "mongo plugin"
else
    bad "mongo plugin"; tail -5 /tmp/cf-mongo.log 2>/dev/null || true
    grep -E 'error:' /tmp/mk-mongo.log 2>/dev/null | head -5 || true
fi

step "distribution archive"
mkdir -p /work/build-distrib && chown build /work/build-distrib
if as_build /work/build-distrib "../configure >/tmp/cf-dist.log 2>&1 && make dist-bzip2 >/tmp/mk-dist.log 2>&1"; then
    ok "distribution archive"
else
    bad "distribution archive"; tail -5 /tmp/mk-dist.log; exit 1
fi
tarball=$(ls /work/build-distrib/mc-*.tar.bz2)

configuration() {
    name=$1; shift
    step "configuration: $name"
    cd /work
    rm -rf "build-$name"
    tar -xjf "$tarball" --one-top-level="build-$name"
    chown -R build "/work/build-$name"
    if ! as_build "/work/build-$name" "../configure --prefix=\$(pwd)/install $* >/tmp/cf-$name.log 2>&1"; then
        bad "$name: configure"; tail -8 "/tmp/cf-$name.log"; return
    fi
    if ! as_build "/work/build-$name" "make -j$JOBS >/tmp/mk-$name.log 2>&1"; then
        bad "$name: build"; grep -E 'error:' "/tmp/mk-$name.log" | head -8; return
    fi
    if ! as_build "/work/build-$name" "make check >/tmp/ck-$name.log 2>&1"; then
        bad "$name: tests"; grep -E '^(FAIL|ERROR):' "/tmp/ck-$name.log" | head -8; return
    fi
    ok "$name"
}

configuration full --enable-mclib --enable-werror
as_build /work/build-full "make install >/tmp/inst-full.log 2>&1" && ok "install" || bad "install"

configuration ncurses --with-screen=ncurses --enable-werror

configuration minimal --disable-shared --disable-static --disable-maintainer-mode \
    --disable-largefile --disable-nls --disable-rpath --disable-mclib --disable-assert \
    --disable-background --disable-vfs --without-x --without-gpm-mouse \
    --without-internal-edit --without-diff-viewer --without-subshell --enable-tests --enable-werror

step "result"
test "$fail" -eq 0 && echo "buildcheck: all green" || echo "buildcheck: something failed"
exit "$fail"
INNER
