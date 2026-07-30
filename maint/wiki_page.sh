#!/bin/sh
# Write the release page for the wiki.
#
# usage: wiki_page.sh <version> [<out>]
#   maint/wiki_page.sh v6.0.2 ~/dev/mcdev.wiki/Release-v6.0.2.md
#
# This is the long form. Where CHANGELOG.md gets one line per change and the tag
# gets a headline, here each change gets a paragraph: what it does, how it is
# reached, what it replaces. The material is the description its author wrote
# under ## Summary, which is long enough for that and is otherwise thrown away.
#
# The page is assembled here and not by the model. Grouping stays with
# release_notes.sh, the model writes prose for one entry at a time, and an entry
# it leaves out keeps its title. So the page can read poorly; it cannot claim a
# change that was not made, or quietly lose one.
#
# Environment:
#   MODELS_TOKEN     token to authenticate with (default: GITHUB_TOKEN, GH_TOKEN)
#   MODELS_MODEL     model to ask (default: openai/gpt-4.1)
#   MODELS_ENDPOINT  where to ask it. Any endpoint of the OpenAI shape does,
#                    such as Cloudflare Workers AI at
#                    .../client/v4/accounts/<id>/ai/v1/chat/completions
#   WIDTH            where to wrap the prose (default: 79)
set -eu

die() {
    echo "$*" >&2
    exit 1
}

version=${1:?usage: wiki_page.sh <version> [<out>]}
output=${2:-}

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
width=${WIDTH:-79}

model=${MODELS_MODEL:-openai/gpt-4.1}
endpoint=${MODELS_ENDPOINT:-https://models.github.ai/inference/chat/completions}
token=${MODELS_TOKEN:-${GITHUB_TOKEN:-${GH_TOKEN:-}}}
test -n "$token" || die "no token: set MODELS_TOKEN, or GITHUB_TOKEN in a workflow"

command -v curl >/dev/null || die "curl is missing"
command -v jq >/dev/null || die "jq is missing"

work=$(mktemp -d) || die "cannot create a temporary directory"
trap 'rm -rf "$work"' EXIT HUP INT TERM

# ask <system-prompt> <user-content> -- prints the answer
ask() {
    jq -n --arg model "$model" --arg system "$1" --arg user "$2" \
        '{model: $model, temperature: 0.2,
          messages: [{role: "system", content: $system},
                     {role: "user", content: $user}]}' > "$work/request.json"

    curl -sS --max-time 180 -X POST "$endpoint" \
        -H "Authorization: Bearer $token" \
        -H "Content-Type: application/json" \
        -H "Accept: application/json" \
        -d @"$work/request.json" > "$work/response.json" ||
        die "the request to $endpoint failed"

    jq -r '.choices[0].message.content // empty' "$work/response.json" 2>/dev/null ||
        die "the model returned nothing usable"
}

"$here/release_notes.sh" "$version" --milestone --raw > "$work/prs.json"
payload=$(jq '[.[] | select((.labels | index("infra")) | not)]' "$work/prs.json")
test "$(printf '%s' "$payload" | jq 'length')" != 0 || die "nothing to write about"

paragraph_system='You write the release page for mc6, Midnight Commander with
Plugins: a terminal file manager with a built-in editor, viewer, embedded
terminal and panel plugins.

You are given merged pull requests as JSON: a number, a title in the language
of commits, labels, and the description its author wrote.

For each, write a paragraph for someone who uses the program.

Open with the thing itself in bold, as a noun phrase, then a full stop:
**Code folding.** or **Undo history browser** (Alt+Shift+U). Then two to five
sentences: what it does, how it is reached, what it replaces, what it fixes.
Where the reader types a key or sees an identifier, set it in backticks --
`Alt+Shift+F`, `mc.macros`, `{`. Otherwise no markup, and no bullet: the list
is added later.

Work strictly from the description. Every sentence must be traceable to it;
where it is thin, write one sentence and stop. Never invent an option, a key or
a benefit. Leave out file names, function names and internal identifiers unless
the reader meets them.

Answer with the paragraph and nothing else: no bullet, no heading, no code
fence, no line breaks. The page is wrapped afterwards.'

# One request per entry. Asked for all of them at once, the answer is cut off
# part way through and the whole page is lost with it; asked one at a time, each
# answer is short, and an entry that fails keeps its title while the rest stand.
echo '{}' > "$work/paragraphs.json"
for number in $(printf '%s' "$payload" | jq -r '.[].number'); do
    entry=$(printf '%s' "$payload" | jq --argjson n "$number" '[.[] | select(.number == $n)]')
    paragraph=$(ask "$paragraph_system" "$entry" |
        sed -e '/^[[:space:]]*```/d' -e '/^[[:space:]]*$/d' \
            -e 's/^[[:space:]]*[-*][[:space:]][[:space:]]*//' |
        tr '\n' ' ' | sed -e 's/[[:space:]][[:space:]]*/ /g' -e 's/^ //' -e 's/ $//')

    if test -z "$paragraph"; then
        echo "::warning::no paragraph for #$number; keeping its title" >&2
        continue
    fi

    jq --arg n "$number" --arg p "$paragraph" '. + {($n): $p}' \
        "$work/paragraphs.json" > "$work/paragraphs.next" &&
        mv "$work/paragraphs.next" "$work/paragraphs.json"
    echo "#$number written" >&2
done

test "$(jq 'length' "$work/paragraphs.json")" != 0 ||
    die "not one paragraph came back; the page would be the titles over again"

"$here/release_notes.sh" "$version" --milestone \
    --titles-from "$work/paragraphs.json" > "$work/body.md"

intro_system='You are given the body of a release page for mc6, Midnight
Commander with Plugins: a terminal file manager with a built-in editor, viewer,
embedded terminal and panel plugins.

Write the opening: two paragraphs, three at the most, before the list begins.

Say what the centre of this release is, and put the rest around it. Where a
release has no centre, say what it is made of instead -- honestly, without
building a theme that is not there. Name things rather than qualities; a reader
scrolling past wants to know whether this release concerns them.

Plain prose. No heading, no bullet, no version number, no closing sentence
about the project as a whole. Keep bold for the names of things, as the body
does. Work strictly from the body: never add a change it does not list.

Answer with the paragraphs and nothing else.'

intro=$(ask "$intro_system" "$(cat "$work/body.md")")
printf '%s\n' "$intro" | sed -e '/^[[:space:]]*```/d' -e '/^[[:space:]]*#/d' > "$work/intro.raw"
test -s "$work/intro.raw" || die "the model wrote no opening"

# The opening is prose and arrives as one line per paragraph; wrap it too, or
# the page begins with lines nobody can read in a terminal.
awk -v width="$width" '
function wrap(text,   n, i, words, line) {
    n = split(text, words, " ")
    line = ""
    for (i = 1; i <= n; i++) {
        if (line == "")
            line = words[i]
        else if (length(line) + 1 + length(words[i]) <= width)
            line = line " " words[i]
        else {
            print line
            line = words[i]
        }
    }
    if (line != "")
        print line
}
/^[[:space:]]*$/ { print ""; next }
{ wrap($0) }
' "$work/intro.raw" > "$work/intro.md"

# The body arrives as one long line per entry. Wrap it the way the page reads:
# continuation lines indented under the bullet, and the link left whole.
awk -v width="$width" '
function flush_paragraph(text,   line, word, n, i, words, indent) {
    n = split(text, words, " ")
    line = ""
    indent = "- "
    for (i = 1; i <= n; i++) {
        if (line == "")
            line = indent words[i]
        else if (length(line) + 1 + length(words[i]) <= width)
            line = line " " words[i]
        else {
            print line
            indent = "  "
            line = indent words[i]
        }
    }
    if (line != "")
        print line
}
/^- / {
    text = $0
    sub(/^- /, "", text)
    # The link ends the entry on a line of its own, as it does not wrap.
    if (match(text, / \(\[#[0-9]+\]\([^)]*\)\)$/)) {
        link = substr(text, RSTART + 1)
        text = substr(text, 1, RSTART - 1)
        flush_paragraph(text)
        print "  " link
    } else
        flush_paragraph(text)
    next
}
{ print }
' "$work/body.md" > "$work/wrapped.md"

# A rule between the top-level sections, as the page has.
awk 'NR == 1 { print; next }
     /^## / { print "---"; print "" }
     { print }' "$work/wrapped.md" > "$work/sections.md"

date=$(git -C "$here/.." for-each-ref --format='%(creatordate:short)' "refs/tags/$version" 2>/dev/null)
test -n "$date" || date=$(date +%Y-%m-%d)

{
    printf '# %s (%s)\n\n' "$version" "$date"
    cat "$work/intro.md"
    # Its own heading is dropped: the page has one already, and so are the blank
    # lines that would otherwise pile up before the first rule.
    sed -e '1{/^# /d;}' "$work/sections.md" |
        awk 'NF == 0 { blank++; next } { while (blank-- > 1) ; if (blank >= 0) print ""; blank = 0; print }'
} > "$work/page.md"

if test -n "$output"; then
    mkdir -p "$(dirname -- "$output")"
    cp "$work/page.md" "$output"
    echo "written to $output" >&2
else
    cat "$work/page.md"
fi
