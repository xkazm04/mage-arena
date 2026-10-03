#!/usr/bin/env bash
# Read-only code review by Gemini through the Antigravity CLI.
# Usage: bash tools/agy-review.sh "<review prompt>" [model]   -> prints status, denied actions and the review text.
# `--mode plan` alone does NOT stop agy from editing or building (2026-10-03), so AGY_READONLY=1 makes the
# PreToolUse guard (tools/agy-guard.mjs) deny every tool that is not a read.
set -u
prompt="$1"; model="${2:-gemini-3.8-flash-high}"
agy="${AGY:-$LOCALAPPDATA/agy/bin/agy.exe}"
repo="$(cd "$(dirname "$0")/.." && pwd)"
out="$(mktemp)"
( cd "$repo" && AGY_READONLY=1 "$agy" -p "Read-only code review using ONLY files in this repository; do not browse. $prompt Do not modify files." --model "$model" --mode plan --output-format json > "$out" 2>/dev/null )
PYTHONIOENCODING=utf-8 python -c "import json,sys;d=json.load(open(sys.argv[1],encoding='utf-8'));print(d.get('status'),'denied',d.get('denied_actions'));print(d.get('response',''))" "$out"
rm -f "$out"
