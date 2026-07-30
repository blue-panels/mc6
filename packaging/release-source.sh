#!/bin/sh
# Create the release source archive consumed by every packaging recipe.
#
# The archive is bootstrapped: autogen.sh runs inside it, so it carries
# configure, the Makefile.in files, po/Makefile.in.in and po/POTFILES.in. None
# of those is in Git, and a recipe cannot make them all for itself -- autoreconn
# needs autopoint for po/Makefile.in.in, and po/POTFILES.in comes from the
# xgettext pass in autogen.sh. What is published is therefore a release tarball
# rather than a snapshot of the tree, and the recipes only configure and build.
#
# The consequence is that the bytes depend on the autoconf, automake, libtool
# and gettext versions that ran here. Take the checksum for a recipe from the
# archive that was published, not from one rebuilt elsewhere.
set -eu

version=${1:?usage: packaging/release-source.sh VERSION [TAG [OUTPUT]]}
tag=${2:-v${version}}
output=${3:-dist/mc6-${version}.tar.gz}

case "$version" in
    *[!0-9A-Za-z.+~:-]* | '')
        echo "invalid version: $version" >&2
        exit 2
        ;;
esac

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
output_dir=$(dirname -- "$output")

git -C "$root" rev-parse --verify --quiet "${tag}^{commit}" >/dev/null
commit_time=$(git -C "$root" show -s --format=%ct "${tag}^{commit}")
if [ -e "$output" ]; then
    echo "refusing to overwrite existing archive: $output" >&2
    exit 1
fi

mkdir -p "$output_dir"
stage=$(mktemp -d "${TMPDIR:-/tmp}/mc6-release.XXXXXX")
trap 'rm -rf "$stage"' EXIT HUP INT TERM

for tool in autoreconf autopoint xgettext; do
    command -v "$tool" >/dev/null ||
        { echo "$tool is missing (install autoconf, automake, libtool, gettext, autopoint)" >&2
          exit 2; }
done

git -C "$root" archive --format=tar --prefix="mc6-${version}/" "$tag" |
    tar -xf - -C "$stage"

# Before autogen.sh: its version.sh step keeps an existing mc-version.h, and
# outside a Git checkout it has nothing else to go by.
printf '%s\n' '#ifndef MC_CURRENT_VERSION' \
    "#define MC_CURRENT_VERSION \"v${version}\"" '#endif' \
    > "$stage/mc6-${version}/mc-version.h"

(cd "$stage/mc6-${version}" && ./autogen.sh >/dev/null)
rm -rf "$stage/mc6-${version}/autom4te.cache"

# One timestamp for everything. Make rebuilds only on a strictly newer
# prerequisite, so equal times leave the generated files alone instead of
# asking for autotools at build time.
tar --sort=name --mtime="@${commit_time}" --owner=0 --group=0 --numeric-owner \
    -C "$stage" -cf - "mc6-${version}" | gzip -n > "$output"
printf '%s\n' "created $output"
