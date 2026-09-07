#!/bin/sh
# Build the links cases under $1.
#
# What a plugin panel learned about symbolic links: a link to a directory is
# entered with Enter, F3 and Right, a link to a file is fetched whole, a
# dangling link is marked as such, and the dot files hide on Alt-. as they do
# in a local panel.
set -e

dir="${1:-/home/mc/cases/links}"
rm -rf "$dir"
mkdir -p "$dir"
cd "$dir"

mkdir -p 01-links/real
printf 'inside the real directory\n' > 01-links/real/inside.txt
printf 'the plain file, one line\n' > 01-links/plain.txt
printf 'hidden\n' > 01-links/.hidden.txt
ln -s real 01-links/todir
ln -s plain.txt 01-links/tofile
ln -s nowhere 01-links/dangling
(cd 01-links && tar cf links.tar real plain.txt todir tofile)

cat > 01-links/cases.tsv <<'EOF2'
file	key	expect	why	transports
todir	Enter	text: inside.txt	Enter on a link to a directory enters it
todir	Enter,..	text: plain.txt	and .. comes back to the listing
todir	F3	text: inside.txt	F3 on a link to a directory enters it as well
todir	key Right	text: inside.txt	Right enters a link to a directory in lynx mode (Panels.navigate_with_arrows=true)
todir	key Right,key Left	text: plain.txt	and Left goes back up
tofile	F3	text: the plain file, one line	F3 on a link to a file shows the whole file
links.tar	Enter,on todir,Enter	text: inside.txt	a link inside an archive leads where it says
plain.txt	key Escape	text: ~todir	a link to a directory carries the ~ mark	local,sftp,sh
plain.txt	key Escape	text: @tofile	a link to a file carries the @ mark	local,sftp,sh
plain.txt	key Escape	text: !dangling	a dangling link carries the ! mark	local,sftp,sh
plain.txt	key Escape	text: .hidden.txt	dot files are shown by default
plain.txt	key M-.	no text: .hidden.txt	Alt-. hides the dot files in a plugin panel as in a local one
plain.txt	key M-.	text: plain.txt	and keeps the rest
EOF2

echo "links cases in $dir"
