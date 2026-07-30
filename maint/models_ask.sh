#!/bin/sh
# Ask a model one question and print the answer.
#
# usage: models_ask.sh <system-prompt-file> <user-content-file>
#
# One place for the request, because providers disagree about the details and
# every script that asks has to survive the same disagreements. Anything that
# speaks the OpenAI chat shape will do: GitHub Models did, Cloudflare Workers AI
# does at /client/v4/accounts/<id>/ai/v1/chat/completions, OpenAI does at
# api.openai.com/v1/chat/completions.
#
# Two of those disagreements are handled by retrying rather than by asking
# whoever runs this to know about them in advance:
#
#   max_tokens          the newer OpenAI models want max_completion_tokens
#   temperature         some of them accept only their default
#
# The answer is transliterated to ASCII, since a package changelog is no place
# for a hyphen that is not a hyphen.
#
# Environment:
#   MODELS_TOKEN       token to authenticate with (default: GITHUB_TOKEN, GH_TOKEN)
#   MODELS_MODEL       model to ask (default: openai/gpt-4.1)
#   MODELS_ENDPOINT    where to ask it
#   MODELS_MAX_TOKENS  room for the answer (default: 4000). A reasoning model
#                      spends this on its thinking before it writes anything, so
#                      too little leaves the answer empty rather than short. It
#                      is a ceiling and not a reservation: what is not used is
#                      not paid for, which is why the default is generous.
set -eu

die() {
    echo "$*" >&2
    exit 1
}

system=${1:?usage: models_ask.sh <system-prompt-file> <user-content-file>}
user=${2:?usage: models_ask.sh <system-prompt-file> <user-content-file>}

test -f "$system" || die "no such file: $system"
test -f "$user" || die "no such file: $user"

model=${MODELS_MODEL:-openai/gpt-4.1}
endpoint=${MODELS_ENDPOINT:-https://models.github.ai/inference/chat/completions}
token=${MODELS_TOKEN:-${GITHUB_TOKEN:-${GH_TOKEN:-}}}
max_tokens=${MODELS_MAX_TOKENS:-4000}

test -n "$token" || die "no token: set MODELS_TOKEN, or GITHUB_TOKEN in a workflow"
command -v curl >/dev/null || die "curl is missing"
command -v jq >/dev/null || die "jq is missing"

work=$(mktemp -d) || die "cannot create a temporary directory"
trap 'rm -rf "$work"' EXIT HUP INT TERM

# post <token-key> <with-temperature> -- leaves the answer in $work/response.json
post() {
    jq -n --arg model "$model" --argjson max "$max_tokens" \
        --arg key "$1" --argjson temperature "$2" \
        --rawfile system "$system" --rawfile user "$user" \
        '{model: $model,
          messages: [{role: "system", content: $system},
                     {role: "user", content: $user}]}
         + {($key): $max}
         + (if $temperature == null then {} else {temperature: $temperature} end)' \
        > "$work/request.json"

    curl -sS --max-time 180 -X POST "$endpoint" \
        -H "Authorization: Bearer $token" \
        -H "Content-Type: application/json" \
        -H "Accept: application/json" \
        -d @"$work/request.json" > "$work/response.json"
}

complains_about() {
    jq -r '.error.message // ""' "$work/response.json" 2>/dev/null | grep -qi "$1"
}

key=max_tokens
temperature=0.2

post "$key" "$temperature" || die "the request to $endpoint failed"

if complains_about max_completion_tokens; then
    key=max_completion_tokens
    post "$key" "$temperature" || die "the request to $endpoint failed"
fi

if complains_about temperature; then
    temperature=null
    post "$key" "$temperature" || die "the request to $endpoint failed"
fi

content=$(jq -r '.choices[0].message.content // empty' "$work/response.json" 2>/dev/null |
    iconv -f UTF-8 -t ASCII//TRANSLIT 2>/dev/null || true)

if test -z "$content"; then
    echo "the model returned no text; the answer was:" >&2
    head -c 400 "$work/response.json" >&2
    echo >&2
    exit 1
fi

printf '%s\n' "$content"
