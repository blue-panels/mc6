#!/bin/sh
# Reduce release notes to the two or three lines that describe the release.
#
# usage: summarize_release.sh <notes.md> [<summary.txt>]
#   maint/summarize_release.sh notes.md summary.txt
#
# The long form goes to the release page and the wiki, where there is room for
# it. The short form goes where there is not: the tag message, which is what
# debian/changelog and the RPM %changelog are made of, and the description of
# the milestone.
#
# The summary is made from the notes, not from the pull requests again, so the
# two cannot describe different releases.
#
# In GitHub Actions the built-in token is enough, with "models: read" among the
# workflow permissions. Elsewhere, pass a token in MODELS_TOKEN.
#
# Environment:
#   MODELS_TOKEN     token to authenticate with (default: GITHUB_TOKEN, GH_TOKEN)
#   MODELS_MODEL     model to ask (default: openai/gpt-4.1)
#   MODELS_ENDPOINT  where to ask it
set -eu

die() {
    echo "$*" >&2
    exit 1
}

input=${1:?usage: summarize_release.sh <notes.md> [<summary.txt>]}
output=${2:-}

test -s "$input" || die "no notes to summarise: $input"

model=${MODELS_MODEL:-openai/gpt-4.1}
endpoint=${MODELS_ENDPOINT:-https://models.github.ai/inference/chat/completions}
token=${MODELS_TOKEN:-${GITHUB_TOKEN:-${GH_TOKEN:-}}}
test -n "$token" || die "no token: set MODELS_TOKEN, or GITHUB_TOKEN in a workflow"

command -v curl >/dev/null || die "curl is missing"
command -v jq >/dev/null || die "jq is missing"

system='You are given the release notes of mc6, a fork of GNU Midnight
Commander: a terminal file manager with a built-in editor, viewer and panel
plugins.

Write two or three lines saying what this release is, for someone deciding
whether to update. Name what they will notice; where a release is mostly
groundwork, say that plainly rather than dressing it up.

Put each sentence on a line of its own: two or three lines, one sentence each,
twenty words at most per line. No markdown, no bullets, no heading, no version
number, no closing flourish about the project.

Work strictly from the notes. Do not add a change they do not mention, and do
not promise an effect they do not claim. Answer with the lines and nothing
else.'

request=$(jq -n --arg model "$model" --arg system "$system" \
    --rawfile notes "$input" \
    '{model: $model, temperature: 0.2,
      messages: [{role: "system", content: $system},
                 {role: "user", content: $notes}]}')

response=$(mktemp) || die "cannot create a temporary file"
trap 'rm -f "$response"' EXIT HUP INT TERM

if ! curl -sS --max-time 120 -X POST "$endpoint" \
        -H "Authorization: Bearer $token" \
        -H "Content-Type: application/json" \
        -H "Accept: application/json" \
        -d "$request" > "$response"; then
    die "the request to $endpoint failed"
fi

summary=$(jq -r '.choices[0].message.content // empty' "$response" 2>/dev/null || true)
if test -z "$summary"; then
    echo "the model returned no text; the answer was:" >&2
    head -c 500 "$response" >&2
    echo >&2
    exit 1
fi

# Markdown creeps in however plainly it is forbidden.
summary=$(printf '%s\n' "$summary" |
    sed -e 's/^[[:space:]]*[-*][[:space:]]*//' -e '/^[[:space:]]*#/d' \
        -e '/^[[:space:]]*$/d' -e 's/[[:space:]]*$//')

# Each line becomes an entry of its own in the package changelogs, so a summary
# that came back as one paragraph is broken back into sentences.
if test "$(printf '%s\n' "$summary" | wc -l)" -eq 1 &&
   test "$(printf '%s' "$summary" | wc -c)" -gt 120; then
    summary=$(printf '%s\n' "$summary" | sed -e 's/\([.!?]\) \([A-Z]\)/\1\n\2/g')
fi

test -n "$summary" || die "nothing left of the summary after tidying it"

if test -n "$output"; then
    printf '%s\n' "$summary" > "$output"
    echo "summarised into $output" >&2
else
    printf '%s\n' "$summary"
fi
