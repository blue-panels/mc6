#!/bin/sh
# Build the archive cases under $1.
#
# One directory per situation, each with a cases.tsv saying what the files in
# it are for: name, keys, expected outcome, reason, and the transports it is
# for (empty: all).  A person reads it as a checklist; run-cases.sh walks the
# directories and presses the keys.  A key is one of Enter, F3, F5, C-o, ".."
# (up one level), "on <name>" (put the cursor there), "cd <path>" (the Quick
# cd box), "type <text>"; several go comma separated, in order.
set -e

dir="${1:-/home/mc/cases/archives}"
rm -rf "$dir"
mkdir -p "$dir"
cd "$dir"

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# The same bytes every time, so that sizes and screens can be compared between
# runs.  Poorly compressible, as an archive of real data would be.
bytes ()
{
    LC_ALL=C awk -v n="$1" -v seed="$2" 'BEGIN {
        srand(seed)
        for (i = 0; i < n; i++)
            printf "%c", 1 + int(rand() * 255)
    }'
}

# what goes inside every archive
mkdir -p "$work/payload/каталог с пробелами"
bytes 3000000 1 > "$work/payload/a.bin"
bytes 1500000 2 > "$work/payload/b.bin"
printf 'hello from the sandbox\n' > "$work/payload/readme.txt"
printf 'кириллица внутри архива\n' > "$work/payload/привет.txt"
printf 'и ещё одна строка\n' > "$work/payload/каталог с пробелами/файл.txt"

mkdir -p "$work/payload_big"
i=1
while [ "$i" -le 10 ]; do
    bytes 1000000 "$((10 + i))" > "$work/payload_big/part$i.bin"
    i=$((i + 1))
done

# Fixed times and owner in the entries, none in the gzip header: the same
# archive, byte for byte, wherever it is made.
find "$work" -exec touch -h -d @1000000000 {} +

pack ()
{
    (cd "$work" && bsdtar --uid 0 --gid 0 --uname root --gname root --options 'gzip:!timestamp' "$@")
}

# ---------------------------------------------------------------- formats ---

mkdir -p 01-formats
pack -a -cf "$dir/01-formats/small.tar.gz" payload
pack -a -cf "$dir/01-formats/small.zip" payload
pack --format 7zip -cf "$dir/01-formats/small.7z" payload
pack --format 7zip -cf "$dir/01-formats/big.7z" payload_big

cat > 01-formats/cases.tsv <<'EOF'
file	key	expect	why	transports
small.tar.gz	Enter	archive panel	reads in one pass, no seeking needed
small.zip	Enter	archive panel	same
small.7z	Enter	archive panel	directory at the end of the file: needs seek
big.7z	Enter	archive panel	past libarchive's read-ahead buffer
small.tar.gz	F3	listing	the view operation, not archive.sh
EOF

# ---------------------------------------------------------------- content ---

mkdir -p 02-content
cp 01-formats/small.tar.gz 02-content/noext
cp 01-formats/big.7z 02-content/sevenzip-without-suffix
printf 'plain text, whatever the name says\n' > 02-content/notanarchive.tar.gz

cat > 02-content/cases.tsv <<'EOF'
file	key	expect	why	transports
noext	Enter	nothing, no error	magic.ini knows archives by name only, so a stream is not looked into
sevenzip-without-suffix	Enter	nothing, no error	same: nothing to open it with, and nothing to complain about
notanarchive.tar.gz	Enter	error dialog	the name lies, the operation turns it down and says so
EOF

# ----------------------------------------------------------------- nested ---

mkdir -p 03-nested
touch -d @1000000000 01-formats/*
(cd 01-formats && bsdtar --uid 0 --gid 0 --uname root --gname root -cf "$dir/03-nested/outer.tar" small.zip small.7z)
(cd 01-formats && bsdtar --uid 0 --gid 0 --uname root --gname root -a -cf "$dir/03-nested/zip-in-zip.zip" small.zip small.tar.gz)

cat > 03-nested/cases.tsv <<'EOF'
file	key	expect	why	transports
outer.tar	Enter	archive panel	then Enter on small.zip inside it
outer.tar	Enter,on small.zip,Enter,..,..	the panel it came from	twice: inner archive, outer archive, then sftp or ftp
zip-in-zip.zip	cd zip-in-zip.zip/uzip://	extfs panel	utar:// is gone, uzip:// is the filesystem left to try	local
zip-in-zip.zip	cd zip-in-zip.zip/uzip://,on small.zip,Enter	extfs panel	a file inside an mc filesystem is left to mc.ext.ini	local
EOF

# --------------------------------------------------------------- non-ascii ---

mkdir -p 04-non-ascii
cp 01-formats/small.tar.gz '04-non-ascii/архив.tar.gz'
cp 01-formats/small.7z '04-non-ascii/архив с пробелами.7z'
printf 'просто текст\n' > '04-non-ascii/заметка.txt'
mkdir -p '04-non-ascii/каталог'
printf 'вложенный файл\n' > '04-non-ascii/каталог/файл.txt'

cat > 04-non-ascii/cases.tsv <<'EOF'
file	key	expect	why	transports
архив.tar.gz	Enter	archive panel	the name survives the panel, the quoting and the shell
архив с пробелами.7z	Enter	archive panel	spaces as well as Cyrillic
архив.tar.gz	F5,Enter	copy to the other panel	the name reaches a file operation intact
заметка.txt	C-o,type ls,Enter	the name as written	the subshell is zsh here	local
EOF

find "$dir" -mindepth 1 -maxdepth 2 | sort
