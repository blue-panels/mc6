#!/bin/sh
# Rewrite pull request titles into sentences a user of the program would read.
#
# usage: rewrite_titles.sh <prs.json> <titles.json>
#   maint/release_notes.sh v6.0.3 --milestone --raw > prs.json
#   maint/rewrite_titles.sh prs.json titles.json
#   maint/release_notes.sh v6.0.3 --milestone --titles-from titles.json -o notes.md
#
# Input is what release_notes.sh --raw produces: number, title, labels and the
# description its author wrote. Output maps the number to one sentence.
#
# The model rewrites text and nothing else. It never decides what belongs in a
# release: the grouping stays with release_notes.sh, and a number the model
# leaves out keeps the title it had. That way an entry can be worded badly, but
# it cannot be invented or lost.
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

input=${1:?usage: rewrite_titles.sh <prs.json> <titles.json>}
output=${2:?usage: rewrite_titles.sh <prs.json> <titles.json>}

test -f "$input" || die "no such file: $input"

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

# Entries labelled infra never reach the notes, so they are not worth rewriting.
payload=$(jq '[.[] | select((.labels | index("infra")) | not)]' "$input")
test "$(printf '%s' "$payload" | jq 'length')" != 0 || die "nothing to rewrite"

system='You write release notes for mc6, a fork of GNU Midnight Commander: a
terminal file manager with a built-in editor, viewer and panel plugins.

You are given merged pull requests as JSON. Each has a number, a title written
in the language of commits, the labels it carries, and the description its
author wrote.

For each pull request, write ONE short sentence -- twenty words at most --
saying what a person using the program will notice. Take the single most
visible change and leave the rest to the pull request itself: a reader who
wants the detail follows the link. Do not list everything the description
mentions, and do not join clauses with "and" to fit more in.

Start with what changed, not with "This pull request". Where a change is
invisible from the keyboard, say what it was in the plainest words and keep it
under twelve.

Work strictly from the description. Never state a benefit it does not claim,
and never guess at what a change might also do. Leave out function names, file
names and other internal identifiers unless the person at the keyboard types or
sees them. Plain sentences, no markdown, no bullet, no trailing whitespace.

Write plain ASCII: straight quotes, plain hyphens, no typographic dashes,
no ellipsis character. The text goes into a package changelog, where a hyphen
that is not a hyphen cannot be searched for.

Answer with a JSON object and nothing else: the pull request number as a
string, mapped to its sentence. No code fence, no commentary.'

work=$(mktemp -d) || die "cannot create a temporary directory"
trap 'rm -rf "$work"' EXIT HUP INT TERM

printf '%s\n' "$system" > "$work/system"
printf '%s\n' "$payload" > "$work/user"

content=$("$here/models_ask.sh" "$work/system" "$work/user") ||
    die "the model did not answer"

# A fenced block is the one liberty models take with "no code fence". Read from
# the first brace rather than cutting the fence lines out: a model that opens
# both on one line would lose the brace along with the fence.
printf '%s\n' "$content" |
    awk 'started { print; next }
         { i = index($0, "{"); if (i) { started = 1; print substr($0, i) } }' |
    sed -e '/^[[:space:]]*```/d' > "$work/body"

jq -e 'type == "object" and length > 0
       and (to_entries | all(.value | type == "string" and length > 0))' \
    "$work/body" >/dev/null 2>&1 ||
    { echo "the model did not answer with a JSON object of sentences:" >&2
      head -c 500 "$work/body" >&2
      echo >&2
      exit 1; }

mv "$work/body" "$output"
echo "rewrote $(jq 'length' "$output") title(s) into $output" >&2
