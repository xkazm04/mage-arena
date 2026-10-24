// Pure scoring for the mesh lane: numbers in, verdict out. No file, no process, no network; the Blender extractors
// (image_stats.py, mesh_metrics.py) measure, this module decides, so the rules are testable with `node --test`.
// Contract (ai-registry game-production: image-to-3d-input-gating, generated-mesh-acceptance):
// - every finding carries a code from a closed vocabulary, a severity and one human line; nothing branches on the line;
// - a criterion is measured, unmeasured or not applicable, and an unmeasured criterion keeps the verdict off pass;
// - ambiguity (a truncated histogram, an undeclared stage) resolves toward the harsher verdict;
// - a pass is never a quality claim, and every report says so.

export const INPUT_NOTICE =
  'A pass is not a quality claim. This gate checks mechanically whether an image can be reconstructed (isolation, ' +
  'framing, resolution, view-set completeness); it does not judge the design, the pose or the art.';
export const MESH_NOTICE =
  'Structural only. A pass says the geometry is well-formed and inside its class budget; it is not a quality claim ' +
  'and says nothing about whether the shape is the right one.';

// ---------------------------------------------------------------- input gate

export const IMAGE_DEFAULTS = {
  minForegroundShare: 0.005, // below this the image holds no subject
  subjectRatio: 0.05, // a component at least this share of the largest one counts as a subject
  borderNearShare: 0.98, // share of border pixels that must sit near the background colour
  maxSideSpread: 0.04, // largest distance between the four border sides' median colours (gradient, vignette)
  darkLuminance: 0.15, // a field darker than this hides a dark subject's silhouette
  whiteLuminance: 0.95, // a field brighter than this can be read as white material
  minMargin: 0.01, // closer to the frame edge than this (share of the side) counts as touching it: hard fail
  warnMargin: 0.03,
  minSubjectPx: 512, // the subject's long side in source pixels
  maxFramingDelta: 0.1, // view set: subject height relative to the master's
  maxBaselineDelta: 0.05, // view set: feet line, share of the frame height
  maxBackgroundDelta: 0.1, // view set: background colour distance
};

// code -> criterion, severity, remedy. Severity: fail = refuse, needs another input; prepare = refuse, fix the image
// and gate it again from scratch; warn = proceed, someone is told.
export const IMAGE_CODES = {
  'no-subject': { criterion: 'single-subject', severity: 'fail', remedy: 'new-input' },
  'multiple-subjects': { criterion: 'single-subject', severity: 'fail', remedy: 'crop-to-one-and-regate' },
  'background-not-plain': { criterion: 'plain-background', severity: 'prepare', remedy: 'remove-background-and-regate' },
  'background-extreme-value': { criterion: 'plain-background', severity: 'warn', remedy: 'prefer-transparency' },
  'subject-touches-frame': { criterion: 'subject-in-frame', severity: 'fail', remedy: 'new-input' },
  'margin-tight': { criterion: 'subject-in-frame', severity: 'warn', remedy: 'pad-the-canvas' },
  'subject-low-resolution': { criterion: 'subject-resolution', severity: 'warn', remedy: 'larger-render' },
  'view-missing': { criterion: 'view-set-completeness', severity: 'fail', remedy: 'supply-the-view' },
  'view-framing-mismatch': { criterion: 'view-set-consistency', severity: 'fail', remedy: 'reframe-or-master-only' },
  'view-background-mismatch': { criterion: 'view-set-consistency', severity: 'warn', remedy: 'remove-background' },
};

// What no pixel statistic here can decide. Each stays UNMEASURED until a judge that can see is wired in.
export const IMAGE_UNMEASURED = {
  'pose-canonicality': 'neutral symmetric frontal stance, limbs apart: needs a judge that can see',
  'silhouette-clarity': 'limbs merged into the body or each other: needs a judge that can see',
  'lighting-neutrality': 'baked shadow or rim light on the subject: needs a judge that can see',
  'detail-focus': 'focus of the features that must become geometry: only the pixel resolution is measured',
};
export const VIEW_SET_UNMEASURED = {
  'view-feature-correspondence': 'limb lengths and landmark heights across views: needs a judge that can see',
  'view-lighting-consistency': 'light direction across views: needs a judge that can see',
};

const SEVERITY_RANK = { fail: 3, prepare: 2, warn: 1 };

const finding = (codes, code, line, view) => {
  const c = codes[code];
  if (!c) throw new Error(`unknown defect code ${code}`);
  const f = { code, severity: c.severity, criterion: c.criterion, remedy: c.remedy, line };
  if (view) f.view = view;
  return f;
};

/** fail > prepare > incomplete (an unmeasured criterion) > warn > pass. */
export function inputVerdict(findings, unmeasured) {
  const worst = Math.max(0, ...findings.map((f) => SEVERITY_RANK[f.severity] ?? 3));
  if (worst === 3) return 'fail';
  if (worst === 2) return 'prepare';
  if (unmeasured.length) return 'incomplete';
  return worst === 1 ? 'warn' : 'pass';
}

const pct = (x) => `${(100 * x).toFixed(1)} %`;

/** The main subject: the largest component. Null when there is none. */
export function mainSubject(stats) {
  return stats.foreground?.components?.[0] ?? null;
}

/** Score one image's numbers (image_stats.py output). Returns criteria and findings; the verdict comes from inputVerdict. */
export function scoreImage(stats, view = 'front', opts = {}) {
  const t = { ...IMAGE_DEFAULTS, ...opts };
  const out = [];
  const criteria = [];
  const bg = stats.background;
  const comps = stats.foreground?.components ?? [];
  const main = mainSubject(stats);

  // single subject
  const subjects = main ? comps.filter((c) => c.area >= t.subjectRatio * main.area) : [];
  if (!main || stats.foreground.share < t.minForegroundShare) {
    out.push(finding(IMAGE_CODES, 'no-subject', `no subject: foreground ${pct(stats.foreground?.share ?? 0)} of the frame`, view));
  } else if (subjects.length > 1) {
    out.push(finding(IMAGE_CODES, 'multiple-subjects', `${subjects.length} separate subjects (each at least ${pct(t.subjectRatio)} of the largest); a sheet of views is several inputs`, view));
  }
  criteria.push({ id: 'single-subject', state: 'measured', view, values: { subjects: subjects.length, components: comps.length + (stats.foreground?.componentsOmitted ?? 0), foregroundShare: stats.foreground?.share } });

  // plain background
  if (bg.mode === 'opaque') {
    if (bg.borderNearShare < t.borderNearShare || bg.sideSpread > t.maxSideSpread) {
      out.push(finding(IMAGE_CODES, 'background-not-plain', `background not plain: ${pct(bg.borderNearShare)} of the border near its colour (need ${pct(t.borderNearShare)}), side spread ${bg.sideSpread} (max ${t.maxSideSpread})`, view));
    }
    if (bg.luminance < t.darkLuminance || bg.luminance > t.whiteLuminance) {
      out.push(finding(IMAGE_CODES, 'background-extreme-value', `background luminance ${bg.luminance}: plain means mid-value (${t.darkLuminance}-${t.whiteLuminance}) or transparent`, view));
    }
  }
  criteria.push({ id: 'plain-background', state: 'measured', view, values: { mode: bg.mode, srgb: bg.srgb, luminance: bg.luminance, borderNearShare: bg.borderNearShare, sideSpread: bg.sideSpread } });

  // uncropped within the frame
  if (main) {
    const [x0, y0, x1, y1] = main.bbox;
    const margins = { left: x0, top: y0, right: 1 - x1, bottom: 1 - y1 };
    const least = Math.min(...Object.values(margins));
    const side = Object.keys(margins).find((k) => margins[k] === least);
    if (least < t.minMargin) {
      out.push(finding(IMAGE_CODES, 'subject-touches-frame', `subject touches the ${side} frame edge (margin ${pct(least)}, need ${pct(t.minMargin)}): cut volume does not come back`, view));
    } else if (least < t.warnMargin) {
      out.push(finding(IMAGE_CODES, 'margin-tight', `${side} margin ${pct(least)} (under ${pct(t.warnMargin)})`, view));
    }
    criteria.push({ id: 'subject-in-frame', state: 'measured', view, values: { margins, bbox: main.bbox } });

    const px = Math.round(Math.max((x1 - x0) * stats.image.width, (y1 - y0) * stats.image.height));
    if (px < t.minSubjectPx) {
      out.push(finding(IMAGE_CODES, 'subject-low-resolution', `subject long side ${px} px (want ${t.minSubjectPx})`, view));
    }
    criteria.push({ id: 'subject-resolution', state: 'measured', view, values: { subjectLongSidePx: px } });
  } else {
    criteria.push({ id: 'subject-in-frame', state: 'not-applicable', view, reason: 'no subject' });
    criteria.push({ id: 'subject-resolution', state: 'not-applicable', view, reason: 'no subject' });
  }
  for (const [id, reason] of Object.entries(IMAGE_UNMEASURED)) criteria.push({ id, state: 'unmeasured', view, reason });
  return { criteria, findings: out };
}

/**
 * Gate one image or a view set. `views` maps a view name (front = the master) to image_stats.py output; `required`
 * lists the views the set must hold. Returns the verdict object (without file paths; the CLI adds those).
 */
export function gateInput(views, required = ['front'], opts = {}) {
  const t = { ...IMAGE_DEFAULTS, ...opts };
  const criteria = [];
  const findings = [];
  const perView = {};
  for (const [view, stats] of Object.entries(views)) {
    const r = scoreImage(stats, view, t);
    criteria.push(...r.criteria);
    findings.push(...r.findings);
    perView[view] = { verdict: inputVerdict(r.findings, r.criteria.filter((c) => c.state === 'unmeasured').map((c) => c.id)), metrics: stats };
  }

  // completeness
  const missing = required.filter((v) => !views[v]);
  for (const v of missing) findings.push(finding(IMAGE_CODES, 'view-missing', `the ${v} view is required and missing`, v));
  criteria.push({ id: 'view-set-completeness', state: 'measured', values: { required, given: Object.keys(views), missing } });

  // consistency against the master
  const given = Object.keys(views);
  const master = views.front && mainSubject(views.front);
  if (given.length < 2) {
    criteria.push({ id: 'view-set-consistency', state: 'not-applicable', reason: 'one view: the master alone' });
  } else if (!master) {
    criteria.push({ id: 'view-set-consistency', state: 'unmeasured', reason: 'no master (front) subject to compare against' });
  } else {
    const mh = master.bbox[3] - master.bbox[1];
    const values = {};
    for (const v of given.filter((x) => x !== 'front')) {
      const s = mainSubject(views[v]);
      if (!s) continue;
      const h = s.bbox[3] - s.bbox[1];
      const framing = h / mh - 1;
      const baseline = s.bbox[3] - master.bbox[3];
      const bgDist = Math.hypot(...views[v].background.srgb.map((x, i) => x - views.front.background.srgb[i]));
      values[v] = { heightVsMaster: Number(framing.toFixed(4)), baselineDelta: Number(baseline.toFixed(4)), backgroundDelta: Number(bgDist.toFixed(4)) };
      if (Math.abs(framing) > t.maxFramingDelta || Math.abs(baseline) > t.maxBaselineDelta) {
        findings.push(finding(IMAGE_CODES, 'view-framing-mismatch', `${v} view framed unlike the master: subject height ${framing >= 0 ? '+' : ''}${pct(framing)}, feet line ${baseline >= 0 ? '+' : ''}${pct(baseline)} of the frame`, v));
      }
      if (bgDist > t.maxBackgroundDelta) {
        findings.push(finding(IMAGE_CODES, 'view-background-mismatch', `${v} background differs from the master's by ${bgDist.toFixed(3)}`, v));
      }
    }
    criteria.push({ id: 'view-set-consistency', state: 'measured', values });
    for (const [id, reason] of Object.entries(VIEW_SET_UNMEASURED)) criteria.push({ id, state: 'unmeasured', reason });
  }

  findings.sort((a, b) => (SEVERITY_RANK[b.severity] ?? 3) - (SEVERITY_RANK[a.severity] ?? 3));
  const unmeasured = [...new Set(criteria.filter((c) => c.state === 'unmeasured').map((c) => c.id))];
  const verdict = inputVerdict(findings, unmeasured);
  const setOnly = findings.filter((f) => f.criterion === 'view-set-consistency' && f.severity === 'fail');
  const result = {
    gate: 'input-gate',
    notice: INPUT_NOTICE,
    verdict,
    findings,
    unmeasured,
    criteria,
    views: perView,
    required,
    thresholds: t,
  };
  if (setOnly.length && perView.front && !['fail', 'prepare'].includes(perView.front.verdict)) {
    result.fallback = 'the set is inconsistent: reconstruct from the master (front) alone';
  }
  if (verdict === 'incomplete') {
    result.ungated = `not passed: ${unmeasured.join(', ')} unmeasured. Generating from this input records it as ungated on those criteria.`;
  }
  return result;
}

// ---------------------------------------------------------------- mesh card

export const MESH_DEFAULTS = {
  speckShare: 0.005, // a component under this share of all faces is a speck (floater)
  speckMinFaces: 8, // and never promote a handful of triangles into a part on share alone
  minVerts: 4,
  minExtent: 1e-6,
  originMaxZ: 0.01, // metres: feet on z = 0 (metrics.py check 6)
  originMaxXY: 0.15, // metres: bbox centre near the origin (metrics.py check 6)
};

// remedy: finish = the finishing pass resolves it (stage raw grades it warn), reroll = a fresh generation can come back
// different, neither = needs another input or a person.
export const MESH_CODES = {
  'empty-mesh': { remedy: 'reroll' },
  'degenerate-bbox': { remedy: 'reroll' },
  floaters: { remedy: 'finish' },
  'too-many-parts': { remedy: 'neither' },
  'not-watertight': { remedy: 'finish' },
  'winding-inconsistent': { remedy: 'finish' },
  'degenerate-faces': { remedy: 'finish' },
  'tris-over-budget': { remedy: 'finish' },
  'tris-under-budget': { remedy: 'neither' },
  'scale-off': { remedy: 'finish' },
  'origin-off': { remedy: 'finish' },
};
export const MESH_REMEDIES = Object.fromEntries(
  ['finish', 'reroll', 'neither'].map((r) => [r, Object.keys(MESH_CODES).filter((c) => MESH_CODES[c].remedy === r)]),
);

/**
 * Split the per-component face histogram into parts and specks. A capped histogram (componentFacesOmitted > 0) is
 * resolved toward the harsher verdict: if the smallest kept entry is a speck, every omitted one is too; otherwise the
 * omitted ones count as parts. Returns { measured: false } when no histogram was emitted: callers must not read counts.
 */
export function splitComponents(metrics, opts = {}) {
  const t = { ...MESH_DEFAULTS, ...opts };
  const hist = metrics.componentFaces;
  if (!Array.isArray(hist)) return { measured: false };
  const floor = Math.max(t.speckMinFaces, metrics.tris * t.speckShare);
  const omitted = metrics.componentFacesOmitted ?? 0;
  let parts = hist.filter((n) => n >= floor).length;
  let specks = hist.length - parts;
  let speckFaces = hist.filter((n) => n < floor).reduce((s, n) => s + n, 0);
  let truncation = 'none';
  if (omitted) {
    const omittedFaces = metrics.tris - hist.reduce((s, n) => s + n, 0);
    if (hist.length && hist[hist.length - 1] < floor) {
      specks += omitted;
      speckFaces += omittedFaces;
      truncation = 'omitted counted as specks';
    } else {
      parts += omitted;
      truncation = 'omitted counted as parts';
    }
  }
  return { measured: true, floorFaces: Number(floor.toFixed(1)), parts, specks, speckFaces, speckFaceShare: metrics.tris ? Number((speckFaces / metrics.tris).toFixed(5)) : 0, truncation };
}

/**
 * Score mesh_metrics.py output. `stage` is raw | finished; anything else is recorded as undeclared and graded as
 * finished (the harsher reading). `cls` is a budgets.json class record (or null: then scale and budget are unmeasured).
 */
export function scoreMesh(metrics, { stage, cls = null, className = null } = {}, opts = {}) {
  const t = { ...MESH_DEFAULTS, ...opts };
  const declared = stage === 'raw' || stage === 'finished' ? stage : 'undeclared';
  const finishable = declared === 'raw' ? 'warn' : 'fail';
  const findings = [];
  const unmeasured = [];
  const add = (code, severity, line) => findings.push({ code, severity, remedy: MESH_CODES[code].remedy, line });
  const grades = {};

  if (!metrics.tris || (metrics.verts ?? 0) < t.minVerts) {
    add('empty-mesh', 'fail', `empty mesh: ${metrics.tris ?? 0} triangles, ${metrics.verts ?? 0} vertices`);
  } else {
    if (metrics.extentM.some((x) => x < t.minExtent)) add('degenerate-bbox', 'fail', `degenerate bounding box ${metrics.extentM.join(' x ')} m`);

    const split = splitComponents(metrics, t);
    grades.components = split;
    const partsMax = cls?.partsMax;
    const specksMax = cls?.specksMax;
    if (!split.measured) {
      unmeasured.push('component-split');
      if (partsMax != null && metrics.components > partsMax) add('too-many-parts', 'fail', `${metrics.components} components (no histogram: blunt count rule, max ${partsMax})`);
    } else {
      if (split.specks > 0) {
        const over = specksMax == null || split.specks > specksMax;
        add('floaters', over ? finishable : 'warn', `${split.specks} floaters under ${split.floorFaces} faces holding ${pct(split.speckFaceShare)} of the faces${specksMax == null ? '' : ` (tolerance ${specksMax})`}`);
      }
      if (partsMax == null) unmeasured.push('part-budget');
      else if (split.parts > partsMax) add('too-many-parts', 'fail', `${split.parts} substantial parts (class allows ${partsMax})`);
    }

    if (!metrics.watertight) add('not-watertight', 'warn', `not watertight: ${metrics.boundaryEdges} boundary and ${metrics.nonManifoldEdges} non-manifold edges`);
    if (metrics.windingInconsistentEdges > 0) add('winding-inconsistent', 'warn', `${metrics.windingInconsistentEdges} edges with inconsistent winding`);
    if (metrics.degenerateFaces > 0) add('degenerate-faces', 'warn', `${metrics.degenerateFaces} degenerate faces`);

    if (cls?.tris) {
      const { min, max, target } = cls.tris;
      grades.budget = { tris: metrics.tris, min, max, target, within: metrics.tris >= min && metrics.tris <= max };
      if (metrics.tris > max) add('tris-over-budget', finishable, `${metrics.tris} triangles over the ${className ?? 'class'} budget ${min}-${max}`);
      if (metrics.tris < min) add('tris-under-budget', finishable, `${metrics.tris} triangles under the ${className ?? 'class'} budget ${min}-${max}`);
    } else unmeasured.push('triangle-budget');

    if (cls?.heightM) {
      const lo = cls.heightM * (1 - cls.heightTolerance);
      const hi = cls.heightM * (1 + cls.heightTolerance);
      grades.scale = { heightM: metrics.heightM, targetM: cls.heightM, minM: Number(lo.toFixed(4)), maxM: Number(hi.toFixed(4)), within: metrics.heightM >= lo && metrics.heightM <= hi };
      if (!grades.scale.within) add('scale-off', finishable, `height ${metrics.heightM} m outside ${lo.toFixed(3)}-${hi.toFixed(3)} m (${className ?? 'class'} ${cls.heightM} m +-${100 * cls.heightTolerance} %)`);
    } else unmeasured.push('world-scale');

    const offXY = Math.max(...metrics.centreXY.map(Math.abs));
    grades.origin = { minZ: metrics.minZ, centreXY: metrics.centreXY, atFeet: Math.abs(metrics.minZ) <= t.originMaxZ && offXY <= t.originMaxXY };
    if (!grades.origin.atFeet) add('origin-off', finishable, `origin not at the feet: min z ${metrics.minZ} m, bbox centre xy ${metrics.centreXY.join(', ')} m`);
  }

  findings.sort((a, b) => (b.severity === 'fail') - (a.severity === 'fail'));
  const fails = findings.filter((f) => f.severity === 'fail').length;
  const warns = findings.length - fails;
  const verdict = fails ? 'fail' : unmeasured.length ? 'incomplete' : warns ? 'warn' : 'pass';
  const card = {
    gate: 'mesh-card',
    notice: MESH_NOTICE,
    stage: declared,
    verdict,
    score: Math.max(0, Math.min(100, 100 - 50 * fails - 15 * warns)),
    findings,
    unmeasured,
    grades,
    class: className,
    thresholds: t,
  };
  if (declared === 'undeclared') card.stageNote = 'stage undeclared: graded as finished, the harsher reading';
  const codes = new Set(findings.map((f) => f.code));
  card.route = findings.length === 0 ? 'none' : [...codes].every((c) => MESH_CODES[c].remedy === 'finish') ? 'finish' : [...codes].some((c) => MESH_CODES[c].remedy === 'reroll') ? 'reroll' : 'person';
  return card;
}

/** The class record from budgets.json, or throws naming the classes there are. */
export function classBudget(budgets, name) {
  const c = budgets.classes?.[name];
  if (!c) throw new Error(`unknown class ${name}; budgets.json has ${Object.keys(budgets.classes ?? {}).join(', ')}`);
  return c;
}

/** Process exit code for a verdict: 0 proceed (pass, warn), 1 refuse (fail, prepare), 3 incomplete (unmeasured). */
export function exitCodeFor(verdict) {
  return verdict === 'pass' || verdict === 'warn' ? 0 : verdict === 'incomplete' ? 3 : 1;
}
