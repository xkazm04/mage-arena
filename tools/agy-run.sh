#!/usr/bin/env bash
# Dispatch one task card to the Antigravity CLI (Gemini) headless in this repo.
# Usage: bash tools/agy-run.sh tasks/<card>.md [model]   (default gemini-3.8-flash-high; hard tasks: gemini-3.1-pro-high)
# Permissions: ~/.gemini/antigravity-cli/settings.json allow-list + .agents/hooks.json deny guard (docs/ORCHESTRATION.md).
set -u
card="$1"; model="${2:-gemini-3.8-flash-high}"
agy="${AGY:-$LOCALAPPDATA/agy/bin/agy.exe}"
repo="$(cd "$(dirname "$0")/.." && pwd)"
id="$(basename "$card" .md | cut -d- -f1)"
out="$repo/runs/$id"; mkdir -p "$out"
start=$(date +%s)
prompt="You are the implementing worker for this repository. Read the task card at $card and carry it out completely. The card's constraints and the repository's CLAUDE.md and docs/DECISIONS.md are binding. Write your report to runs/$id/REPORT.md as the card specifies. Do not commit."
( cd "$repo" && "$agy" -p "$prompt" --model "$model" --output-format json > "$out/result.json" 2> "$out/stderr.log" )
code=$?
# print mode reports SUCCESS even when tools were denied: surface denials as a failure
denied=$(python -c "import json,sys;d=json.load(open(sys.argv[1],encoding='utf-8'));print(len(d.get('denied_actions') or []))" "$out/result.json" 2>/dev/null || echo "?")
echo "{\"card\":\"$card\",\"worker\":\"agy\",\"model\":\"$model\",\"exit\":$code,\"denied_actions\":\"$denied\",\"wall_s\":$(( $(date +%s) - start ))}" > "$out/run.json"
cat "$out/run.json"
