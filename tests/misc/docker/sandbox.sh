#!/bin/sh
# Drive a sandbox environment.  Run it from anywhere.
#
# An environment is a directory under envs/ with a docker-compose.yml in it.
# They are found by looking, not by being listed here, so adding one is adding
# a directory and nothing else.
set -e

cd "$(dirname "$0")"
root=$(pwd)

COMPOSE="docker compose"
$COMPOSE version >/dev/null 2>&1 || COMPOSE="docker-compose"

envs ()
{
    for d in "$root"/envs/*/; do
        [ -f "$d/docker-compose.yml" ] && basename "$d"
    done
}

usage ()
{
    cat <<EOF
usage: sandbox.sh [env] <command> [args]

  up        build the images, start the remote host, build mc      (first run)
  mc        run mc against that environment                        (what you want)
  build     rebuild mc from the working tree: build [-f profiles]
  check     ask every protocol for a listing, without a terminal
  test      press the keys in the cases: test [-c subject] [-w transports]
            [-l locale] [-o key=value]... [-k keymap] [dir...]
  ui        the same, chosen from menus
  shell     a shell next to mc, with ssh, curl and smbclient in it
  remote    a shell on the remote host
  logs      what the remote host has to say
  down      stop the containers
  clean     stop them and throw the build away
  list      the environments, subjects, transports and profiles there are

environments: $(envs | tr '\n' ' ')
default:      \$MC_SANDBOX (currently ${MC_SANDBOX:-debian-12})

Each environment is its own compose project, so two of them do not share a
network, a container or a build.  See envs/<name>/README.md for what one is
meant to catch, and README.md for the rest.
EOF
}

list ()
{
    echo "environments: $(envs | tr '\n' ' ')"
    echo "subjects:     $(for d in "$root"/cases/*/; do basename "$d"; done | tr '\n' ' ')"
    echo "transports:   local sftp ftp smb sh"
    echo "profiles:     $(sed -n 's/^\[\(.*\)\]$/\1/p' "$root/common/features.ini" | tr '\n' ' ')"
    echo "keymaps:      $(ls "$root"/common/keymaps/ 2>/dev/null | sed 's/\.keymap$//' | tr '\n' ' ')"
    echo "locales:      ru_RU.UTF-8 en_US.UTF-8 ru_RU.KOI8-R C"
}

# The environment may be named first; otherwise the default one is used.
env=${MC_SANDBOX:-debian-12}
if [ -n "${1:-}" ] && [ -f "$root/envs/$1/docker-compose.yml" ]; then
    env=$1
    shift
fi

command="${1:-}"
[ -n "$command" ] && shift || true

case "$command" in
list)
    list
    exit 0
    ;;
ui)
    # the curses one takes the mouse; the dialog one is for a host without python
    if command -v python3 >/dev/null 2>&1; then
        exec python3 "$root/common/ui.py" "$env" "$@"
    fi
    exec sh "$root/common/ui.sh" "$env" "$@"
    ;;
esac

if [ ! -f "$root/envs/$env/docker-compose.yml" ]; then
    echo "sandbox.sh: no such environment: $env" >&2
    echo "have: $(envs | tr '\n' ' ')" >&2
    exit 1
fi

cd "$root/envs/$env"

case "$command" in
up)
    $COMPOSE build
    $COMPOSE up -d remote
    $COMPOSE run --rm mc /usr/local/bin/build-mc.sh "$@"
    echo
    echo "ready: $0 $env mc"
    ;;
mc)
    $COMPOSE up -d remote
    $COMPOSE run --rm mc /work/opt/mc/bin/mc "$@"
    ;;
build)
    $COMPOSE run --rm mc /usr/local/bin/build-mc.sh "$@"
    ;;
check)
    $COMPOSE up -d remote
    $COMPOSE run --rm mc /usr/local/bin/check-remote.sh
    ;;
test)
    $COMPOSE up -d remote
    $COMPOSE run --rm mc /usr/local/bin/run-cases.sh "$@"
    ;;
shell)
    $COMPOSE up -d remote
    $COMPOSE run --rm mc bash
    ;;
remote)
    $COMPOSE up -d remote
    $COMPOSE exec remote bash
    ;;
logs)
    $COMPOSE logs remote
    ;;
down)
    $COMPOSE down
    ;;
clean)
    $COMPOSE down -v
    ;;
*)
    usage
    exit 1
    ;;
esac
