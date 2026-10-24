// Offline proof of the mesh lane's scoring rules (gate-lib.mjs): no Blender, no files.
//
//   node --test apps/vr/tools/mesh/gate-lib.test.mjs
import test from 'node:test';
import assert from 'node:assert/strict';
import {
  exitCodeFor,
  gateInput,
  IMAGE_UNMEASURED,
  inputVerdict,
  MESH_REMEDIES,
  scoreImage,
  scoreMesh,
  splitComponents,
} from './gate-lib.mjs';

// image_stats.py output for a clean single subject on a flat mid-grey field
const image = (over = {}) => ({
  image: { width: 768, height: 1376, channels: 4 },
  background: { mode: 'opaque', srgb: [0.8, 0.8, 0.8], luminance: 0.8, borderNearShare: 1, sideSpread: 0, ...over.background },
  foreground: { share: 0.37, components: [{ area: 40000, share: 1, bbox: [0.05, 0.05, 0.95, 0.94] }], componentsOmitted: 0, ...over.foreground },
});
const codes = (r) => r.findings.map((f) => f.code);

test('inputVerdict: fail > prepare > incomplete > warn > pass', () => {
  const f = (severity) => ({ severity });
  assert.equal(inputVerdict([f('warn'), f('fail'), f('prepare')], ['x']), 'fail');
  assert.equal(inputVerdict([f('warn'), f('prepare')], ['x']), 'prepare');
  assert.equal(inputVerdict([f('warn')], ['x']), 'incomplete');
  assert.equal(inputVerdict([f('warn')], []), 'warn');
  assert.equal(inputVerdict([], []), 'pass');
});

test('input gate: a clean image is never a pass while a criterion is unmeasured', () => {
  const r = gateInput({ front: image() });
  assert.deepEqual(r.findings, []);
  assert.equal(r.verdict, 'incomplete');
  assert.deepEqual(r.unmeasured, Object.keys(IMAGE_UNMEASURED));
  assert.match(r.notice, /not a quality claim/);
  assert.match(r.ungated, /ungated/);
  assert.equal(exitCodeFor(r.verdict), 3);
});

test('input gate: two subjects of comparable size fail single-subject; a speck does not', () => {
  const two = image({ foreground: { components: [{ area: 18000, bbox: [0.05, 0.05, 0.39, 0.95] }, { area: 10000, bbox: [0.41, 0.05, 0.56, 0.95] }] } });
  assert.deepEqual(codes(scoreImage(two)), ['multiple-subjects']);
  const speck = image({ foreground: { components: [{ area: 40000, bbox: [0.05, 0.05, 0.95, 0.94] }, { area: 100, bbox: [0.5, 0.96, 0.51, 0.97] }] } });
  assert.deepEqual(codes(scoreImage(speck)), []);
});

test('input gate: no foreground is no-subject, and framing is then not applicable', () => {
  const r = scoreImage(image({ foreground: { share: 0, components: [] } }));
  assert.deepEqual(codes(r), ['no-subject']);
  assert.equal(r.criteria.find((c) => c.id === 'subject-in-frame').state, 'not-applicable');
});

test('input gate: touching the frame is a hard fail, a tight margin a warning', () => {
  const touch = gateInput({ front: image({ foreground: { components: [{ area: 40000, bbox: [0.05, 0, 0.95, 0.94] }] } }) });
  assert.equal(touch.verdict, 'fail');
  assert.deepEqual(codes(touch), ['subject-touches-frame']);
  const tight = scoreImage(image({ foreground: { components: [{ area: 40000, bbox: [0.02, 0.05, 0.95, 0.94] }] } }));
  assert.deepEqual(codes(tight), ['margin-tight']);
  assert.equal(tight.findings[0].severity, 'warn');
});

test('input gate: a busy or graded background is prepare (fix and re-gate), not fail', () => {
  const busy = gateInput({ front: image({ background: { borderNearShare: 0.8 } }) });
  assert.equal(busy.verdict, 'prepare');
  assert.deepEqual(codes(busy), ['background-not-plain']);
  const graded = gateInput({ front: image({ background: { sideSpread: 0.1 } }) });
  assert.deepEqual(codes(graded), ['background-not-plain']);
  assert.equal(exitCodeFor(graded.verdict), 1);
});

test('input gate: a dark field and a small subject are warnings', () => {
  assert.deepEqual(codes(scoreImage(image({ background: { luminance: 0.05 } }))), ['background-extreme-value']);
  const small = image({ foreground: { components: [{ area: 400, bbox: [0.4, 0.4, 0.6, 0.6] }] } });
  assert.deepEqual(codes(scoreImage(small)), ['subject-low-resolution']);
});

test('input gate: a transparent background is not graded on its border colour', () => {
  const r = scoreImage(image({ background: { mode: 'transparent', borderNearShare: 0, sideSpread: 1, luminance: 0 } }));
  assert.deepEqual(codes(r), []);
});

test('view set: a missing required view fails completeness; one image needs only the master', () => {
  const r = gateInput({ front: image(), back: image() }, ['front', 'side', 'back']);
  assert.equal(r.verdict, 'fail');
  assert.deepEqual(codes(r), ['view-missing']);
  assert.equal(r.findings[0].view, 'side');
  const one = gateInput({ front: image() });
  assert.equal(one.criteria.find((c) => c.id === 'view-set-consistency').state, 'not-applicable');
  assert.deepEqual(one.criteria.find((c) => c.id === 'view-set-completeness').values.missing, []);
});

test('view set: a view framed unlike the master fails the set and falls back to the master', () => {
  const far = image({ foreground: { components: [{ area: 20000, bbox: [0.3, 0.3, 0.7, 0.94] }] } });
  const r = gateInput({ front: image(), side: image(), back: far }, ['front', 'side', 'back']);
  assert.equal(r.verdict, 'fail');
  assert.deepEqual(codes(r), ['view-framing-mismatch']);
  assert.equal(r.findings[0].view, 'back');
  assert.match(r.fallback, /master/);
  assert.ok(r.unmeasured.includes('view-feature-correspondence'));
});

test('view set: consistent views carry only the unmeasured criteria', () => {
  const r = gateInput({ front: image(), side: image(), back: image() }, ['front', 'side', 'back']);
  assert.deepEqual(r.findings, []);
  assert.equal(r.verdict, 'incomplete');
  assert.equal(r.fallback, undefined);
});

// mesh_metrics.py output
const mesh = (over = {}) => ({
  tris: 24000, verts: 12000, watertight: true, boundaryEdges: 0, nonManifoldEdges: 0, windingInconsistentEdges: 0,
  degenerateFaces: 0, components: 1, componentFaces: [24000], componentFacesOmitted: 0,
  extentM: [0.5, 0.8, 1.8], heightM: 1.8, minZ: 0, centreXY: [0, 0], ...over,
});
const humanoid = { heightM: 1.8, heightTolerance: 0.1, tris: { target: 24000, min: 18000, max: 30000 }, partsMax: 8, specksMax: 4 };

test('splitComponents: share floor with an absolute minimum', () => {
  const s = splitComponents(mesh({ tris: 1000, componentFaces: [900, 60, 30, 7, 3], components: 5 }));
  assert.equal(s.floorFaces, 8); // 0.5 % of 1000 is 5, under the 8-face minimum
  assert.deepEqual([s.parts, s.specks, s.speckFaces], [3, 2, 10]);
});

test('splitComponents: a truncated histogram resolves toward the harsher verdict', () => {
  // smallest kept entry is a speck: every omitted component is a speck, and their faces are the remainder
  const a = splitComponents(mesh({ tris: 10000, componentFaces: [9000, 900, 40], componentFacesOmitted: 6 }));
  assert.deepEqual([a.parts, a.specks, a.speckFaces, a.truncation], [2, 7, 100, 'omitted counted as specks']);
  // smallest kept entry is a part: the omitted ones count as parts
  const b = splitComponents(mesh({ tris: 10000, componentFaces: [5000, 4000], componentFacesOmitted: 9 }));
  assert.deepEqual([b.parts, b.specks, b.truncation], [11, 0, 'omitted counted as parts']);
});

test('splitComponents: no histogram is unmeasured, not zero', () => {
  assert.deepEqual(splitComponents(mesh({ componentFaces: undefined })), { measured: false });
  const card = scoreMesh(mesh({ componentFaces: undefined, components: 12 }), { stage: 'finished', cls: humanoid });
  assert.ok(card.unmeasured.includes('component-split'));
  assert.deepEqual(codes(card), ['too-many-parts']); // the blunt count rule, unchanged
});

test('mesh card: a finished mesh inside its class passes, structurally', () => {
  const card = scoreMesh(mesh(), { stage: 'finished', cls: humanoid, className: 'Humanoid' });
  assert.equal(card.verdict, 'pass');
  assert.equal(card.score, 100);
  assert.equal(card.grades.budget.within, true);
  assert.equal(card.grades.scale.within, true);
  assert.match(card.notice, /not a quality claim/);
});

test('mesh card: raw output grades what finishing resolves as warn, finished grades it fail', () => {
  const raw = mesh({ tris: 695551, componentFaces: [694000, ...Array(150).fill(10)], components: 151, watertight: false, boundaryEdges: 40, heightM: 1, minZ: -0.5 });
  const r = scoreMesh(raw, { stage: 'raw', cls: humanoid });
  assert.equal(r.verdict, 'warn');
  assert.deepEqual(codes(r).sort(), ['floaters', 'not-watertight', 'origin-off', 'scale-off', 'tris-over-budget']);
  assert.equal(r.route, 'finish');
  const f = scoreMesh(raw, { stage: 'finished', cls: humanoid });
  assert.equal(f.verdict, 'fail');
  assert.equal(f.findings[0].severity, 'fail');
  assert.equal(f.score, 0);
});

test('mesh card: an undeclared stage is graded as finished and says so', () => {
  const card = scoreMesh(mesh({ tris: 40000 }), { cls: humanoid });
  assert.equal(card.stage, 'undeclared');
  assert.match(card.stageNote, /harsher/);
  assert.equal(card.findings.find((x) => x.code === 'tris-over-budget').severity, 'fail');
});

test('mesh card: without a class, scale and budget are unmeasured and the card cannot pass', () => {
  const card = scoreMesh(mesh(), { stage: 'finished' });
  assert.equal(card.verdict, 'incomplete');
  assert.deepEqual(card.unmeasured, ['part-budget', 'triangle-budget', 'world-scale']);
  assert.equal(card.grades.budget, undefined);
  assert.equal(exitCodeFor(card.verdict), 3);
});

test('mesh card: floaters within tolerance warn, above it fail at the finished stage', () => {
  const few = scoreMesh(mesh({ componentFaces: [23990, 5, 5], components: 3 }), { stage: 'finished', cls: humanoid });
  assert.equal(few.findings.find((x) => x.code === 'floaters').severity, 'warn');
  const many = scoreMesh(mesh({ componentFaces: [23950, ...Array(10).fill(5)], components: 11 }), { stage: 'finished', cls: humanoid });
  assert.equal(many.findings.find((x) => x.code === 'floaters').severity, 'fail');
});

test('mesh card: too many substantial parts and an empty mesh have their own codes and routes', () => {
  const parts = scoreMesh(mesh({ componentFaces: Array(12).fill(2000), components: 12 }), { stage: 'raw', cls: humanoid });
  assert.deepEqual(codes(parts), ['too-many-parts']);
  assert.equal(parts.route, 'person');
  const empty = scoreMesh(mesh({ tris: 0, verts: 0 }), { stage: 'raw', cls: humanoid });
  assert.deepEqual(codes(empty), ['empty-mesh']);
  assert.equal(empty.route, 'reroll');
  assert.ok(MESH_REMEDIES.reroll.includes('degenerate-bbox'));
});
