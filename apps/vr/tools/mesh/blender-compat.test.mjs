// Offline proof of blender-compat.mjs: the EEVEE engine id is adapted, nothing else moves. No Blender.
//
//   node --test apps/vr/tools/mesh/blender-compat.test.mjs
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { adaptEevee, EEVEE_PRELUDE, evidenceFiles } from './blender-compat.mjs';
import { HERE } from './blender.mjs';

const RENDER_EVIDENCE = readFileSync(join(HERE, '..', 'assets', 'render_evidence.py'), 'utf8');

test('adaptEevee: render_evidence.py gets its one engine id replaced, line numbers kept', () => {
  const { source, replaced } = adaptEevee(RENDER_EVIDENCE);
  assert.equal(replaced, 1);
  assert.equal(source.split('\n').length, RENDER_EVIDENCE.split('\n').length);
  const line21 = source.split('\n')[20];
  assert.match(line21, /s\.render\.engine = MAGEVR_EEVEE;/);
  assert.ok(source.startsWith(EEVEE_PRELUDE), 'the prelude sits on line 1, before the comment');
  assert.doesNotMatch(source.slice(EEVEE_PRELUDE.length), /'BLENDER_EEVEE'/);
});

test('adaptEevee: BLENDER_EEVEE_NEXT and other engines stay; both quote styles are adapted', () => {
  const src = '# x\na = "BLENDER_EEVEE"\nb = \'BLENDER_EEVEE_NEXT\'\nc = \'CYCLES\'\n';
  const { source, replaced } = adaptEevee(src);
  assert.equal(replaced, 1);
  assert.match(source, /a = MAGEVR_EEVEE\n/);
  assert.match(source, /b = 'BLENDER_EEVEE_NEXT'/);
  assert.match(source, /c = 'CYCLES'/);
});

test('adaptEevee: a script without the id is returned unchanged', () => {
  const src = "# x\ns.render.engine = 'CYCLES'\n";
  assert.deepEqual(adaptEevee(src), { source: src, replaced: 0 });
});

test('adaptEevee: a first line that is code gets the prelude as its own line', () => {
  const src = "import bpy\nbpy.context.scene.render.engine = 'BLENDER_EEVEE'\n";
  const { source } = adaptEevee(src);
  assert.equal(source, `${EEVEE_PRELUDE.trimEnd()}\nimport bpy\nbpy.context.scene.render.engine = MAGEVR_EEVEE\n`);
});

test('evidenceFiles: the turnaround, the game view and one strip per clip, attack clip second', () => {
  assert.deepEqual(evidenceFiles('Brennic'), [
    'Brennic_turnaround.png',
    'Brennic_game_640x480.png',
    'Brennic_clip_Idle.png',
    'Brennic_clip_Cast.png',
    'Brennic_clip_Hit.png',
    'Brennic_clip_Death.png',
  ]);
  assert.ok(evidenceFiles('Conscript', 'Attack').includes('Conscript_clip_Attack.png'));
});
