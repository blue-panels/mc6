#!/bin/sh
# Put a release page into the wiki, and name it in the index.
#
# usage: publish_wiki.sh <version> <page.md> [<summary.txt>]
#   maint/publish_wiki.sh v6.0.2 Release-v6.0.2.md summary.txt
#
# The wiki is a repository of its own, mcdev.wiki.git, so it is cloned, written
# to and pushed rather than committed alongside the source. The page goes in
# under its own name, and the line in Releases.md is added or replaced, taking
# its date from the page and its description from the release headline.
#
# Environment:
#   WIKI_TOKEN  token to authenticate with (default: GITHUB_TOKEN, GH_TOKEN)
#   WIKI_REPO   owner/name of the source repository (default: from git, then
#               GITHUB_REPOSITORY)
#   PUSH        no, to stop after committing locally -- for trying it out
set -eu

die() {
    echo "$*" >&2
    exit 1
}

version=${1:?usage: publish_wiki.sh <version> <page.md> [<summary.txt>]}
page=${2:?usage: publish_wiki.sh <version> <page.md> [<summary.txt>]}
summary=${3:-}

test -s "$page" || die "no page to publish: $page"

token=${WIKI_TOKEN:-${GITHUB_TOKEN:-${GH_TOKEN:-}}}
test -n "$token" || die "no token: set WIKI_TOKEN, or GITHUB_TOKEN in a workflow"

repo=${WIKI_REPO:-${GITHUB_REPOSITORY:-}}
if test -z "$repo"; then
    repo=$(git config --get remote.origin.url 2>/dev/null |
        sed -e 's|.*github\.com[:/]||' -e 's|\.git$||')
fi
test -n "$repo" || die "cannot tell which repository this is: set WIKI_REPO"

page=$(CDPATH= cd -- "$(dirname -- "$page")" && pwd)/$(basename -- "$page")
if test -n "$summary"; then
    test -s "$summary" || die "no summary to describe it with: $summary"
    summary=$(CDPATH= cd -- "$(dirname -- "$summary")" && pwd)/$(basename -- "$summary")
fi

wiki=$(mktemp -d) || die "cannot create a temporary directory"
trap 'rm -rf "$wiki"' EXIT HUP INT TERM

# The token belongs in the header rather than the URL, where it would be left
# behind in .git/config and printed by any later remote -v.
if ! git -c "http.extraheader=Authorization: Basic $(printf 'x-access-token:%s' "$token" | base64 | tr -d '\n')" \
        clone --quiet --depth 1 "https://github.com/$repo.wiki.git" "$wiki/clone" 2>"$wiki/clone.log"; then
    echo "the wiki could not be cloned:" >&2
    tail -3 "$wiki/clone.log" >&2
    die "if this is a permission error, the built-in token does not reach the wiki and a PAT is needed"
fi

cd "$wiki/clone"
cp "$page" "Release-$version.md"

# The index line: the date from the page heading, the description from the
# headline written for the tag.
date=$(sed -n '1s/.*(\([0-9-]*\)).*/\1/p' "Release-$version.md")
test -n "$date" || date=$(date +%Y-%m-%d)

description=""
test -z "$summary" || description=$(sed -e '/^[[:space:]]*$/d' -e 's/[[:space:]]*$//' "$summary" | head -n 1)
test -n "$description" || description="see the page"

index=Releases.md
if test -f "$index"; then
    line="- [$version](Release-$version) - $date - $description"
    awk -v version="$version" -v line="$line" '
        # A line for this version already there is replaced where it stands.
        index($0, "- [" version "](") == 1 { print line; written = 1; next }
        { print }
        # Otherwise it goes at the top of the list, which is newest first.
        /^## Release list/ && !written {
            getline blank
            print blank
            print line
            written = 1
        }
        END { if (!written) print line }
    ' "$index" > "$index.new"
    mv "$index.new" "$index"
fi

git add "Release-$version.md" ${index:+"$index"}
if git diff --cached --quiet; then
    echo "the wiki already says this; nothing to publish" >&2
    exit 0
fi

git config user.name "${GIT_AUTHOR_NAME:-github-actions[bot]}"
git config user.email "${GIT_AUTHOR_EMAIL:-41898282+github-actions[bot]@users.noreply.github.com}"
git commit --quiet -m "Release $version"

if test "${PUSH:-yes}" = no; then
    echo "committed in $wiki/clone, not pushed (PUSH=no)" >&2
    trap - EXIT HUP INT TERM
    exit 0
fi

if ! git -c "http.extraheader=Authorization: Basic $(printf 'x-access-token:%s' "$token" | base64 | tr -d '\n')" \
        push --quiet origin HEAD 2>"$wiki/push.log"; then
    echo "the wiki would not take the push:" >&2
    tail -3 "$wiki/push.log" >&2
    die "if this is a permission error, the built-in token does not reach the wiki and a PAT is needed"
fi

echo "published Release-$version to the $repo wiki" >&2
