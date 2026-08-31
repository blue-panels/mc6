#!/bin/sh
# Build the arcmc cases under $1.
#
# A format that is not compiled in can be registered at runtime in arcmc.ini,
# and Ctrl+PgDn asks every plugin whether it opens the name under the cursor,
# without a magic.ini rule and without a rebuild.  cases/arcmc/config/arcmc.ini
# registers ".pkgx"; the bytes behind it are an ordinary tar.
set -e

dir="${1:-/home/mc/cases/arcmc}"
rm -rf "$dir"
mkdir -p "$dir"
cd "$dir"

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

mkdir -p "$work/payload"
printf 'hello from the sandbox\n' > "$work/payload/readme.txt"
printf 'and one more file\n' > "$work/payload/second.txt"

mkdir -p 01-runtime-format
(cd "$work" && tar --owner=0 --group=0 -cf "$dir/01-runtime-format/bundle.pkgx" payload)
(cd "$work" && tar --owner=0 --group=0 -cf "$dir/01-runtime-format/bundle.tar" payload)

cat > 01-runtime-format/cases.tsv <<'EOF'
file	key	expect	why	transports
bundle.pkgx	key C-PgDn	archive panel	the suffix is known to arcmc.ini alone, and Ctrl+PgDn asks the plugin	local
bundle.pkgx	key C-PgDn,on payload,Enter,on readme.txt,F3	text: hello from the sandbox	and what is inside can be read out of it	local
bundle.tar	key C-PgDn	archive panel	Ctrl+PgDn opens a format that was compiled in the same way	local
bundle.pkgx	Enter	nothing, no error	Enter goes by magic.ini, which says nothing about this suffix	local
EOF

echo "arcmc cases in $dir"
