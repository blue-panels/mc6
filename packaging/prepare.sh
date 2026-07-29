#!/bin/sh
# Materialise the packaging recipes for one version.
#
# usage: packaging/prepare.sh VERSION [ARCHIVE]
#   packaging/prepare.sh 6.0.3
#   packaging/prepare.sh 6.0.3 dist/mc6-6.0.3.tar.gz
#
# The recipes in this repository carry no version of their own: it comes from
# the release tag, so nothing has to be edited after tagging. This script
# writes the files that name a version, from the committed templates:
#
#   debian/changelog                    from the v* tags
#   packaging/rpm/mc6.spec              from mc6.spec.in
#   packaging/arch/PKGBUILD             from PKGBUILD.in
#   packaging/gentoo/mc6-VERSION.ebuild from mc6.ebuild.in
#
# Both changelogs are meant for a real upload, so their entries come from the
# annotated tag messages and their dates from the tags themselves: the same
# tag always yields the same source package. A version with no tag of its own
# is a test build and is marked UNRELEASED, which stops dput from taking it.
#
# Environment:
#   DEB_DISTRIBUTION   target suite of the leading entry (default: unstable;
#                      a PPA wants the series, e.g. noble)
#   DEB_VERSION_SUFFIX appended to the leading version, e.g. ~ubuntu24.04.1
#
# Given the release archive, the Arch checksum is computed from it. Without
# one the recipe keeps SKIP, and makepkg then says so instead of failing on a
# stale checksum.
set -eu

die() {
    echo "$*" >&2
    exit 1
}

version=${1:?usage: packaging/prepare.sh VERSION [ARCHIVE]}
archive=${2:-}
distribution=${DEB_DISTRIBUTION:-unstable}
suffix=${DEB_VERSION_SUFFIX:-}

case "$version" in
    *[!0-9A-Za-z.+~]* | '')
        die "invalid version: $version"
        ;;
esac

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"

maintainer=$(sed -n 's/^Maintainer: *//p' debian/control)
test -n "$maintainer" || die "cannot read Maintainer from debian/control"

deb_epoch=3
rpm_epoch=$(sed -n 's/^Epoch: *\([0-9][0-9]*\).*/\1/p' packaging/rpm/mc6.spec.in)
test -n "$rpm_epoch" || die "cannot read Epoch from packaging/rpm/mc6.spec.in"

if test -n "$archive"; then
    test -f "$archive" || die "no such archive: $archive"
    checksum=$(b2sum "$archive" | cut -d' ' -f1)
else
    checksum=SKIP
fi

tags=$(git tag --list 'v*' --sort=-version:refname 2>/dev/null || true)

# What the release says is written once, in the signed tag; both changelogs
# then quote it.
tag_body() {
    git tag --list --format='%(contents:subject)%0a%(contents:body)' "$1" 2>/dev/null |
        sed -e '/^[[:space:]]*$/d'
}

deb_entry() {
    printf 'mc6 (%s:%s-1) %s; urgency=medium\n\n' "$deb_epoch" "$1" "$3"
    if test -n "$4"; then
        printf '%s\n' "$4" | sed 's/^/  * /'
    else
        printf '  * Release v%s.\n' "$1"
    fi
    printf '\n -- %s  %s\n\n' "$maintainer" "$2"
}

rpm_entry() {
    printf '* %s %s - %s:%s-1\n' \
        "$(LC_ALL=C date -d "$2" '+%a %b %d %Y')" "$maintainer" "$rpm_epoch" "$1"
    if test -n "$3"; then
        printf '%s\n' "$3" | sed 's/^/- /'
    else
        printf -- '- Release v%s.\n' "$1"
    fi
    printf '\n'
}

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/mc6-prepare.XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

untagged=yes
printf '%s\n' $tags | grep -qx "v$version" && untagged=no

{
    test "$untagged" = no || deb_entry "$version$suffix" "$(date -R)" UNRELEASED ''

    for tag in $tags; do
        tag_date=$(git for-each-ref --format='%(creatordate:rfc2822)' "refs/tags/$tag")
        test -n "$tag_date" || tag_date=$(date -R)

        if test "$tag" = "v$version"; then
            deb_entry "$version$suffix" "$tag_date" "$distribution" "$(tag_body "$tag")"
        else
            deb_entry "${tag#v}" "$tag_date" unstable "$(tag_body "$tag")"
        fi
    done
} > debian/changelog

test -s debian/changelog || die "generated an empty debian/changelog"

{
    test "$untagged" = no || rpm_entry "$version" "$(date -R)" ''

    for tag in $tags; do
        tag_date=$(git for-each-ref --format='%(creatordate:rfc2822)' "refs/tags/$tag")
        test -n "$tag_date" || tag_date=$(date -R)
        rpm_entry "${tag#v}" "$tag_date" "$(tag_body "$tag")"
    done
} > "$tmpdir/rpm-changelog"

sed -e "s/@VERSION@/$version/g" packaging/rpm/mc6.spec.in |
    sed -e "/^@CHANGELOG@\$/r $tmpdir/rpm-changelog" -e "/^@CHANGELOG@\$/d" \
        > packaging/rpm/mc6.spec

sed -e "s/@VERSION@/$version/g" -e "s/@B2SUMS@/$checksum/g" \
    packaging/arch/PKGBUILD.in > packaging/arch/PKGBUILD

rm -f packaging/gentoo/mc6-*.ebuild
cp packaging/gentoo/mc6.ebuild.in "packaging/gentoo/mc6-$version.ebuild"

echo "prepared $version"
echo "  debian/changelog ($distribution)"
echo "  packaging/rpm/mc6.spec"
echo "  packaging/arch/PKGBUILD (b2sums: $checksum)"
echo "  packaging/gentoo/mc6-$version.ebuild"
