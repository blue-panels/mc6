#!/bin/sh
# Build the source packages a Launchpad PPA takes, one per Ubuntu series.
#
# usage: packaging/ppa-source.sh VERSION ARCHIVE SERIES:UBUNTU_VERSION...
#   packaging/ppa-source.sh 6.0.3 dist/mc6-6.0.3.tar.gz noble:24.04 jammy:22.04
#
# A PPA builds the binaries itself, so what is uploaded is a signed source
# package per series. Every series shares one orig tarball: the series lives in
# the Debian revision (3:6.0.3-1~ubuntu24.04.1), which leaves the upstream
# version, and hence the tarball name, alone. Only the first series carries the
# tarball in its upload; the rest refer to the one already there.
#
# The series is given with its Ubuntu version because that version orders the
# uploads: ~ubuntu24.04.1 sorts above ~ubuntu22.04.1, so moving to a newer
# series is an upgrade. Codenames cannot do that, having wrapped the alphabet.
#
# Environment:
#   PPA          dput target (default: ppa:il-smind/mc6)
#   PPA_REVISION number after the series in the version, 1 by default. A PPA
#                keeps every version it has ever accepted, so a rejected or
#                broken upload comes back as 2, never as 1 again.
#   SIGN_KEY     key to sign with; unset uses the default gpg key
#   NOSIGN       yes to build unsigned packages, for a dry run
#   UPLOAD       yes to dput each source package after building it
#   OUTDIR       where the results land (default: dist/ppa)
set -eu

die() {
    echo "$*" >&2
    exit 1
}

version=${1:?usage: packaging/ppa-source.sh VERSION ARCHIVE SERIES:UBUNTU_VERSION...}
archive=${2:?an archive from packaging/release-source.sh is required}
shift 2
test $# -gt 0 || die "name at least one series, for example noble:24.04"

ppa=${PPA:-ppa:il-smind/mc6}
revision=${PPA_REVISION:-1}
outdir=${OUTDIR:-dist/ppa}

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"

test -f "$archive" || die "no such archive: $archive"
archive=$(CDPATH= cd -- "$(dirname -- "$archive")" && pwd)/$(basename -- "$archive")

command -v dpkg-buildpackage >/dev/null || die "dpkg-buildpackage is missing (install dpkg-dev)"
if test "${UPLOAD:-}" = yes; then
    command -v dput >/dev/null || die "dput is missing (install dput)"
fi

mkdir -p "$outdir"
outdir=$(CDPATH= cd -- "$outdir" && pwd)

stage=$(mktemp -d "${TMPDIR:-/tmp}/mc6-ppa.XXXXXX")
trap 'rm -rf "$stage"' EXIT HUP INT TERM

# One shared orig tarball, and one unpacked tree per series next to it.
cp "$archive" "$stage/mc6_$version.orig.tar.gz"

first=yes
for pair in "$@"; do
    series=${pair%%:*}
    ubuntu_version=${pair#*:}
    test "$series" != "$pair" || die "give the Ubuntu version too, as $pair:24.04"
    test -n "$series" -a -n "$ubuntu_version" || die "bad series: $pair"

    DEB_DISTRIBUTION="$series" DEB_VERSION_SUFFIX="~ubuntu$ubuntu_version.$revision" \
        "$root/packaging/prepare.sh" "$version" "$archive" >/dev/null

    rm -rf "$stage/mc6-$version"
    tar -xf "$archive" -C "$stage"
    cp -r debian "$stage/mc6-$version/"

    # -sa carries the tarball, -sd refers to the one uploaded before it.
    if test "$first" = yes; then
        source_flag=-sa
        first=no
    else
        source_flag=-sd
    fi

    # -d: build dependencies belong to the PPA builder, not here. -nc: the tree
    # was just unpacked from the archive, so there is nothing to clean, and
    # skipping it keeps debhelper out of a source-only build.
    set -- -S "$source_flag" -d -nc
    if test "${NOSIGN:-}" = yes; then
        set -- "$@" -us -uc
    elif test -n "${SIGN_KEY:-}"; then
        set -- "$@" "-k${SIGN_KEY}"
    fi

    (cd "$stage/mc6-$version" && dpkg-buildpackage "$@")

    changes=$(ls "$stage"/mc6_*~ubuntu"$ubuntu_version"."$revision"_source.changes)
    cp "$stage"/mc6_*~ubuntu"$ubuntu_version"."$revision"* "$outdir/"
    test "$first" = no || cp "$stage/mc6_$version.orig.tar.gz" "$outdir/"

    echo "built $series: $(basename "$changes")"

    if test "${UPLOAD:-}" = yes; then
        dput "$ppa" "$outdir/$(basename "$changes")"
    fi
done

cp "$stage/mc6_$version.orig.tar.gz" "$outdir/"
echo "source packages in $outdir"
test "${UPLOAD:-}" = yes || echo "not uploaded: run with UPLOAD=yes, target $ppa"
