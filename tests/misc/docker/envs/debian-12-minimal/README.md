# debian-12-minimal

Debian 12 without the programs mc.ext.ini and the extfs helpers run: file(1),
unzip, zip, 7z, bzip2 (the programs are removed, the packages stay: libtool depends on file; xz stays, autopoint unpacks with it). The arcmc plugin does not need them (it uses the
libarchive library), so the archive cases should come out the same as on
debian-12; anything that turns out to need a program should say so in an
error box, not hang or show an empty panel.

    tests/misc/docker/sandbox.sh debian-12-minimal up
    tests/misc/docker/sandbox.sh debian-12-minimal test -w local,sftp

What differs is written in expect.tsv, per case and key.
