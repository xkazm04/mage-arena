// Vocabulary distinctiveness audit for the sigil vocabulary (three spell lines): steps 1-5
// of the registry technique vocabulary-distinctiveness-audit, on the project's own JS ports
// of the recognizer and the stroke builder (tools/glyphs/qdollar.mjs, stroke.mjs), so no
// second copy of the matcher exists. Node built-ins only.
//
// Declared limits (the UE C++ tests are the authority, this is a model):
//  - the gesture is the first two pen-down strokes (loop, inner mark), projected on the
//    plane fitted to the whole clip; the builder's IsReady and timing gates are not applied.
//  - the cloud distance has no early abandon, so every pair yields its real minimum.
// Usage: node apps/vr/tools/vocab-audit/audit.mjs [--check | --update]
// Needs the gitignored corpus: node apps/vr/tools/clipgen/generate.mjs --corpus 30 --seed 1000
//        then node apps/vr/tools/clipgen/mousepaths.mjs
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { loadClip, projectRightIndex } from '../glyphs/stroke.mjs';
import { normalize, cloudDistance, strokesToPoints } from '../glyphs/qdollar.mjs';

const here = path.dirname(fileURLToPath(import.meta.url));
const gameRoot = path.resolve(here, '..', '..', 'Game');
const REJECT = 26000;
const CLASSES = ['sigil-line1', 'sigil-line2', 'sigil-line3'];
const SHAPE = { 'sigil-line1': 'vertical', 'sigil-line2': 'horizontal', 'sigil-line3': 'diagonal' };

function readClip(file) {
  return loadClip(file);
}

// Pen-down strokes projected on the fitted plane by the project's own port of the builder
// (tools/glyphs/stroke.mjs). Only the first maxStrokes strokes are kept (loop, inner mark).
export function strokesOf(clip, maxStrokes = 2) {
  const strokes = projectRightIndex(clip.frames);
  return strokes ? strokes.slice(0, maxStrokes) : [];
}
const flat = (strokes) => strokesToPoints(strokes);
const normalizeCloud = (pts) => normalize(pts);
const distance = cloudDistance;

const listClips = (dir, prefix) => (fs.existsSync(dir)
  ? fs.readdirSync(dir).filter((f) => f.startsWith(prefix) && f.endsWith('.jsonl')).sort().map((f) => path.join(dir, f))
  : []);
const cloudOf = (file, maxStrokes = 2) => normalizeCloud(flat(strokesOf(readClip(file), maxStrokes)));
const sorted = (v) => v.slice().sort((x, y) => x - y);

export function runAudit() {
  const report = {};
  const tdir = path.join(gameRoot, 'Clips', 'templates');
  const templates = [];
  for (const c of CLASSES) {
    for (const f of listClips(tdir, `${c}.`)) {
      const n = cloudOf(f);
      if (n) templates.push({ label: c, file: path.basename(f), n });
    }
  }
  report.templates = Object.fromEntries(CLASSES.map((c) => [c, templates.filter((t) => t.label === c).length]));

  // Step 1: pairwise template distance.
  const pair = {};
  for (let i = 0; i < templates.length; i++) {
    for (let j = i + 1; j < templates.length; j++) {
      const a = templates[i].label;
      const b = templates[j].label;
      const d = distance(templates[i].n, templates[j].n);
      const k = a === b ? `${SHAPE[a]}~self` : `${SHAPE[a]}|${SHAPE[b]}`;
      (pair[k] ||= []).push(d);
    }
  }
  report.pairwise = Object.fromEntries(Object.entries(pair).map(([k, v]) => {
    const s = sorted(v);
    return [k, { n: s.length, min: s[0], median: s[s.length >> 1], max: s[s.length - 1] }];
  }));

  // Step 2: prefix and containment. Every shape opens with the same loop.
  const share = {};
  const loopOnly = {};
  for (const c of CLASSES) {
    // Share of the drawn path length that is the inner mark (stroke 1), over the class's templates.
    const shares = listClips(tdir, c + ".").map((f) => {
      const [loop, mark] = strokesOf(readClip(f));
      const len = (st) => st.slice(1).reduce((a, p, i) => a + Math.hypot(p.x - st[i].x, p.y - st[i].y), 0);
      return mark ? len(mark) / (len(loop) + len(mark)) : 0;
    });
    share[SHAPE[c]] = shares.reduce((a, b) => a + b, 0) / shares.length;
    const loops = listClips(tdir, `${c}.`).map((f) => normalizeCloud(flat(strokesOf(readClip(f), 1)))).filter(Boolean);
    const all = loops.flatMap((l) => templates.map((t) => ({ label: t.label, d: distance(l, t.n) })));
    loopOnly[SHAPE[c]] = {
      toOwn: Math.min(...all.filter((x) => x.label === c).map((x) => x.d)),
      toNearestOther: Math.min(...all.filter((x) => x.label !== c).map((x) => x.d)),
    };
  }
  report.innerMarkPathShare = share;
  report.loopOnlyDistance = loopOnly;

  // Step 3: quarter-turn relation.
  const rot = {};
  for (const c of CLASSES) {
    for (const f of listClips(tdir, `${c}.`).slice(0, 4)) {
      const n = normalizeCloud(flat(strokesOf(readClip(f))).map((p) => ({ x: -p.y, y: p.x, strokeId: p.strokeId })));
      for (const o of CLASSES) {
        if (o === c) continue;
        const d = Math.min(...templates.filter((t) => t.label === o).map((t) => distance(n, t.n)));
        (rot[`${SHAPE[c]}+90 -> ${SHAPE[o]}`] ||= []).push(d);
      }
    }
  }
  report.quarterTurnMin = Object.fromEntries(Object.entries(rot).map(([k, v]) => [k, Math.min(...v)]));

  // Step 4: confusion matrix on the synthetic corpus, rejection as a column.
  const cols = [...CLASSES, 'reject', 'nogesture'];
  const matrix = Object.fromEntries(CLASSES.map((c) => [c, Object.fromEntries(cols.map((x) => [x, 0]))]));
  const margins = [];
  for (const c of CLASSES) {
    for (const f of listClips(path.join(gameRoot, 'TestClips', 'corpus'), `${c}.`)) {
      const n = cloudOf(f);
      if (!n) {
        matrix[c].nogesture++;
        continue;
      }
      const per = Object.fromEntries(CLASSES.map((o) => [o, Math.min(...templates.filter((t) => t.label === o).map((t) => distance(n, t.n)))]));
      const ranked = Object.entries(per).sort((x, y) => x[1] - y[1]);
      matrix[c][ranked[0][1] > REJECT ? 'reject' : ranked[0][0]]++;
      const wrong = Math.min(...CLASSES.filter((o) => o !== c).map((o) => per[o]));
      margins.push({ c, right: per[c], wrong });
    }
  }
  report.matrix = matrix;

  // Step 4b: the same matrix cut by source, variant and class. The project's accuracy gate
  // pools the three classes inside a variant, so one class can fall under the bar unseen.
  const cells = {};
  for (const dir of ['corpus', 'mouse']) {
    for (const f of listClips(path.join(gameRoot, 'TestClips', dir), 'sigil-line')) {
      const clip = readClip(f);
      const c = clip.header.action;
      if (!CLASSES.includes(c)) continue;
      const n = normalizeCloud(flat(strokesOf(clip)));
      const key = `${clip.header.source === 'mouse' ? 'mouse' : 'synthetic'}/${clip.header.variant}/${SHAPE[c]}`;
      const cell = (cells[key] ||= { n: 0, ok: 0, to: {} });
      cell.n++;
      let got = 'nogesture';
      if (n) {
        const per = CLASSES.map((o) => [o, Math.min(...templates.filter((t) => t.label === o).map((t) => distance(n, t.n)))]).sort((x, y) => x[1] - y[1]);
        got = per[0][1] > REJECT ? 'reject' : SHAPE[per[0][0]];
      }
      if (got === SHAPE[c]) cell.ok++;
      else cell.to[got] = (cell.to[got] || 0) + 1;
    }
  }
  report.cells = Object.fromEntries(Object.entries(cells).sort());
  report.corpusMargin = Object.fromEntries(CLASSES.map((c) => {
    const m = margins.filter((x) => x.c === c);
    return [SHAPE[c], {
      n: m.length,
      worstRight: Math.max(...m.map((x) => x.right)),
      nearestWrongMin: Math.min(...m.map((x) => x.wrong)),
      minRatio: Math.min(...m.map((x) => x.wrong / Math.max(x.right, 1))),
    }];
  }));

  // Step 5: negative proximity.
  const idir = path.join(gameRoot, 'TestClips', 'impostors');
  const neg = {};
  for (const f of fs.existsSync(idir) ? fs.readdirSync(idir) : []) {
    const n = normalizeCloud(flat(strokesOf(readClip(path.join(idir, f)), 2)));
    if (!n) continue;
    (neg[f.split('.')[0]] ||= []).push(Math.min(...templates.map((t) => distance(n, t.n))));
  }
  report.negativeNearest = Object.fromEntries(Object.entries(neg).map(([k, v]) => [k, {
    n: v.length, min: Math.min(...v), belowReject: v.filter((d) => d <= REJECT).length,
  }]));
  return report;
}

// The accuracy gate's own bars, applied per class instead of pooled: normal and slow 0.95,
// sloppy 0.90, mouse 0.90 (Tests/SigilTests.cpp, MageArena.Sigils.Accuracy).
const BAR = { normal: 0.95, slow: 0.95, sloppy: 0.90 };
export function barOf(cellKey) {
  const [source, variant] = cellKey.split('/');
  return source === 'mouse' ? 0.90 : BAR[variant];
}

// --check is a ratchet, not a bar: HEAD has a class under its bar (see the notes it prints),
// so a bar gate would be red on arrival. A cell may not lose clips against the baseline, and
// a cell that gained clips must be re-baselined with --update so the gain is recorded.
if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const report = runAudit();
  const baseFile = path.join(here, 'vocab-audit.baseline.json');
  const mode = process.argv[2];
  if (mode === '--update') {
    const cells = Object.fromEntries(Object.entries(report.cells).map(([k, v]) => [k, { n: v.n, ok: v.ok }]));
    fs.writeFileSync(baseFile, `${JSON.stringify({ cells }, null, 2)}\n`);
    process.stdout.write(`wrote ${baseFile}\n`);
  } else if (mode === '--check') {
    const base = JSON.parse(fs.readFileSync(baseFile, 'utf8')).cells;
    let bad = 0;
    for (const [k, v] of Object.entries(report.cells)) {
      const b = base[k];
      if (!b) { process.stdout.write(`NEW  ${k} ${v.ok}/${v.n}\n`); bad++; }
      else if (v.ok < b.ok) { process.stdout.write(`DROP ${k} ${v.ok}/${v.n} was ${b.ok}/${b.n}\n`); bad++; }
      else if (v.ok > b.ok) { process.stdout.write(`GAIN ${k} ${v.ok}/${v.n} was ${b.ok}/${b.n}: run --update\n`); bad++; }
      else if (v.ok / v.n < barOf(k)) process.stdout.write(`note ${k} ${v.ok}/${v.n} under its bar ${barOf(k)}: known deviation ${JSON.stringify(v.to)}\n`);
    }
    process.stdout.write(bad ? `vocab-audit: ${bad} cell(s) moved\n` : 'vocab-audit: OK\n');
    process.exit(bad ? 1 : 0);
  } else {
    process.stdout.write(`${JSON.stringify(report, null, 2)}\n`);
  }
}
