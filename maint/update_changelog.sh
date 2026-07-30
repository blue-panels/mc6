#!/bin/sh
# Put a version's entries into CHANGELOG.md.
#
# usage: update_changelog.sh <version> <entries-file> [<changelog>]
#   maint/release_notes.sh v6.0.3 --milestone --flat > entries.txt
#   maint/update_changelog.sh 6.0.3 entries.txt
#
# The entries are plain lines, one per change, as release_notes.sh --flat writes
# them. This file is what the package changelogs are made of: prepare.sh takes
# the section for the version being built, so what lands in a .deb or an .rpm is
# what was committed here, the same on every rebuild.
#
# A section that is already there is replaced, so the command can be run again
# after the wording changes.
#
# Environment:
#   DATE  the date of the section (default: today)
set -eu

die() {
    echo "$*" >&2
    exit 1
}

version=${1:?usage: update_changelog.sh <version> <entries-file> [<changelog>]}
entries=${2:?usage: update_changelog.sh <version> <entries-file> [<changelog>]}

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
changelog=${3:-$root/CHANGELOG.md}

# The tag carries a v, the section does not: a version is a number.
version=${version#v}
case "$version" in
    *[!0-9.]* | '' | .* | *.)
        die "not a release version: $version"
        ;;
esac

test -s "$entries" || die "no entries to add: $entries"
grep -q '^- ' "$entries" || die "$entries has no entries: expected lines starting with '- '"

date=${DATE:-$(date +%Y-%m-%d)}

if ! test -f "$changelog"; then
    printf '# Changelog\n\nThe releases of this fork, newest first.\n\n' > "$changelog"
fi

new=$(mktemp) || die "cannot create a temporary file"
trap 'rm -f "$new"' EXIT HUP INT TERM

# Replacing happens where the section already is; a new section goes above the
# newest one. Told apart beforehand, because a section replaced at the top of
# the file would jump the order of the releases below it.
if grep -qE "^## $version([^0-9.]|\$)" "$changelog"; then
    mode=replace
else
    mode=insert
fi

awk -v version="$version" -v date="$date" -v entries="$entries" -v mode="$mode" '
function emit_section() {
    printf "## %s - %s\n\n", version, date
    while ((getline line < entries) > 0)
        if (line ~ /^- /)
            print line
    close(entries)
    print ""
}
# The section of this version, if it is already here, is replaced rather than
# doubled: the wording changes more than once before a release goes out.
$0 ~ "^## " version "([^0-9.]|$)" {
    emit_section()
    written = 1
    skipping = 1
    next
}
skipping && /^## / { skipping = 0 }
skipping { next }
mode == "insert" && /^## / && !written {
    emit_section()
    written = 1
}
{ print }
END {
    if (!written)
        emit_section()
}
' "$changelog" > "$new"

# awk cannot tell us whether it replaced or inserted, so check the outcome.
grep -q "^## $version - " "$new" || die "the section for $version did not get written"

mv "$new" "$changelog"
echo "$changelog: section $version - $date" >&2
