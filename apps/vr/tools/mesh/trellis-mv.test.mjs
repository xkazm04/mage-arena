// Offline proof of the two-view TRELLIS.2 workflow and the comfy.mjs dry run: no ComfyUI, no GPU, no upload.
//
//   node --test apps/vr/tools/mesh/trellis-mv.test.mjs
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { BASE_WORKFLOW, MV_CONDITIONING, buildWorkflow, dryRun, parseRunArgs, uploadName } from './trellis-mv.mjs';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const base = JSON.parse(fs.readFileSync(BASE_WORKFLOW, 'utf8'));
const names = { front: 'f.jpg', back: 'b.jpg' };
const refs = (p) => Object.entries(p).flatMap(([id, n]) => Object.values(n.inputs).filter((v) => Array.isArray(v) && typeof v[0] === 'string').map((v) => [id, v[0]]));

test('parseRunArgs: positionals plus --back, --faces and --dry-run in any order', () => {
  const a = parseRunArgs(['a.jpg', 'maw', '7', 'out', '--back', 'b.jpg', '--dry-run', '--faces', '1000']);
  assert.deepEqual(a, { back: 'b.jpg', faces: 1000, dryRun: true, front: 'a.jpg', label: 'maw', seed: 7, outdir: 'out' });
  assert.equal(parseRunArgs(['a.jpg', 'maw', '7', 'out']).back, null);
  assert.throws(() => parseRunArgs(['a.jpg', 'maw', 'x', 'out']), /usage/);
  assert.throws(() => parseRunArgs(['a.jpg', 'maw', '7', 'out', '--back']), /needs a value/);
  assert.throws(() => parseRunArgs(['a.jpg', 'maw', '7', 'out', '--bogus']), /unknown argument/);
});

test('buildWorkflow: one view sets only the image, faces, seeds and prefix, and leaves the base alone', () => {
  const before = JSON.stringify(base);
  const p = buildWorkflow(base, { names: { front: 'f.jpg' }, seed: 9, faces: 5000, label: 'x' });
  assert.equal(JSON.stringify(base), before);
  assert.equal(p['122'].inputs.image, 'f.jpg');
  assert.equal(p['186'].inputs.target_face_count, 5000);
  for (const id of ['3', '12', '18', '23']) assert.equal(p[id].inputs.seed, 9);
  assert.equal(p['322'].inputs.filename_prefix, 't38/x');
  assert.equal(p['299'].class_type, 'Trellis2Conditioning');
  assert.equal(p['422'], undefined);
});

test('buildWorkflow: two views load both images and feed one multi-view conditioning node', () => {
  const p = buildWorkflow(base, { names, seed: 7, faces: 700000, label: 'maw' });
  assert.equal(p['122'].inputs.image, 'f.jpg');
  assert.equal(p['422'].class_type, 'LoadImage');
  assert.equal(p['422'].inputs.image, 'b.jpg');
  const mv = p[MV_CONDITIONING];
  assert.equal(mv.class_type, 'Pixal3DMultiViewConditioning');
  assert.deepEqual(mv.inputs, { fov: 20, clip_vision_model: ['15', 0], front: ['312', 0], back: ['412', 0] });
  assert.equal(p['299'], undefined, 'the front-only conditioning is gone');
  for (const [from, to] of [['3', 'positive'], ['3', 'negative'], ['91', 'positive'], ['91', 'negative']]) {
    assert.deepEqual(p[from].inputs[to], [MV_CONDITIONING, 0]);
  }
  // the back chain is the front chain, cloned and wired to the back image
  assert.deepEqual(p['492'].inputs.image, ['422', 0]);
  assert.deepEqual(p['448'].inputs.on_false, ['422', 1]);
  assert.deepEqual(p['448'].inputs.on_true, ['492', 0]);
  assert.equal(p['412'].class_type, 'ImageCropToMask');
  assert.deepEqual(p['412'].inputs.images, ['422', 0]);
  assert.equal(p['412'].inputs.pad_factor, 1.1);
  // the front chain is untouched
  for (const id of ['122', '192', '248', '303', '312']) assert.equal(p[id].class_type, base[id].class_type);
  assert.deepEqual(p['312'].inputs.images, ['122', 0]);
});

test('buildWorkflow: every wire of the two-view graph points at a node that exists', () => {
  const p = buildWorkflow(base, { names, seed: 7, faces: 700000, label: 'maw' });
  for (const [id, to] of refs(p)) assert.ok(p[to], `node ${id} reads missing node ${to}`);
});

test('dryRun: names the uploads per view and refuses a missing image', () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'mv-'));
  const f = path.join(dir, 'f.jpg');
  const b = path.join(dir, 'b.png');
  fs.writeFileSync(f, 'x');
  fs.writeFileSync(b, 'x');
  const p = dryRun({ front: f, back: b, label: 'maw', seed: 7, faces: 700000 });
  assert.equal(p['122'].inputs.image, uploadName('maw', 'front', f));
  assert.equal(p['422'].inputs.image, 't38_maw_back.png');
  assert.throws(() => dryRun({ front: f, back: path.join(dir, 'none.jpg'), label: 'm', seed: 1, faces: 1 }), /no such image/);
});

test('comfy.mjs run --back --dry-run prints a two-image workflow and needs nothing running', () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'mv-cli-'));
  const f = path.join(dir, 'f.jpg');
  const b = path.join(dir, 'b.jpg');
  fs.writeFileSync(f, 'x');
  fs.writeFileSync(b, 'x');
  const r = spawnSync(process.execPath, [path.join(HERE, 'comfy.mjs'), 'run', f, 'maw', '7', path.join(dir, 'out'), '--back', b, '--dry-run'], { encoding: 'utf8' });
  assert.equal(r.status, 0, r.stderr);
  const p = JSON.parse(r.stdout);
  const loads = Object.values(p).filter((n) => n.class_type === 'LoadImage').map((n) => n.inputs.image).sort();
  assert.deepEqual(loads, ['t38_maw_back.jpg', 't38_maw_front.jpg']);
  assert.equal(p[MV_CONDITIONING].class_type, 'Pixal3DMultiViewConditioning');
  assert.equal(fs.existsSync(path.join(dir, 'out')), false, 'a dry run writes nothing');
});
