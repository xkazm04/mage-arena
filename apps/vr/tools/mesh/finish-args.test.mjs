// Offline proof of the finish.py argument list finish.mjs builds (finish-args.mjs): no Blender, no files.
//
//   node --test apps/vr/tools/mesh/finish-args.test.mjs
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { finishParams, finishPyArgs, geometryDrift } from './finish-args.mjs';
import { classBudget } from './gate-lib.mjs';

const budgets = JSON.parse(readFileSync(new URL('./budgets.json', import.meta.url), 'utf8'));
const human = classBudget(budgets, 'Humanoid');
const optOf = (flags) => (name, dflt) => (name in flags ? flags[name] : dflt);
const args = (flags, cls = human) => finishPyArgs('raw.glb', 'out', 'Brennic', finishParams(optOf(flags), budgets, cls));

test('--mode flat passes exactly the pre-bake argument list', () => {
  // the list finish.mjs built before the bake mode: the HANDOFF rebuild line "Brennic 0 4096 0.01 24000"
  assert.deepEqual(args({ mode: 'flat' }), ['raw.glb', 'out', 'Brennic', '0', '4096', '0.01', '24000', 'Brennic']);
});

test('bake is the default (DECISIONS 2026-10-24, Athena 17:56Z), bakes at bakeTextureRes and says so', () => {
  assert.equal(budgets.finish.bakeTextureRes, 2048);
  assert.deepEqual(args({}), ['raw.glb', 'out', 'Brennic', '0', '2048', '0.01', '24000', 'Brennic', 'mode=bake']);
  assert.deepEqual(args({ mode: 'bake' }), args({}));
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
  assert.equal(args({ mode: 'flat' }, classBudget(budgets, 'CinderHound'))[7], 'CinderHound');
  assert.equal(args({}, classBudget(budgets, 'CinderHound'))[7], 'CinderHound');
  assert.equal(args({ palette: 'Conscript', mode: 'bake' })[7], 'Conscript');
});

test('bad mode, bad clamp, and bake flags without the bake mode are refused', () => {
  assert.throws(() => args({ mode: 'paint' }), /--mode paint/);
  assert.throws(() => args({ mode: 'bake', clamp: 'flat' }), /--clamp flat/);
  assert.throws(() => args({ mode: 'flat', clamp: 'hue' }), /need --mode bake/);
  assert.throws(() => args({ mode: 'flat', cage: '0.01' }), /need --mode bake/);
  assert.deepEqual(args({ clamp: 'hue' }).slice(8), ['mode=bake', 'clamp=hue']); // the default takes the bake flags
});

test('--normal on passes normal=on; off (the default) leaves the flat and bake lists unchanged', () => {
  assert.deepEqual(args({ mode: 'bake', normal: 'on' }).slice(8), ['mode=bake', 'normal=on']);
  assert.deepEqual(args({ normal: 'on', cage: '0.02' }).slice(8), ['mode=bake', 'cage=0.02', 'normal=on']);
  assert.deepEqual(args({ normal: 'off' }), args({}));
  assert.deepEqual(args({ mode: 'flat', normal: 'off' }), args({ mode: 'flat' }));
  assert.throws(() => args({ mode: 'flat', normal: 'on' }), /need --mode bake/);
  assert.throws(() => args({ normal: 'yes' }), /--normal yes/);
});

test('geometryDrift: the card numbers and the section count must match the reference run exactly', () => {
  const card = { metrics: { tris: 23990, verts: 11987, edges: 35985, components: 1, watertight: true, heightM: 1.79923, minZ: 0.00027, bboxMin: [-0.51389, -0.19248, 0.00027], bboxMax: [0.51281, 0.19221, 1.7995], areaM2: 4.64266, file: { bytes: 3563700 } } };
  const finish = { materials: { M_Brennic: {} }, tris_final: 23990, verts_final: 11987 };
  const withNormal = { metrics: { ...card.metrics, file: { bytes: 9999999 } } }; // a bigger GLB is not drift
  assert.deepEqual(geometryDrift(card, finish, withNormal, { ...finish, normal: {} }), []);
  assert.deepEqual(geometryDrift(card, finish, { metrics: { ...card.metrics, tris: 23988, heightM: 1.79924 } }, finish), [
    { key: 'tris', expected: 23990, actual: 23988 },
    { key: 'heightM', expected: 1.79923, actual: 1.79924 },
  ]);
  assert.deepEqual(geometryDrift(card, finish, card, { ...finish, materials: { A: {}, B: {} }, verts_final: 11990 }), [
    { key: 'sections', expected: 1, actual: 2 },
    { key: 'verts_final', expected: 11987, actual: 11990 },
  ]);
});
