// Gate for tools/agy-guard.mjs. Run: node --test tools/agy-guard.test.mjs
// Each case feeds the guard one shell command on stdin, as the agy PreToolUse hook does,
// and checks the decision. The allow cases keep the deny rules from swallowing read-only work.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const guard = fileURLToPath(new URL('./agy-guard.mjs', import.meta.url));

function decide(command, env = {}) {
  const input = JSON.stringify({ toolCall: { name: 'run_command', args: { CommandLine: command } } });
  const run = spawnSync(process.execPath, [guard], { input, encoding: 'utf8', env: { ...process.env, AGY_READONLY: '', ...env } });
  assert.equal(run.status, 0, `guard exited ${run.status}: ${run.stderr}`);
  return JSON.parse(run.stdout).decision;
}

const deny = [
  'git push origin master',
  'git commit -m x',
  'git -C . commit -m x',
  'git -c core.x=1 push origin master',
  'git --no-pager commit -m x',
  'git -C "some dir" reset --hard',
  'rm -rf runs',
  'rm -fr runs',
  'rm -r runs',
  'rm --recursive runs',
  'Remove-Item runs -Recurse -Force',
  'Remove-Item runs -r -fo',
  'Remove-Item runs -rec',
  'ri runs -Recurse',
  'rmdir /s /q runs',
  'rd /s /q runs',
  'del /s /q *.cs',
  'cmd /c "rmdir /s /q runs"',
  'type .env',
];

const allow = [
  'ls apps',
  'git status',
  'git -C . status',
  'git diff --stat',
  'git log -1',
  'rm notes.txt',
  'Remove-Item notes.txt',
  'rmdir emptydir',
  'del notes.txt',
  'node tools/check.mjs',
];

for (const command of deny) {
  test(`denies: ${command}`, () => assert.equal(decide(command), 'deny'));
}
for (const command of allow) {
  test(`allows: ${command}`, () => assert.equal(decide(command), 'allow'));
}

test('read-only mode still denies a write command', () => {
  assert.equal(decide('Remove-Item runs -r', { AGY_READONLY: '1' }), 'deny');
});
