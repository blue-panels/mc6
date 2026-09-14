#!/bin/sh
# Build the file operation cases under $1.
#
# Copy, move and delete: what the dialog says before anything is touched, what
# happens when the target is already there, and what is left in the panel
# afterwards.  The cases that change the tree are local only, and the tree is
# built again before each of them.
set -e

dir="${1:-/home/mc/cases/fileops}"
rm -rf "$dir"
mkdir -p "$dir"
cd "$dir"

# ------------------------------------------------------------------ copy ---

mkdir -p 01-copy/target 01-copy/sub
printf 'the file that is copied\n' > 01-copy/copy-me.txt
printf 'the file that is already there\n' > 01-copy/target/copy-me.txt
printf 'inside the subdirectory\n' > 01-copy/sub/inside.txt
# bigger than the 64k tmpfs the sandbox mounts at /small
dd if=/dev/zero of=01-copy/big.bin bs=1024 count=256 2>/dev/null

cat > 01-copy/cases.tsv <<'EOF'
file	key	expect	why	transports
copy-me.txt	F5	text: Follow links	F5 on a file opens the copy dialog	local
copy-me.txt	F5	text: to:	and asks where to put it	local
copy-me.txt	F5,Enter	copy to the other panel	Enter copies it to the other panel	local
copy-me.txt	F5,key C-a,key C-k,type target,Enter	text: File exists	a target that is already there is a question, not an overwrite	local
copy-me.txt	F5,key C-a,key C-k,type target,Enter	text: Overwrite this file?	and the question is which file to keep	local
copy-me.txt	F5,key C-a,key C-k,type target,Enter,key Escape	text: copy-me.txt	Escape leaves the question and the panel as they were	local
sub	F5	text: Copy directory	a directory says so in the dialog	local
big.bin	F5,key C-a,key C-k,type /small,Enter	text: No space left on device	a full filesystem is an error dialog, not a silent half a file	local
EOF

# ------------------------------------------------------------------ move ---

mkdir -p 02-move/target
printf 'the file that is moved\n' > 02-move/move-me.txt
printf 'the file that is already there\n' > 02-move/target/move-me.txt
printf 'the file that is renamed\n' > 02-move/rename-me.txt

cat > 02-move/cases.tsv <<'EOF'
file	key	expect	why	transports
move-me.txt	F6	text: Move file	F6 on a file opens the move dialog	local
rename-me.txt	F6,key C-a,key C-k,type renamed.txt,Enter	text: renamed.txt	a name typed instead of a path is a rename in place	local
rename-me.txt	F6,key C-a,key C-k,type renamed.txt,Enter	no text: rename-me.txt	and the old name is gone from the panel	local
move-me.txt	F6,key C-a,key C-k,type target,Enter	text: Overwrite this file?	a move onto a file that is there asks as a copy does	local
EOF

# ---------------------------------------------------------------- delete ---

mkdir -p 03-delete/full/inner
printf 'the file that is deleted\n' > 03-delete/delete-me.txt
printf 'the file that stays\n' > 03-delete/keep-me.txt
printf 'inside the directory\n' > 03-delete/full/inside.txt
printf 'deeper still\n' > 03-delete/full/inner/deeper.txt

cat > 03-delete/cases.tsv <<'EOF'
file	key	expect	why	transports
delete-me.txt	F8	text: Delete	F8 asks before it deletes	local
delete-me.txt	F8,Enter	no text: delete-me.txt	and the file is gone from the panel afterwards	local
delete-me.txt	F8,Enter	text: keep-me.txt	while the rest of the listing stays	local
delete-me.txt	F8,key Escape	text: delete-me.txt	Escape at the question keeps the file	local
full	F8,Enter	text: not empty	a directory with files in it is a second question	local
full	F8,Enter	text: Delete it recursively?	which says what saying yes means	local
EOF

# ----------------------------------------------------------------- links ---

mkdir -p 04-links
printf 'what the link points at\n' > 04-links/real.txt
ln -s real.txt 04-links/tofile
ln -s nowhere 04-links/dangling

cat > 04-links/cases.tsv <<'EOF'
file	key	expect	why	transports
tofile	F8,Enter	no text: tofile	deleting a link takes the link	local
tofile	F8,Enter	text: real.txt	and leaves what it points at	local
dangling	F8,Enter	no text: dangling	a link to nothing is deleted like any other	local
tofile	F6,key C-a,key C-k,type renamed,Enter	text: @renamed	a renamed link is still a link	local
EOF

echo "fileops cases in $dir"
