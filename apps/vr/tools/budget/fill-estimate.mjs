// Fill estimate for the D-G4 particle proxy: blended layers per pixel, per row, beside the count row D-G4 judges.
// Run: node apps/vr/tools/budget/fill-estimate.mjs. Written against master c1b94cc (docs/research/OVERDRAW-CEILING-2026-10.md).
// Inputs are copied from Budget/BudgetFx.cpp FxRows(), BudgetStressDriver.cpp SpawnFx(), BudgetStressPlan.cpp and
// MageArenaPawn.cpp (vignette); change them here when those change.
// Model: every live sprite is a camera-facing square quad of side s at distance d, fully inside the view, drawn in full
// (additive, so no early rejection). Covered share of one eye's buffer = (s/d)^2 / VIEW, near the view centre.
// Mean blended layers per pixel over the view = sum of shares. Resolution-free; pixels follow by eye-buffer size.
const HFOV = 110 * Math.PI / 180, VFOV = 96 * Math.PI / 180; // assumption: published Quest 3 field of view
// The eye buffer is a planar projection, so pixels are uniform in tangent space: view area = (2 tan h/2)(2 tan v/2).
const VIEW = (2 * Math.tan(HFOV / 2)) * (2 * Math.tan(VFOV / 2));
const es2 = (a, b) => (b ** 3 - a ** 3) / (3 * (b - a || 1)) || a * a; // E[s^2] for s ~ U(a,b), cm^2
const m2 = (cm2) => cm2 / 1e4;

// Positions relative to the eye (pad top + 1.2 m). Forward 0.6 m, up -0.1 m for the palm point (pad +0.6 fwd, +1.1 up).
const dPalm = Math.hypot(0.6, 0.1);
const dMotes = Math.hypot(2.5, 0.1);
const dSigil = 0.45; // assumption: fingertip during a stroke, 0.35-0.55 m from the eye
const near = 8, far = 15; // spawn-marker ring radii from BUDGET-DG4 (near 8 m, far 15 m)
const dTele = Math.hypot(far, 1.2 - 1.0);

// Bolt trail: projectile 0, near-ring lane, ends at fwd 0.6, right -0.7, chest 1.35 (eye 1.2) => 0.934 m.
const dBoltEnd = Math.hypot(0.6, 0.7, 0.15);
const speed = 12, life = 0.5;
function boltShare(endDist) {
  // 300 sprites, ages uniform over [0, life], at distance endDist + speed*age along the lane, at the arrival moment.
  const n = 300, steps = 2000; let sum = 0;
  for (let i = 0; i < steps; i++) { const age = (i + 0.5) / steps * life; const d = endDist + speed * age; sum += m2(es2(4, 8)) / (d * d); }
  return n * sum / steps / VIEW;
}
function boltMeanShare() {
  // Time-average over a whole flight from ~7.5 m to the end (trail ahead of the projectile is behind it in time).
  const lane = near - 0.5 - dBoltEnd; const T = lane / speed; const steps = 400; let s = 0;
  for (let i = 0; i < steps; i++) { const t = (i + 0.5) / steps * T; const head = dBoltEnd + (lane - speed * t); s += boltShare(head); }
  return s / steps;
}

const rows = [
  { name: 'BoltTrail (arrival)', live: 300, share: boltShare(dBoltEnd), d: dBoltEnd },
  { name: 'BoltTrail (flight mean)', live: 300, share: boltMeanShare(), d: NaN, alt: true },
  { name: 'SigilTrail (ribbon)', live: 90, share: m2(1) * (0.30 * 100) / (dSigil * dSigil) / VIEW, d: dSigil }, // 1 cm x 30 cm stroke
  { name: 'WardAbsorb', live: 120, share: 120 * m2(es2(4, 8)) / dPalm ** 2 / VIEW, d: dPalm },
  { name: 'PerfectFlash', live: 60, share: 60 * m2(es2(4, 8)) / dPalm ** 2 / VIEW, d: dPalm },
  { name: 'Telegraph', live: 300, share: 300 * m2(es2(4, 8)) / dTele ** 2 / VIEW, d: dTele },
  { name: 'Motes', live: 600, share: 600 * m2(es2(1, 2)) / dMotes ** 2 / VIEW, d: dMotes },
];
const main = rows.filter((r) => !r.alt);
const totLive = main.reduce((a, r) => a + r.live, 0), totShare = main.reduce((a, r) => a + r.share, 0);
console.log('instrument check: 1 m quad at 1 m covers', (1 / VIEW).toFixed(3), 'of the view (expect ~0.158)');
console.log('row'.padEnd(26), 'dist_m'.padStart(7), 'live'.padStart(5), 'count%'.padStart(7), 'layers'.padStart(8), 'fill%'.padStart(7));
for (const r of rows) {
  console.log(r.name.padEnd(26), (isNaN(r.d) ? '-' : r.d.toFixed(2)).padStart(7), String(r.live).padStart(5),
    (100 * r.live / totLive).toFixed(1).padStart(7), r.share.toFixed(4).padStart(8), r.alt ? '      -' : (100 * r.share / totShare).toFixed(1).padStart(7));
}
console.log('total live', totLive, 'mean layers over the view at bolt arrival', totShare.toFixed(3));
// Peak over the gaze region: ward + flash sprites inside a disc of radius R at the palm point.
for (const R of [0.15, 0.3]) {
  const layers = (180 * m2(es2(4, 8))) / (Math.PI * R * R);
  console.log(`palm cluster peak layers, R=${R} m:`, layers.toFixed(1));
}
// Count-bounded worst case: all 2000 allowed particles as 8 cm sprites at the palm distance.
console.log('2000 x 8 cm at palm distance, mean layers:', (2000 * m2(64) / dPalm ** 2 / VIEW).toFixed(2));
// Same 1,470 particles, distances swapped: telegraph's 300 at the palm, ward's 120 at the far ring.
const swapped = totShare - rows[5].share - rows[3].share + 300 * m2(es2(4, 8)) / dPalm ** 2 / VIEW + 120 * m2(es2(4, 8)) / dTele ** 2 / VIEW;
console.log('same count, telegraph and ward swapped, mean layers:', swapped.toFixed(3));
// Blink vignette fully closed: four non-overlapping quads outside a 15 deg half-angle square (MageArenaPawn.cpp).
const win = 2 * Math.tan(15 * Math.PI / 180);
console.log('closed vignette share of the eye buffer:', (1 - (win * win) / VIEW).toFixed(3));
