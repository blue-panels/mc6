#!/bin/sh
# Build the lua viewer cases under $1.
#
# The viewers written in Lua take a file magic.ini binds them to and put up a
# screen of their own: lua-dbf decodes a table itself and draws it on
# mc.ui.screen, lua-readelf runs readelf and shows what it printed.  Both need
# the Lua runtime in the build; lua-dbf needs nothing else, lua-readelf needs
# readelf, which comes with binutils.
set -e

dir="${1:-/home/mc/cases/lua}"
rm -rf "$dir"
mkdir -p "$dir"
cd "$dir"

mkdir -p 01-dbf

# A dBase III table, written byte by byte: a 32 byte header, a 32 byte
# descriptor per field, the terminator, then one record per row with the
# delete mark in front of it.  Three fields of 10, 3 and 1 characters make a
# record of 15 bytes with the mark.
# LC_ALL=C: a byte above 127 would come out as two in a UTF-8 locale
LC_ALL=C awk 'BEGIN {
    # header
    printf "%c", 3                      # dBase III without a memo file
    printf "%c%c%c", 126, 8, 31         # last update: 2026-08-31
    printf "%c%c%c%c", 3, 0, 0, 0       # records
    printf "%c%c", 129, 0               # header length: 32 + 3 * 32 + 1
    printf "%c%c", 15, 0                # record length
    for (i = 0; i < 20; i++) printf "%c", 0

    field("NAME", "C", 10)
    field("AGE", "N", 3)
    field("OK", "L", 1)
    printf "%c", 13                     # end of the descriptors

    record(" ", "alpha     ", " 42", "T")
    record(" ", "bravo     ", " 17", "F")
    record("*", "gone-away ", "  7", "T")
    printf "%c", 26                     # end of the file
}
function field(name, type, len,   i) {
    printf "%s", name
    for (i = length(name); i < 11; i++) printf "%c", 0
    printf "%s", type
    for (i = 0; i < 4; i++) printf "%c", 0
    printf "%c%c", len, 0
    for (i = 0; i < 14; i++) printf "%c", 0
}
function record(mark, a, b, c) { printf "%s%s%s%s", mark, a, b, c }' > 01-dbf/people.dbf

cat > 01-dbf/cases.tsv <<'EOF'
file	key	expect	why	transports
people.dbf	F3	text: alpha	the table is decoded by the script and drawn row per record	local
people.dbf	F3	text: NAME	one column per field, named as the header names it	local
people.dbf	F3	text: gone-away	a record marked deleted is drawn, in red	local
people.dbf	F3,key F4	no text: gone-away	F4 hides the records marked deleted	local
people.dbf	F3,key F2	text: Record length	F2 shows the structure of the table	local
people.dbf	F3,key F2	text: dBase III	and the version the header carries	local
EOF

mkdir -p 02-elf

# Any ELF the image has will do: what is read is its header and its sections,
# not what it does.
cp /bin/true 02-elf/program.bin
ln -sf program.bin 02-elf/link-to-program

cat > 02-elf/cases.tsv <<'EOF'
file	key	expect	why	transports
program.bin	F3	text: ELF Header	readelf is run and what it printed is shown	local
program.bin	F3	text: Section Headers	the sections are in the same view	local
link-to-program	F3	text: ELF Header	a symbolic link is followed when the type is looked up	local
EOF

echo "lua cases in $dir"
