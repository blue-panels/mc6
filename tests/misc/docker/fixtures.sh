#!/bin/sh
# Archives for the panel plugin stream tests.  $1 is where they go.
set -e

dir="${1:-/home/mc/archives}"
mkdir -p "$dir"
cd "$dir"

mkdir -p payload
head -c 3000000 /dev/urandom > payload/a.bin
head -c 1500000 /dev/urandom > payload/b.bin
printf 'hello from the sandbox\n' > payload/readme.txt
printf 'кириллица внутри архива\n' > 'payload/привет.txt'

# read in one pass by any consumer
bsdtar -a -cf small.tar.gz payload
bsdtar -a -cf small.zip payload

# 7z keeps its directory at the end: unreadable unless the stream can seek
bsdtar --format 7zip -cf small.7z payload

# past the read-ahead buffer, so seeking is the only way through
mkdir -p payload_big
i=1
while [ "$i" -le 10 ]; do
    head -c 1000000 /dev/urandom > "payload_big/part$i.bin"
    i=$((i + 1))
done
bsdtar --format 7zip -cf big.7z payload_big

# no extension at all: only the content says what it is
cp small.tar.gz noext
cp big.7z sevenzip-without-suffix

# the name lies; Enter must fall through to the plugin's own handling
printf 'plain text, whatever the name says\n' > notanarchive.tar.gz

# an archive inside an archive
bsdtar -cf outer.tar small.zip small.7z

rm -rf payload payload_big
ls -l "$dir"
