#!/bin/sh
# Press the keys of every subject and say what failed.  This is what the
# ci-sandbox workflow runs, and what a person runs before pushing.
#
# usage: ci.sh [env] [-c subjects] [-w transports] [args for test]
#
#   -c   comma separated subjects (default: every directory under cases/)
#   -w   comma separated transports (default: local)
#
# Anything else is handed to "sandbox.sh test" as it is, so -l, -o and -k work
# here too.  A subject with a shells.txt is run once under each shell named in
# it.  One run writes one report directory with a subject in each
# subdirectory and an index.md over them; the exit status is that of the worst
# subject.
set -u

cd "$(dirname "$0")"
root=$(pwd)

env=${MC_SANDBOX:-debian-12}
if [ -n "${1:-}" ] && [ -f "$root/envs/$1/docker-compose.yml" ]; then
    env=$1
    shift
fi

subjects=
transports=local
while getopts "c:w:" opt; do
    case "$opt" in
    c) subjects=$OPTARG ;;
    w) transports=$OPTARG ;;
    *) exit 2 ;;
    esac
done
shift $((OPTIND - 1))

# what is left is for "test"; it is flags, so splitting it on spaces is enough
rest="$*"

if [ -z "$subjects" ]; then
    for d in "$root"/cases/*/; do
        subjects="$subjects $(basename "$d")"
    done
else
    subjects=$(echo "$subjects" | tr ',' ' ')
fi

stamp=$(date +%Y-%m-%dT%H-%M-%S)
run=$stamp-$env-ci
mkdir -p "$root/reports/$run"
index=$root/reports/$run/index.md

{
    echo "# $env: every subject"
    echo
    echo "- started: $stamp"
    echo "- transports: $transports"
    echo
    echo "| subject | verdict | report |"
    echo "|---------|---------|--------|"
} > "$index"

failed=
# one subject, under the name $2 in the report, with $3 of its own flags
run_one ()
{
    echo
    echo "=== $2 ==="
    # shellcheck disable=SC2086
    sh "$root/sandbox.sh" "$env" test \
        -c "$1" -w "$transports" -r "/reports/$run/$2" $3 $rest
    status=$?
    # 2 is the script refusing to run at all: no mc, no fixtures, a shell the
    # image does not have.  That is a run that did not happen, not a failure.
    case $status in
    0)
        echo "| $2 | ok | [index]($2/index.md) |" >> "$index"
        ;;
    2)
        echo "| $2 | not run | - |" >> "$index"
        ;;
    *)
        echo "| $2 | FAILED | [index]($2/index.md) |" >> "$index"
        failed="$failed $2"
        ;;
    esac
}

for subject in $subjects; do
    list=$root/cases/$subject/shells.txt
    if [ -f "$list" ]; then
        # a subject that asks to be run under every shell it names
        for name in $(grep -v '^ *#' "$list" | grep .); do
            run_one "$subject" "$subject-$name" "-s $name"
        done
    else
        run_one "$subject" "$subject" ""
    fi
done

{
    echo
    if [ -z "$failed" ]; then
        echo "No failures."
    else
        echo "Failed:$failed"
    fi
    echo
    echo "- finished: $(date +%Y-%m-%dT%H-%M-%S)"
} >> "$index"

echo
cat "$index"
echo "report: $root/reports/$run"
[ -z "$failed" ]
