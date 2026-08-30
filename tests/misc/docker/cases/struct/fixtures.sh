#!/bin/sh
# Build the mcstruct cases under $1.
#
# mcstruct shows a binary as a tree of named fields, driven by a def-file.
# magic.ini binds it to what libmagic recognises: a u-boot image, a device
# tree blob, an MBR.  These are the smallest files of those kinds that
# libmagic still names, so F3 on one has to bring the structure up.
set -e

dir="${1:-/home/mc/cases/struct}"
rm -rf "$dir"
mkdir -p "$dir"
cd "$dir"

mkdir -p 01-formats

# A u-boot legacy image: 64 bytes of big-endian header, then the data the
# header says is there.  Fields are laid out as uimage.stl reads them.
{
    printf '\047\005\031\126'          # magic 0x27051956
    printf '\000\000\000\000'          # header crc
    printf '\000\000\000\000'          # timestamp
    printf '\000\000\000\020'          # data size: 16
    printf '\000\010\000\000'          # load address
    printf '\000\010\000\000'          # entry point
    printf '\000\000\000\000'          # data crc
    printf '\005'                      # os: Linux
    printf '\002'                      # architecture
    printf '\002'                      # image type
    printf '\000'                      # compression
    printf 'sandbox uImage'            # image name, 32 bytes with the padding
    printf '\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000'
    printf 'sixteen bytes!!\n'         # the data itself
} > 01-formats/boot.uimage

# An MBR: the boot signature at the end of a 512 byte sector is what names it.
{
    printf '\372\061\300\216\330\216\300\216\320\274\000\174'   # a few bytes of code
    i=12
    while [ "$i" -lt 510 ]; do
        printf '\000'
        i=$((i + 1))
    done
    printf '\125\252'                  # 0x55 0xAA
} > 01-formats/disk.mbr

cat > 01-formats/cases.tsv <<'EOF'
file	key	expect	why	transports
boot.uimage	F3	text: image name	magic.ini sends a u-boot image to mcstruct, which reads it with uimage.stl	local
boot.uimage	F3	text: sandbox uImage	the name stored in the header is read out of the file	local
disk.mbr	F3	text: disk signature	an MBR is shown through mbr.stl	local
disk.mbr	F3	text: Partition	and the partition table is a level of the tree	local
EOF

echo "struct cases in $dir"
