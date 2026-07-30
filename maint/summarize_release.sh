#!/bin/sh
# Reduce release notes to the one line that names the release.
#
# usage: summarize_release.sh <notes.md> [<summary.txt>] [--context <prs.json>]
#   maint/summarize_release.sh notes.md summary.txt --context prs.json
#
# The long form goes to the release page and the wiki, where there is room for
# it. This is the other end: a headline for the tag message and the description
# of the milestone, saying what the release is about rather than what is in it.
#
# The notes leave out the work labelled infra, because a reader of a release
# page does not care for it. A headline is the one place that does: a release
# can be mostly groundwork, and saying so is more honest than naming whichever
# small visible change happened to come with it. So --context takes the full
# list of pull requests, and the headline may draw on those as well.
#
# In GitHub Actions the built-in token is enough, with "models: read" among the
# workflow permissions. Elsewhere, pass a token in MODELS_TOKEN.
#
# Environment:
#   MODELS_TOKEN     token to authenticate with (default: GITHUB_TOKEN, GH_TOKEN)
#   MODELS_MODEL     model to ask (default: openai/gpt-4.1)
#   MODELS_MAX_TOKENS  room for the answer (default: 1500). A reasoning model
#                    spends this on its thinking before it writes anything, so
#                    too little leaves the answer empty rather than short.
#   MODELS_ENDPOINT  where to ask it. Any endpoint of the OpenAI shape does,
#                    such as Cloudflare Workers AI at
#                    .../client/v4/accounts/<id>/ai/v1/chat/completions
set -eu

die() {
    echo "$*" >&2
    exit 1
}

input=""
output=""
context=""

while test $# -gt 0; do
    case "$1" in
        --context)
            context=${2:?--context needs a file}
            shift
            ;;
        -*)
            die "unknown option: $1"
            ;;
        *)
            if test -z "$input"; then input=$1; else output=$1; fi
            ;;
    esac
    shift
done

test -n "$input" || die "usage: summarize_release.sh <notes.md> [<summary.txt>] [--context <prs.json>]"

test -s "$input" || die "no notes to summarise: $input"

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)


system='You are given the release notes of mc6, a fork of GNU Midnight
Commander: a terminal file manager with a built-in editor, viewer and panel
plugins.

Write ONE line naming what this release is about. Fifteen words at most, and
fewer is better.

Name the theme, not the changes. Do not enumerate what happened; the notes
already do that, and whoever wants the detail reads them. Weigh the themes by
how much of the release each accounts for -- count the pull requests -- and
name at most two, the largest first.

Name things, not qualities. Write packaging, plugins, the editor, the viewer,
the build, Debian, Fedora, Arch. Never write improvements, enhancements,
updates, internalization, refactoring, overhaul, or across components: they
say nothing and fill the fifteen words with air.

This is the form to aim for, as an example of length and register only, not of
content:

    Project cleanup and automated packaging for Debian, Fedora, and Arch Linux.

You may be given, after the notes, the full list of merged pull requests --
including the ones kept out of the notes as infrastructure: packaging, build
and repository work. Those count towards the theme. A release that is mostly
groundwork should say so, rather than advertise whichever small visible change
came along with it.

No markdown, no bullet, no heading, no version number, no praise of the
project.

Write plain ASCII: straight quotes, plain hyphens, no typographic dashes,
no ellipsis character. The text goes into a package changelog, where a hyphen
that is not a hyphen cannot be searched for. Work strictly from what you are given: never name a theme it does not
support. Answer with the line and nothing else.'

user=$(cat "$input")
if test -n "$context"; then
    test -f "$context" || die "no such file: $context"
    user=$(printf '%s\n\n# Every pull request in the release, notes or not\n\n%s\n' \
        "$user" "$(jq -r '.[] | "- [\(.labels | join(", "))] \(.title)"' "$context")")
fi

work=$(mktemp -d) || die "cannot create a temporary directory"
trap 'rm -rf "$work"' EXIT HUP INT TERM

printf '%s\n' "$system" > "$work/system"
printf '%s\n' "$user" > "$work/user"

summary=$("$here/models_ask.sh" "$work/system" "$work/user") ||
    die "the model did not answer"

# Markdown creeps in however plainly it is forbidden.
summary=$(printf '%s\n' "$summary" |
    sed -e 's/^[[:space:]]*[-*][[:space:]]*//' -e '/^[[:space:]]*#/d' \
        -e '/^[[:space:]]*$/d' -e 's/[[:space:]]*$//')

# One line, whatever came back: the first is the headline, and anything after it
# is the model carrying on.
summary=$(printf '%s\n' "$summary" | head -n 1)

test -n "$summary" || die "nothing left of the summary after tidying it"

if test -n "$output"; then
    printf '%s\n' "$summary" > "$output"
    echo "summarised into $output" >&2
else
    printf '%s\n' "$summary"
fi
