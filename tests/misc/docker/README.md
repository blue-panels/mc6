# Stream sandbox

A remote host with archives on it and a container that builds this tree, for
trying out what panel plugins do with an input stream.

    tests/misc/docker/sandbox.sh up     # images, remote host, mc -- a few minutes
    tests/misc/docker/sandbox.sh mc     # mc against that host

`sandbox.sh` with no argument lists the rest: `build` after an edit, `check`
to ask every protocol for a listing without a terminal, `shell`, `remote`,
`logs`, `down`, `clean`.

The sources are mounted read-only and copied inside the container, so the
build leaves nothing in the working tree.  `sandbox.sh build` after an edit
reuses the object files in the `work` volume.

## The remote host

`remote`, user `mc`, password `mc`, archives in `~/archives`:

| file                      | what it is for                                     |
|---------------------------|----------------------------------------------------|
| `small.tar.gz`            | reads in one pass, works without seeking            |
| `small.zip`               | same                                                |
| `small.7z`                | directory at the end: needs a seekable stream       |
| `big.7z`                  | ~10 MB, past libarchive's read-ahead buffer         |
| `noext`                   | a tar.gz with no extension: format from content     |
| `sevenzip-without-suffix` | a 7z with no extension                              |
| `notanarchive.tar.gz`     | plain text with a lying name: must be declined      |
| `outer.tar`               | archives inside an archive                          |

The same directory is served four ways, all as user `mc` with password `mc`:

| protocol            | from the `mc` container | from the host              |
|---------------------|-------------------------|----------------------------|
| sftp                | `remote`, port 22       | `localhost`, port 2222     |
| shell link (FISH)   | `remote`, port 22       | `localhost`, port 2222     |
| ftp                 | `remote`, port 21       | `localhost`, port 2121 (passive 21100-21110) |
| samba               | `remote`, share `archives` | `localhost`, port 1445  |

## What to try

**sftp panel** — connect to `remote`, port 22, user `mc`, password `mc`, then
`~/archives`:

- Enter on `small.tar.gz` opens an archive panel without downloading it first.
- Enter on `big.7z` does too.  This is the case that only works because the
  stream can seek: `libssh2_sftp_seek64()` behind `ops->seek`.
- Enter on `noext` and `sevenzip-without-suffix` opens them as well — the
  format comes from the content, not from the name.
- Enter on `notanarchive.tar.gz` must *not* open an archive panel.
- `..` out of the archive returns to the sftp panel with the connection alive.

**shell link panel** — the same host over ssh (`sh://mc@remote`).  Same list.
Here the source cannot cancel a running transfer, so a file arrives in windows
that grow while it is read in order; a seek starts a new one.  `big.7z` is the
interesting case again.

**samba panel** — `remote`, share `archives`, same credentials.  Like ftp, no
`get_input_stream()` yet.

**ftp panel** — `remote`, port 21, same credentials.  The ftp plugin has no
`get_input_stream()` yet, so archives there still go through a download.  That
is the next thing to write, and this is where it will be tried.

## Poking at it by hand

    tests/misc/docker/sandbox.sh remote      # the remote host
    tests/misc/docker/sandbox.sh shell       # the build container
    tests/misc/docker/sandbox.sh clean       # remove containers and the build

Or by hand, if mc is already built:

    docker run --rm -it --network mc-sandbox_default \
        -v mc-sandbox_work:/work mc-sandbox-mc /work/opt/mc/bin/mc

The `mc` container has `ssh` and `curl`, so a transfer can be watched from
outside mc as well.
