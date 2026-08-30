#!/bin/sh
# Build the panel cases under $1.
#
# What the panel got in this milestone: a quick filter that hides what does
# not match and shares its pattern with quick search, a quick cd that is an
# input line in the panel rather than a dialog, and a permission field with a
# colour per bit.
set -e

dir="${1:-/home/mc/cases/panel}"
rm -rf "$dir"
mkdir -p "$dir"
cd "$dir"

mkdir -p 01-filter/sub
printf 'first\n'  > 01-filter/alpha-one.txt
printf 'second\n' > 01-filter/alpha-two.txt
printf 'third\n'  > 01-filter/bravo-one.txt
printf 'fourth\n' > 01-filter/bravo-two.txt
printf 'inside the subdirectory\n' > 01-filter/sub/inside.txt

cat > 01-filter/cases.tsv <<'EOF'
file	key	expect	why	transports
alpha-one.txt	key Escape,key M-S,type alpha	no text: bravo-one.txt	the quick filter hides what does not match	local
alpha-one.txt	key Escape,key M-S,type alpha	text: alpha-two.txt	and keeps what does	local
alpha-one.txt	key Escape,key M-S,type alpha,key C-g	text: bravo-one.txt	Ctrl-G brings the whole listing back	local
alpha-one.txt	key M-c,type sub,key Enter	text: inside.txt	quick cd is an input line in the panel	local
alpha-one.txt	key M-?	text: Find File	the find dialog is reachable from the panel	local
EOF

# The permission field takes a colour per bit, which a captured screen has no
# way of showing: the text is the same either way.
mkdir -p 02-permissions
printf 'plain\n'      > 02-permissions/plain.txt
printf 'executable\n' > 02-permissions/runnable.sh
chmod 755 02-permissions/runnable.sh
printf 'setuid\n'     > 02-permissions/setuid.bin
chmod 4755 02-permissions/setuid.bin
printf 'nobody\n'     > 02-permissions/locked.txt
chmod 400 02-permissions/locked.txt

cat > 02-permissions/cases.tsv <<'EOF'
file	key	expect	why	transports
runnable.sh	key F9	r, w, x, s and - each in a colour of its own	for a person: turn Permission colors on in the panel options; a captured screen carries no colour	local
setuid.bin	key F9	the s bit in the colour of a special bit	for a person: same, and the marked triplet keeps its colour on top	local
EOF

echo "panel cases in $dir"
