#!/bin/sh
# Drive the panel plugin sandbox.  Run it from anywhere.
set -e

cd "$(dirname "$0")"

COMPOSE="docker compose"
$COMPOSE version >/dev/null 2>&1 || COMPOSE="docker-compose"

usage ()
{
    cat <<'EOF'
usage: sandbox.sh <command>

  up        build the images, start the remote host, build mc      (first run)
  mc        run mc against the remote host                         (what you want)
  build     rebuild mc from the working tree, keeping the objects
  check     ask every protocol for a listing, without a terminal
  shell     a shell next to mc, with ssh, curl and smbclient in it
  remote    a shell on the remote host
  logs      what the remote host has to say
  down      stop the containers
  clean     stop them and throw the build away

The remote host is "remote", user "mc", password "mc", archives in
~/archives.  See README.md for what each archive is there to catch.
EOF
}

case "${1:-}" in
up)
    $COMPOSE build
    $COMPOSE up -d remote
    $COMPOSE run --rm mc /usr/local/bin/build-mc.sh
    echo
    echo "ready: $0 mc"
    ;;
mc)
    $COMPOSE up -d remote
    $COMPOSE run --rm mc /work/opt/mc/bin/mc "$@"
    ;;
build)
    $COMPOSE run --rm mc /usr/local/bin/build-mc.sh
    ;;
check)
    $COMPOSE up -d remote
    $COMPOSE run --rm mc /usr/local/bin/check-remote.sh
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
