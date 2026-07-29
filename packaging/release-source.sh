#!/bin/sh
# Create the release source archive consumed by every packaging recipe.
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

git -C "$root" archive --format=tar --prefix="mc6-${version}/" "$tag" |
    tar -xf - -C "$stage"
printf '%s\n' '#ifndef MC_CURRENT_VERSION' \
    "#define MC_CURRENT_VERSION \"v${version}\"" '#endif' \
    > "$stage/mc6-${version}/mc-version.h"
touch -d "@${commit_time}" "$stage/mc6-${version}/mc-version.h"

tar --sort=name --mtime="@${commit_time}" --owner=0 --group=0 --numeric-owner \
    -C "$stage" -cf - "mc6-${version}" | gzip -n > "$output"
printf '%s\n' "created $output"
