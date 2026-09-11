#!/bin/sh
# Build the command line cases under $1.
#
# Copy and paste on the command line through the clipfile: Ctrl-Insert with
# nothing selected takes the marked files, else the line, else the file under
# the cursor; Shift-Insert pastes the clipfile as one line, with the panels
# shown or hidden, and asks first when it is over 2 KB.  The clipfile for a
# paste is written by the shell in the terminal, so every case is local.
set -e

dir="${1:-/home/mc/cases/cmdline}"
rm -rf "$dir"
mkdir -p "$dir"
cd "$dir"

mkdir -p 01-clip
printf 'a file for the cursor to stand on\n' > 01-clip/alpha.txt
printf 'and a second one to mark\n' > 01-clip/bravo.txt

# The shell writes the clipfile where mc reads it; the directory may not exist yet.
clip='~/.local/share/mc6/mcedit'
w="mkdir -p $clip && printf"

cat > 01-clip/cases.tsv <<EOF2
file	key	expect	why	transports
alpha.txt	key Insert,key C-Insert	clipfile: alpha.txt	Ctrl-Insert with a marked file puts its name in the clipfile	local
alpha.txt	key Insert,key Insert,key C-Insert	clipfile: alpha.txt	every marked file: the first	local
alpha.txt	key Insert,key Insert,key C-Insert	clipfile: bravo.txt	and the second, on a line of its own	local
alpha.txt	type echo LINEMARK,key C-Insert	clipfile: LINEMARK	nothing marked: the whole command line	local
alpha.txt	key Insert,type echo LINEMARK,key C-Insert	clipfile: alpha.txt	the marked files come before the line	local
alpha.txt	key C-Insert	clipfile: alpha.txt	nothing marked and nothing typed: the file under the cursor	local
alpha.txt	key C-o,type $w 'one\\ntwo\\r\\nthree\\n' > $clip/mcedit.clip,key Enter,key C-o,key S-Insert	text: one two three	Shift-Insert pastes the clipfile as one line: line breaks become spaces	local
alpha.txt	key C-o,type $w 'foo\\rbar\\0baz' > $clip/mcedit.clip,key Enter,key C-o,key S-Insert	text: foo bar baz	a lone CR and a NUL byte are breaks too, not glue and not the end	local
alpha.txt	key C-o,type $w 'one\\ntwo\\n' > $clip/mcedit.clip,key Enter,key S-Insert	text: one two	with the panels hidden the paste goes to the shell's line	local
alpha.txt	key C-o,type head -c 3000 /dev/zero | tr '\\0' x > $clip/mcedit.clip,key Enter,key C-o,key S-Insert	text: 2 KB	more than 2 KB asks first	local
alpha.txt	key C-o,type head -c 3000 /dev/zero | tr '\\0' x > $clip/mcedit.clip,key Enter,key C-o,key S-Insert,key Escape	no text: xxxxxxxxxx	and Escape leaves the line alone	local
alpha.txt	key C-o,type head -c 3000 /dev/zero | tr '\\0' x > $clip/mcedit.clip,key Enter,key C-o,key S-Insert,key Enter	text: xxxxxxxxxx	Enter pastes it all the same	local
alpha.txt	key C-o,type $w 'PASTEMARK' > $clip/mcedit.clip,key Enter,key C-o,key S-Insert,type -TAIL,key C-Insert	clipfile: PASTEMARK-TAIL	Ctrl-Insert on the shell's own line copies that line, as it is now	local
EOF2

echo "cmdline cases in $dir"
