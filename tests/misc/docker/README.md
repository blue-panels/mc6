# Sandbox

Docker scenarios for trying out panel plugins by hand: a remote host with
archives on it, and a container that builds this tree and runs mc against it.

    tests/misc/docker/sandbox.sh arcmc up     # images, remote host, mc -- a few minutes
    tests/misc/docker/sandbox.sh arcmc mc     # mc against that scenario

The scenario name may be left out; `arcmc` is the default, or whatever
`$MC_SANDBOX` says. `sandbox.sh` with no command lists the rest: `build` after
an edit, `check` to ask every protocol for a listing without a terminal,
`test` to press the keys in the `cases.tsv` files, `shell`, `remote`, `logs`,
`down`, `clean`, and `list` for the scenarios there are.

Sources are mounted read-only and copied inside the container, so a build
leaves nothing in the working tree and reuses its object files between runs.

## Layout

    sandbox.sh              the driver; it holds no list of scenarios
    common/                 what a scenario should not have to write again
      build-mc.sh           copy the tree in, configure, make, install
      check-remote.sh       ask each protocol for a listing
      fixtures.sh           build the scenario directories
      remote/               the host that serves them: sshd, vsftpd, smbd
    scenarios/
      arcmc/                docker-compose.yml, Dockerfile.mc, README.md

## Adding a scenario

Add a directory under `scenarios/` with a `docker-compose.yml` in it. Nothing
else has to change: `sandbox.sh` finds scenarios by looking for that file, and
each is its own compose project with its own network, containers and build
volume, so an existing one is never touched or rebuilt because a new one
appeared.

A scenario that only differs in its environment -- an older distribution,
fewer tools installed, another shell -- is a `Dockerfile.mc` with a different
`FROM` and the same three `COPY` lines from `common/`; `build-mc.sh` does not
care which distribution it is on. One that differs in how the far end behaves
reuses the image and changes `common/remote` through its own compose file.

Build contexts are the sandbox root, which is why the Dockerfiles refer to
`common/...` and `scenarios/<name>/...`.

Scenarios publish no host ports, so several can run side by side; mc reaches
its host over the compose network by the name `remote`. To get at a server
from outside, add a compose override with the ports you want.

## What a scenario contains

`remote`, user `mc`, password `mc`, scenarios in `~/archives`; the mc container
has the same tree in `/work/local` for what needs no server.

One directory per situation, each with a `cases.tsv` of file, key, expected
outcome and reason -- a checklist to read, and the columns `test` walks:

| directory      | what it is for                                            |
|----------------|-----------------------------------------------------------|
| `01-formats`   | tar, zip and 7z, including one past libarchive's buffer    |
| `02-content`   | archives with no extension, and plain text named as one    |
| `03-nested`    | an archive inside an archive, and one inside `uzip://`     |
| `04-non-ascii` | Cyrillic and spaces in names, inside the archives and out  |

The same tree is served four ways, all as user `mc` with password `mc`:
sftp and ssh on port 22, ftp on 21, and the samba share `archives`.

## What to try

The cases.tsv files say what each file is for; what differs is where the panel
is standing when you press the key.

**sftp** and **shell link** supply a stream, so an archive opens without being
downloaded first. `01-formats/big.7z` is the case that only works because the
stream can seek.

**ftp** and **samba** have no `get_input_stream()` yet, so an archive is
fetched to a local copy first.

`02-content` is what happens when the name does not say: `magic.ini` knows
archives by extension, so an archive without one is left alone everywhere, and
plain text called `.tar.gz` gets an error from the operation that was asked to
open it.

**A local panel** in `/work/local` covers the same ground with no server, plus
`03-nested/zip-in-zip.zip` for what happens inside an mc filesystem.

## Pressing the keys

    tests/misc/docker/sandbox.sh arcmc test              # local panel, every cases.tsv
    tests/misc/docker/sandbox.sh arcmc test -w sftp      # the same over sftp: ftp, smb, sh too
    tests/misc/docker/sandbox.sh arcmc test -w sh 01-formats

`run-cases.sh` starts mc under tmux in the case directory (through the plugin's
connection list for a remote one), finds the file by quick search, presses the
key and reads the screen: an `Arcmc:` panel title, the viewer's button bar, an
`Error` box, or none of them. Rows it cannot press or read -- `..`, `cd`, `F5`,
a name that is a situation rather than a file -- are listed as skipped; those
are the checklist for a person. A failure prints the screen it saw.

## Poking at it by hand

    tests/misc/docker/sandbox.sh arcmc remote    # the remote host
    tests/misc/docker/sandbox.sh arcmc shell     # the build container
    tests/misc/docker/sandbox.sh arcmc clean     # remove containers and the build

Or directly, if mc is already built:

    docker run --rm -it --network mc-sandbox-arcmc_default \
        -v mc-sandbox-arcmc_work:/work mc-sandbox-arcmc-mc /work/opt/mc/bin/mc

The `mc` container has `ssh`, `curl` and `smbclient`, so a transfer can be
watched from outside mc as well.
