# Sandbox

Docker environments for trying out mc by hand and for pressing the keys from
a script: a remote host with cases on it, and a container that builds this
tree and runs mc against it.

    tests/misc/docker/sandbox.sh debian-12 up     # images, remote host, mc -- a few minutes
    tests/misc/docker/sandbox.sh debian-12 mc     # mc against that environment
    tests/misc/docker/sandbox.sh debian-12 test   # press the keys in every cases.tsv
    tests/misc/docker/sandbox.sh ui               # the same, chosen from menus

The environment name may be left out; `debian-12` is the default, or whatever
`$MC_SANDBOX` says. `sandbox.sh` with no command lists the rest: `build` after
an edit, `check` to ask every protocol for a listing without a terminal,
`shell`, `remote`, `logs`, `down`, `clean`, and `list` for what there is.

Sources are mounted read-only and copied inside the container, so a build
leaves nothing in the working tree and reuses its object files between runs.

## Layout

    sandbox.sh              the driver; it holds no list of environments
    common/                 what an environment should not have to write again
      build-mc.sh           copy the tree in, configure, make, install
      features.ini          build profiles for build -f
      run-cases.sh          press the keys, read the screen, write the report
      ui.sh                 menus that compose a test command
      check-remote.sh       ask each protocol for a listing
      keymaps/              mc.keymap files for test -k
      remote/               the host that serves the cases: sshd, vsftpd, smbd
    envs/
      debian-12/            docker-compose.yml, Dockerfile.mc, README.md, expect.tsv
    cases/
      archives/fixtures.sh  the files and the cases.tsv of one subject
    reports/                what a run leaves behind (not in git)

Three axes, chosen independently: the **environment** (which image mc is
built and run in), the **subject** (which cases), and how mc is run there
(**transport**, locale, ini values, keymap, build profile).

## Adding an environment

Add a directory under `envs/` with a `docker-compose.yml` in it. Nothing else
has to change: `sandbox.sh` finds environments by looking for that file, and
each is its own compose project with its own network, containers and build
volume, so an existing one is never touched or rebuilt because a new one
appeared.

One that only differs in what is installed -- an older distribution, fewer
tools, another shell -- is a `Dockerfile.mc` with a different `FROM` and the
same `COPY` lines from `common/`; `build-mc.sh` does not care which
distribution it is on. One that differs in how the far end behaves reuses the
image and changes `common/remote` through its own compose file.

Base images are pinned by digest, so a run is the same run next month; moving
to a newer image is a change to the Dockerfile.

Build contexts are the sandbox root, which is why the Dockerfiles refer to
`common/...`, `cases/...` and `envs/<name>/...`. The compose file sets
`SANDBOX_ENV`, which is how `run-cases.sh` finds the environment's
`expect.tsv`.

Environments publish no host ports, so several can run side by side; mc
reaches its host over the compose network by the name `remote`.

## What a subject contains

`cases/<subject>/fixtures.sh` builds a directory of files and, in each
subdirectory, a `cases.tsv` of file, key, expected outcome and reason -- a
checklist to read, and the columns `test` walks. The bytes are generated from
a seed, so sizes and screens are the same from run to run.

The remote host builds every subject under `/home/mc/cases/<subject>` and
serves it four ways, all as user `mc` with password `mc`: sftp and ssh on port
22, ftp on 21 (`/cases/<subject>`), and the samba share `cases`. The mc
container has the same tree in `/work/local/<subject>` for what needs no
server.

### archives

| directory      | what it is for                                            |
|----------------|-----------------------------------------------------------|
| `01-formats`   | tar, zip and 7z, including one past libarchive's buffer    |
| `02-content`   | archives with no extension, and plain text named as one    |
| `03-nested`    | an archive inside an archive, and one inside `uzip://`     |
| `04-non-ascii` | Cyrillic and spaces in names, inside the archives and out  |

**sftp** and **shell link** supply a stream, so an archive opens without being
downloaded first. `01-formats/big.7z` is the case that only works because the
stream can seek. **ftp** and **samba** have no `get_input_stream()` yet, so an
archive is fetched to a local copy first.

`02-content` is what happens when the name does not say: `magic.ini` knows
archives by extension, so an archive without one is left alone everywhere, and
plain text called `.tar.gz` gets an error from the operation that was asked to
open it.

## Pressing the keys

    sandbox.sh debian-12 test                          # archives, local panel
    sandbox.sh debian-12 test -w local,sftp,ftp,smb,sh # over every transport
    sandbox.sh debian-12 test -w sh 01-formats         # one directory
    sandbox.sh debian-12 test -l ru_RU.KOI8-R          # an 8-bit locale
    sandbox.sh debian-12 test -o old_esc_mode=true -k shift-tab-complete
    sandbox.sh debian-12 build -f all,ncurses && sandbox.sh debian-12 test

`run-cases.sh` starts mc under tmux in the case directory (through the
plugin's connection list for a remote one), finds the file by quick search,
presses the keys and reads what came of it. A `cases.tsv` row is file, keys,
expectation, reason, and optionally the transports it is for. The keys go
comma separated, in order: `Enter`, `F3`, `F5`, `C-o`, `..` (up one level),
`on <name>` (the cursor goes there), `cd <path>` (the Quick cd box),
`type <text>`. The expectations: `archive panel`, `listing`, `error dialog`,
`nothing, no error`, `the panel it came from`, `extfs panel`, `copy to the
other panel` (the file is then in `/tmp`, as big as mc said), `the name as
written` (the shell printed it). mc's stderr is read as well; an assertion
or a critical warning fails the case whatever the screen shows. A row with
keys or an expectation the script does not know is listed as skipped.

`-o` writes ini values before mc starts (`section.key=value`, the section
`Midnight-Commander` when left out), `-k` puts a keymap from `common/keymaps/`
in place, `-l` picks the locale mc runs in (messages stay English so that
the screen can be read). `build -f` picks a profile from
`common/features.ini`; each set of features has its own build and install
directory, and `test` runs the one built last.

An environment that expects something else -- no archiver installed, so
`small.7z` gives an error rather than a panel -- says so in its
`envs/<name>/expect.tsv`: `dir/file`, key, expectation, and optionally the
transports it applies to.

### Under valgrind

    sandbox.sh debian-12 build -f all,debug     # -O0 -g3, so a stack reads
    sandbox.sh debian-12 test -g 01-formats     # the same cases under memcheck

`-g` starts mc under `valgrind --tool=memcheck` with
`common/valgrind.supp`, which holds what the libraries never free and nothing
of mc's own.  Every wait is multiplied by six, `$SLOW` sets another factor,
and mc is asked to quit with F10 instead of being killed, because a killed
process writes no summary.  A case fails on an invalid read, write or free,
on a jump on an uninitialised value, whatever the screen shows; what was
definitely lost is written down and left to a person, since mc frees little
on the way out by design.

The log of each case is kept next to its screen as `<case>.<key>.valgrind`,
`<transport>/valgrind.tsv` counts them, and `index.md` gains a Memory table
with a link to every log worth opening.  Only `debian-12` has valgrind in its
image; `-g` elsewhere says so and stops.

A case takes minutes rather than seconds, so a run under memcheck is one
directory at a time, not the whole subject over every transport.

### Reports

Every run writes `reports/<stamp>-<env>/`: `index.md` with a table per
transport and the list of failures, and under `<transport>/` a `results.tsv`
(case, key, expectation, verdict, milliseconds, reason), the screen of every
failure, and mc's stderr per case. `index.md` is what goes into a release
issue.

## Poking at it by hand

    sandbox.sh debian-12 remote    # the remote host
    sandbox.sh debian-12 shell     # the build container
    sandbox.sh debian-12 clean     # remove containers and the build

Or directly, if mc is already built:

    docker run --rm -it --network mc-sandbox-debian-12_default \
        -v mc-sandbox-debian-12_work:/work mc-sandbox-debian-12-mc /work/opt/mc/bin/mc

The `mc` container has `ssh`, `curl` and `smbclient`, so a transfer can be
watched from outside mc as well.
