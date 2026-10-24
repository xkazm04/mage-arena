// Offline proof of the finish.py argument list finish.mjs builds (finish-args.mjs): no Blender, no files.
//
//   node --test apps/vr/tools/mesh/finish-args.test.mjs
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { finishParams, finishPyArgs } from './finish-args.mjs';
import { classBudget } from './gate-lib.mjs';

const budgets = JSON.parse(readFileSync(new URL('./budgets.json', import.meta.url), 'utf8'));
const human = classBudget(budgets, 'Humanoid');
const optOf = (flags) => (name, dflt) => (name in flags ? flags[name] : dflt);
const args = (flags, cls = human) => finishPyArgs('raw.glb', 'out', 'Brennic', finishParams(optOf(flags), budgets, cls));

test('flat is the default and passes exactly the pre-bake argument list', () => {
  // the list finish.mjs built before the bake mode: the HANDOFF rebuild line "Brennic 0 4096 0.01 24000"
  assert.deepEqual(args({}), ['raw.glb', 'out', 'Brennic', '0', '4096', '0.01', '24000', 'Brennic']);
  assert.deepEqual(args({ mode: 'flat' }), args({}));
});

test('bake mode bakes at bakeTextureRes and says so', () => {
  assert.equal(budgets.finish.bakeTextureRes, 2048);
  assert.deepEqual(args({ mode: 'bake' }), ['raw.glb', 'out', 'Brennic', '0', '2048', '0.01', '24000', 'Brennic', 'mode=bake']);
  assert.equal(args({ mode: 'bake', bake: '1024' })[4], '1024');
});

test('bake options pass through as key=value; clamp none is left out', () => {
  assert.deepEqual(args({ mode: 'bake', clamp: 'both', strength: '0.4', levels: '6', cage: '0.015' }).slice(8), [
    'mode=bake',
    'clamp=both',
    'strength=0.4',
    'levels=6',
    'cage=0.015',
  ]);
  assert.deepEqual(args({ mode: 'bake', clamp: 'none' }).slice(8), ['mode=bake']);
});

test('a class palette and --palette still reach finish.py', () => {
  assert.equal(args({}, classBudget(budgets, 'CinderHound'))[7], 'CinderHound');
  assert.equal(args({ palette: 'Conscript', mode: 'bake' })[7], 'Conscript');
});

test('bad mode, bad clamp, and bake flags without the bake mode are refused', () => {
  assert.throws(() => args({ mode: 'paint' }), /--mode paint/);
  assert.throws(() => args({ mode: 'bake', clamp: 'flat' }), /--clamp flat/);
  assert.throws(() => args({ clamp: 'hue' }), /need --mode bake/);
  assert.throws(() => args({ cage: '0.01' }), /need --mode bake/);
});
