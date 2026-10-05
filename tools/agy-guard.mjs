// PreToolUse guard for Antigravity CLI workers (see docs/ORCHESTRATION.md).
// Headless agy cannot prompt, so every tool call is decided here: allow by default,
// hard-deny the operations a worker must never perform.
import { readFileSync } from 'node:fs';

let call = {};
try { call = JSON.parse(readFileSync(0, 'utf8') || '{}').toolCall ?? {}; } catch { /* malformed input: decide on nothing */ }
const cmd = String(call.args?.CommandLine ?? call.args?.command ?? '');
// Global git options may sit between `git` and the subcommand: -C <dir>, -c k=v, --no-pager, --git-dir <p>.
const gitOpts = String.raw`(?:\s+(?:-[Cc]\s+(?:"[^"]*"|\S+)|--[\w-]+(?:[=\s](?:"[^"]*"|\S+))?))*`;
const gitMutation = new RegExp(String.raw`\bgit${gitOpts}\s+(commit|push|reset|checkout|switch|rebase|merge|stash|clean|branch\s+-D|tag)\b`, 'i');
// Workers run on Windows, so the cmd.exe and PowerShell spellings of a recursive delete are covered too,
// including PowerShell's abbreviated -r / -rec / -recurse and the ri alias.
const recursiveDelete = new RegExp([
  String.raw`\brm\b[^\n;|&]*\s(?:-[a-z]*r[a-z]*|--recursive)\b`,
  String.raw`\b(?:Remove-Item|ri)\b.*\s-r(?:e(?:c(?:u(?:r(?:s(?:e)?)?)?)?)?)?\b`,
  String.raw`\b(?:rmdir|rd|del|erase)\b.*\s/s\b`,
].join('|'), 'i');
const rules = [
  [gitMutation, 'workers never change git history or the worktree state; the orchestrator commits'],
  [/--no-verify|--force\b|\s-f\s.*push/i, 'no hook bypass or force operations'],
  [recursiveDelete, 'no recursive deletes; ask the orchestrator'],
  [/ELEVENLABS_API_KEY|\.env\b/i, 'secrets are read only by tools the card names, never echoed'],
];
// Review mode: `--mode plan` does NOT stop agy from editing files or building (observed 2026-10-03, T10 review), so a
// read-only review is enforced here. With AGY_READONLY=1 only reading, listing and searching are allowed.
if (process.env.AGY_READONLY === '1') {
  const name = String(call.name ?? '');
  const readTool = /^(view|read|list|grep|search|find|glob|codebase|get)/i.test(name) || /(_read|_view|_list|_search)$/i.test(name);
  const readCmd = /^\s*(git\s+(diff|show|log|status|ls-files|grep)\b|rg\b|grep\b|cat\b|type\b|head\b|tail\b|wc\b|ls\b|dir\b|findstr\b|Get-Content\b|Select-String\b|node\s+-e\s+"[^"]*readFileSync)/i.test(cmd)
    && !/[>|]\s*(?!\s*(head|tail|wc|grep|findstr|Select-String))/i.test(cmd.replace(/2>&1|2>\$null|2>\/dev\/null/g, ''));
  const allowed = cmd ? readCmd : readTool;
  if (!allowed) {
    process.stdout.write(JSON.stringify({ decision: 'deny', reason: `read-only review: ${cmd ? 'command' : 'tool ' + name} is not a read` }));
    process.exit(0);
  }
}
const hit = rules.find(([re]) => re.test(cmd));
process.stdout.write(JSON.stringify(hit
  ? { decision: 'deny', reason: hit[1] }
  : { decision: 'allow', permissionOverrides: cmd ? [`command(${cmd})`] : [] }));
