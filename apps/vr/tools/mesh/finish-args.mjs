// The finishing parameters finish.mjs passes to ../assets/finish.py, apart so they can be tested without Blender
// (finish-args.test.mjs). finish.py's own checks (../mesh/finish_lib.py parse_options) stay the authority; these fail
// early with the same rules so a bad flag never costs a Blender start.
//
// mode bake (default since DECISIONS 2026-10-24, Athena 17:56Z: the pipeline default for characters and creatures):
// the TRELLIS albedo baked onto LOD0 as its texture, at budgets.finish.bakeTextureRes unless --bake says otherwise;
// --clamp none|posterize|hue|both (with --strength, --levels) pulls the texture toward the palette; --cage, --ray,
// --margin tune the bake; --normal on also bakes a tangent-space normal map (T_<name>_Normal.png) onto the same UVs.
// mode flat (an option): the T38 recipe, one flat palette colour per material. Its argument list is exactly the one
// finish.mjs built before the bake mode existed: 7 positionals and the palette set, no options.

export const MODES = ['flat', 'bake'];
export const CLAMPS = ['none', 'posterize', 'hue', 'both'];
export const NORMALS = ['off', 'on'];
const BAKE_OPTIONS = ['clamp', 'strength', 'levels', 'cage', 'ray', 'margin', 'normal'];
const OFF = { clamp: 'none', normal: 'off' }; // an option at its off value is left out of the list

/** opt(name, default) reads a --flag; budgets is budgets.json; cls its class. Throws on a bad mode or clamp. */
export function finishParams(opt, budgets, cls) {
  const f = budgets.finish;
  const mode = opt('mode', 'bake');
  if (!MODES.includes(mode)) throw new Error(`--mode ${mode}: one of ${MODES.join(', ')}`);
  const clamp = opt('clamp', 'none');
  if (!CLAMPS.includes(clamp)) throw new Error(`--clamp ${clamp}: one of ${CLAMPS.join(', ')}`);
  const normal = opt('normal', 'off');
  if (!NORMALS.includes(normal)) throw new Error(`--normal ${normal}: one of ${NORMALS.join(', ')}`);
  const extra = Object.fromEntries(BAKE_OPTIONS.map((k) => [k, opt(k)]).filter(([k, v]) => v !== undefined && v !== OFF[k]));
  if (mode === 'flat' && Object.keys(extra).length) throw new Error(`--${Object.keys(extra).join(', --')} need --mode bake`);
  const bakeDefault = mode === 'bake' ? f.bakeTextureRes : f.bakeRes;
  return {
    palette: opt('palette', cls.palette),
    rotZDeg: Number(opt('rot', f.rotZDeg)),
    bakeRes: Number(opt('bake', bakeDefault)),
    voxelM: Number(opt('voxel', f.voxelM)),
    trisTarget: cls.tris.target,
    mode,
    options: extra,
  };
}

/** The argument list after `--` for finish.py. */
export function finishPyArgs(raw, outdir, name, p) {
  const args = [raw, outdir, name, String(p.rotZDeg), String(p.bakeRes), String(p.voxelM), String(p.trisTarget), p.palette];
  if (p.mode === 'flat') return args;
  return [...args, 'mode=bake', ...Object.entries(p.options).map(([k, v]) => `${k}=${v}`)];
}

// The geometry a re-finish must keep (--expect <reference finish dir>): a normal or colour pass changes what the surface
// carries, never the mesh, so the finished card's numbers and the section count must match the reference run's exactly.
const GEOMETRY_KEYS = ['tris', 'verts', 'edges', 'components', 'watertight', 'heightM', 'minZ', 'bboxMin', 'bboxMax', 'areaM2'];

/** The fields that differ between the reference and the new run: [{ key, expected, actual }]; empty when none drift. */
export function geometryDrift(refCard, refFinish, card, finish) {
  const same = (a, b) => JSON.stringify(a) === JSON.stringify(b);
  const drift = GEOMETRY_KEYS.filter((k) => !same(refCard.metrics[k], card.metrics[k])).map((k) => ({ key: k, expected: refCard.metrics[k], actual: card.metrics[k] }));
  const sections = (f) => Object.keys(f.materials ?? {}).length;
  if (sections(refFinish) !== sections(finish)) drift.push({ key: 'sections', expected: sections(refFinish), actual: sections(finish) });
  for (const k of ['tris_final', 'verts_final']) if (refFinish[k] !== finish[k]) drift.push({ key: k, expected: refFinish[k], actual: finish[k] });
  return drift;
}
