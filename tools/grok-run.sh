#!/usr/bin/env bash
# Dispatch one task card to Grok headless in this repo. Usage: bash tools/grok-run.sh tasks/<card>.md [model]
set -u
card="$1"; model="${2:-grok-4.7}"
repo="$(cd "$(dirname "$0")/.." && pwd)"
id="$(basename "$card" .md | cut -d- -f1)"
turns="$(grep -m1 -i '^Max turns:' "$card" | grep -o '[0-9]\+' || echo 120)"
out="$repo/runs/$id"; mkdir -p "$out"
start=$(date +%s)
prompt="You are the implementing worker for this repository. Read the task card at $card and carry it out completely. The card's constraints and the repository's CLAUDE.md and docs/DECISIONS.md are binding. Write your report to runs/$id/REPORT.md as the card specifies. Do not commit."
( cd "$repo" && grok -p "$prompt" -m "$model" --always-approve --max-turns "$turns" --output-format json \
    > "$out/result.json" 2> "$out/stderr.log" )
code=$?
echo "{\"card\":\"$card\",\"model\":\"$model\",\"exit\":$code,\"wall_s\":$(( $(date +%s) - start ))}" > "$out/run.json"
cat "$out/run.json"
