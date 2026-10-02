// PreToolUse guard for Antigravity CLI workers (see docs/ORCHESTRATION.md).
// Headless agy cannot prompt, so every tool call is decided here: allow by default,
// hard-deny the operations a worker must never perform.
import { readFileSync } from 'node:fs';

let call = {};
try { call = JSON.parse(readFileSync(0, 'utf8') || '{}').toolCall ?? {}; } catch { /* malformed input: decide on nothing */ }
const cmd = String(call.args?.CommandLine ?? call.args?.command ?? '');
const rules = [
  [/\bgit\s+(commit|push|reset|checkout|switch|rebase|merge|stash|clean|branch\s+-D|tag)\b/i, 'workers never change git history or the worktree state; the orchestrator commits'],
  [/--no-verify|--force\b|\s-f\s.*push/i, 'no hook bypass or force operations'],
  [/\brm\s+-[a-z]*r[a-z]*f|Remove-Item\b.*-Recurse/i, 'no recursive deletes; ask the orchestrator'],
  [/ELEVENLABS_API_KEY|\.env\b/i, 'secrets are read only by tools the card names, never echoed'],
];
const hit = rules.find(([re]) => re.test(cmd));
process.stdout.write(JSON.stringify(hit
  ? { decision: 'deny', reason: hit[1] }
  : { decision: 'allow', permissionOverrides: cmd ? [`command(${cmd})`] : [] }));
